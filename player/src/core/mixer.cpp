#include "mixer.h"

#include <QMutexLocker>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace dk2 {

Mixer::Mixer(QObject *parent)
    : QIODevice(parent)
{
}

void Mixer::setOutputFormat(const QAudioFormat &format)
{
    QMutexLocker lock(&m_mutex);
    m_format = format;
    m_source.clear();
    m_phase = 0.0;
}

void Mixer::setLayers(const QList<std::shared_ptr<Layer>> &layers)
{
    QMutexLocker lock(&m_mutex);
    m_layers = layers;
}

qint64 Mixer::bytesAvailable() const
{
    // An endless stream
    return std::max<qint64>(m_format.bytesForDuration(100000), 4096) + QIODevice::bytesAvailable();
}

void Mixer::renderFrames(float *out, int frames)
{
    std::fill_n(out, qsizetype(frames) * 2, 0.0f);
    for (const std::shared_ptr<Layer> &layer : std::as_const(m_layers))
        layer->render(out, frames);
    const float gain = m_masterGain.load(std::memory_order_relaxed);
    for (qsizetype i = 0; i < qsizetype(frames) * 2; ++i)
        out[i] *= gain;
}

qint64 Mixer::readData(char *data, qint64 maxSize)
{
    QMutexLocker lock(&m_mutex);
    const int bytesPerFrame = m_format.bytesPerFrame();
    if (!m_format.isValid() || bytesPerFrame <= 0)
        return 0;
    const qint64 frames = maxSize / bytesPerFrame;
    if (frames <= 0)
        return 0;

    const double step = double(Layer::SampleRate) / m_format.sampleRate();
    for (qint64 i = 0; i < frames; ++i) {
        const auto index = qsizetype(m_phase);
        // Need frames index and index + 1
        const qsizetype needed = (index + 2) * 2 - m_source.size();
        if (needed > 0) {
            const int renderFrames = int(std::max<qsizetype>(needed / 2, 256));
            const qsizetype start = m_source.size();
            m_source.resize(start + qsizetype(renderFrames) * 2);
            this->renderFrames(m_source.data() + start, renderFrames);
        }
        const float frac = float(m_phase - double(index));
        const float *a = m_source.constData() + index * 2;
        writeFrame(data + i * bytesPerFrame, a[0] + (a[2] - a[0]) * frac, a[1] + (a[3] - a[1]) * frac);
        m_phase += step;
    }

    const auto consumed = qsizetype(m_phase);
    m_source.remove(0, std::min(consumed * 2, m_source.size()));
    m_phase -= double(consumed);
    return frames * bytesPerFrame;
}

void Mixer::writeFrame(char *dst, float left, float right) const
{
    const int channels = m_format.channelCount();
    const int bytesPerSample = m_format.bytesPerSample();
    for (int c = 0; c < channels; ++c) {
        float v = channels == 1 ? (left + right) * 0.5f : (c == 0 ? left : c == 1 ? right : 0.0f);
        v = std::clamp(v, -1.0f, 1.0f);
        char *p = dst + c * bytesPerSample;
        switch (m_format.sampleFormat()) {
        case QAudioFormat::Float:
            std::memcpy(p, &v, sizeof(float));
            break;
        case QAudioFormat::Int16: {
            const auto s = qint16(std::lrint(v * 32767.0f));
            std::memcpy(p, &s, sizeof(s));
            break;
        }
        case QAudioFormat::Int32: {
            const auto s = qint32(std::lrint(double(v) * 2147483647.0));
            std::memcpy(p, &s, sizeof(s));
            break;
        }
        case QAudioFormat::UInt8:
            *reinterpret_cast<quint8 *>(p) = quint8(std::lrint(v * 127.0f + 128.0f));
            break;
        default:
            std::memset(p, 0, bytesPerSample);
            break;
        }
    }
}

} // namespace dk2
