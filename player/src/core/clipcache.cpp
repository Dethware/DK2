#include "clipcache.h"

#include <QAudioBuffer>
#include <QAudioDecoder>
#include <QBuffer>
#include <QMutexLocker>

#include <algorithm>

namespace dk2 {

namespace {

constexpr quint8 TypeMpegMono = 0x24;

void appendSamples(const QAudioBuffer &buffer, QList<float> &out)
{
    const QAudioFormat format = buffer.format();
    const qsizetype count = buffer.sampleCount();
    const qsizetype start = out.size();
    out.resize(start + count);
    float *dst = out.data() + start;
    switch (format.sampleFormat()) {
    case QAudioFormat::Float:
        std::copy_n(buffer.constData<float>(), count, dst);
        break;
    case QAudioFormat::Int16:
        std::transform(buffer.constData<qint16>(), buffer.constData<qint16>() + count, dst,
                       [](qint16 v) { return v / 32768.0f; });
        break;
    case QAudioFormat::Int32:
        std::transform(buffer.constData<qint32>(), buffer.constData<qint32>() + count, dst,
                       [](qint32 v) { return float(v / 2147483648.0); });
        break;
    case QAudioFormat::UInt8:
        std::transform(buffer.constData<quint8>(), buffer.constData<quint8>() + count, dst,
                       [](quint8 v) { return (v - 128) / 128.0f; });
        break;
    default:
        std::fill_n(dst, count, 0.0f);
        break;
    }
}

} // namespace

ClipCache::ClipCache(const GameData *data, QObject *parent)
    : QObject(parent), m_data(data)
{
}

ClipCache::~ClipCache() = default;

void ClipCache::request(const Clip &clip)
{
    {
        QMutexLocker lock(&m_mutex);
        if (!clip.isValid() || m_cache.contains(clip) || m_pending.contains(clip) || m_failed.contains(clip))
            return;
        m_pending.insert(clip);
    }
    QMetaObject::invokeMethod(this, [this, clip] { enqueue(clip); }, Qt::QueuedConnection);
}

std::shared_ptr<const Pcm> ClipCache::get(const Clip &clip)
{
    QMutexLocker lock(&m_mutex);
    auto it = m_cache.find(clip);
    if (it == m_cache.end())
        return nullptr;
    it->lastUse = ++m_useCounter;
    return it->pcm;
}

bool ClipCache::hasFailed(const Clip &clip) const
{
    QMutexLocker lock(&m_mutex);
    return m_failed.contains(clip);
}

void ClipCache::clear()
{
    QMutexLocker lock(&m_mutex);
    m_cache.clear();
    m_failed.clear();
}

void ClipCache::enqueue(const Clip &clip)
{
    m_queue.append(clip);
    startNext();
}

void ClipCache::startNext()
{
    if (m_decoder || m_queue.isEmpty())
        return;

    m_decoding = m_queue.takeFirst();
    const SdtEntry *entry = m_data->entry(m_decoding);
    const QByteArray data = entry ? m_data->archive(m_decoding.archive).read(m_decoding.entry) : QByteArray();
    if (data.isEmpty()) {
        finish(false, QStringLiteral("no data"));
        return;
    }

    m_output = std::make_shared<Pcm>();
    m_buffer = new QBuffer(this);
    m_buffer->setData(data);
    m_buffer->open(QIODevice::ReadOnly);

    QAudioFormat format;
    format.setSampleFormat(QAudioFormat::Float);
    format.setSampleRate(entry->sampleRate > 0 ? entry->sampleRate : 22050);
    format.setChannelCount(entry->type == TypeMpegMono ? 1 : 2);

    m_decoder = new QAudioDecoder(this);
    m_decoder->setAudioFormat(format);
    m_decoder->setSourceDevice(m_buffer);
    connect(m_decoder, &QAudioDecoder::bufferReady, this, [this] {
        const QAudioBuffer buffer = m_decoder->read();
        if (!buffer.isValid())
            return;
        m_output->channels = buffer.format().channelCount();
        m_output->sampleRate = buffer.format().sampleRate();
        appendSamples(buffer, m_output->samples);
    });
    connect(m_decoder, &QAudioDecoder::finished, this, [this] { finish(true); });
    connect(m_decoder, qOverload<QAudioDecoder::Error>(&QAudioDecoder::error), this,
            [this](QAudioDecoder::Error) { finish(false, m_decoder->errorString()); });
    m_decoder->start();
}

void ClipCache::finish(bool ok, const QString &error)
{
    const Clip clip = m_decoding;
    if (m_decoder) {
        m_decoder->disconnect(this);
        m_decoder->stop();
        m_decoder->deleteLater();
        m_decoder = nullptr;
    }
    if (m_buffer) {
        m_buffer->deleteLater();
        m_buffer = nullptr;
    }

    ok = ok && m_output && m_output->channels > 0;
    if (ok) {
        // The MPEG data decodes into whole frames; the game plays exactly pcmBytes of 16 bit PCM,
        // which makes consecutive music segments join seamlessly
        const SdtEntry *entry = m_data->entry(clip);
        if (entry && entry->pcmBytes > 0)
            m_output->samples.resize(qsizetype(entry->pcmBytes / 2), 0.0f);
    }

    {
        QMutexLocker lock(&m_mutex);
        m_pending.remove(clip);
        if (ok)
            m_cache.insert(clip, {m_output, ++m_useCounter});
        else
            m_failed.insert(clip);
    }
    m_output.reset();

    if (ok) {
        evict();
        emit decoded(clip);
    } else {
        emit decodeFailed(clip, error);
    }
    startNext();
}

void ClipCache::evict()
{
    QMutexLocker lock(&m_mutex);
    qsizetype total = 0;
    for (const Cached &cached : std::as_const(m_cache))
        total += cached.pcm->samples.size() * qsizetype(sizeof(float));

    while (total > m_limitBytes && m_cache.size() > 1) {
        auto oldest = std::min_element(m_cache.begin(), m_cache.end(),
                                       [](const Cached &a, const Cached &b) { return a.lastUse < b.lastUse; });
        total -= oldest->pcm->samples.size() * qsizetype(sizeof(float));
        m_cache.erase(oldest);
    }
}

} // namespace dk2
