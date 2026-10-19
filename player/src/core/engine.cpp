#include "engine.h"

#include <QAudioDevice>
#include <QAudioSink>
#include <QFileInfo>
#include <QMediaDevices>
#include <QRandomGenerator>

namespace dk2 {

namespace {

QAudioFormat outputFormat(const QAudioDevice &device)
{
    QAudioFormat format;
    format.setSampleRate(Layer::SampleRate);
    format.setChannelCount(2);
    format.setSampleFormat(QAudioFormat::Int16);
    // The mixer resamples and converts to whatever the device prefers otherwise
    return device.isFormatSupported(format) ? format : device.preferredFormat();
}

} // namespace

Engine::Engine(QObject *parent)
    : QObject(parent), m_cache(&m_data)
{
    m_ticker.setInterval(TickMs);
    connect(&m_ticker, &QTimer::timeout, this, &Engine::tick);
    connect(&m_cache, &ClipCache::decodeFailed, this, [this](const Clip &clip, const QString &error) {
        const SdtEntry *entry = m_data.entry(clip);
        emit errorOccurred(tr("Failed to decode %1: %2").arg(entry ? entry->name : tr("a sound"), error));
    });
}

Engine::~Engine()
{
    stop();
}

bool Engine::open(const QString &gameDir)
{
    stop();
    m_cache.clear();
    QString error;
    m_open = m_data.open(gameDir, &error);
    if (!m_open) {
        emit errorOccurred(error);
        return false;
    }
    m_thresholds = m_data.threatThresholds();
    return true;
}

bool Engine::start(int musicGroup)
{
    stop();
    if (!m_open)
        return false;

    m_layers[Music] = createLayer(Music, QStringLiteral("music"), musicGroup);
    m_layers[Ambience] = createLayer(Ambience, QStringLiteral("ambience"), AmbienceGroup);
    m_layers[Atmosphere] = createLayer(Atmosphere, QStringLiteral("one_shot_atmos"), AtmosphereGroup);
    QList<std::shared_ptr<Layer>> layers;
    for (const std::shared_ptr<Layer> &layer : m_layers) {
        if (layer)
            layers.append(layer);
    }
    if (layers.isEmpty())
        return false;

    m_level.set(MusicLevel::Silent);
    m_level.update();
    m_publishedLevel = MusicLevel::Silent;
    emit levelChanged(m_publishedLevel, m_level.intensity());

    const QAudioDevice device = QMediaDevices::defaultAudioOutput();
    m_mixer.setOutputFormat(outputFormat(device));
    m_mixer.setLayers(layers);
    m_mixer.open(QIODevice::ReadOnly);
    m_sink = new QAudioSink(device, m_mixer.outputFormat(), this);
    m_sink->start(&m_mixer);
    m_ticker.start();
    return true;
}

void Engine::stop()
{
    m_ticker.stop();
    if (m_sink) {
        m_sink->stop();
        delete m_sink;
        m_sink = nullptr;
    }
    m_mixer.close();
    m_mixer.setLayers({});
    m_layers.fill(nullptr);
}

void Engine::setLayerGain(LayerId layer, float gain)
{
    m_gains[layer] = gain;
    if (m_layers[layer])
        m_layers[layer]->setGain(gain);
}

void Engine::tick()
{
    m_level.raise(MusicLevel::levelForThreat(m_threat, m_thresholds));
    const int requested = m_level.level();
    m_level.update();
    if (requested != m_publishedLevel) {
        m_publishedLevel = requested;
        emit levelChanged(requested, m_level.intensity());
    }
}

std::shared_ptr<Layer> Engine::createLayer(LayerId id, const QString &category, int group)
{
    QString error;
    const std::optional<LoadedGroup> loaded = m_data.loadGroup(category, group, &error);
    if (!loaded) {
        emit errorOccurred(error);
        return nullptr;
    }

    auto layer = std::make_shared<Layer>(*loaded, &m_cache, [this] { return m_level.intensity(); },
                                         QRandomGenerator::global()->generate());
    layer->setGain(m_gains[id]);
    layer->setStartedCallback([this, id](int item, int sound) {
        // Audio thread -> UI thread
        QMetaObject::invokeMethod(this, [this, id, item, sound] {
            emit itemStarted(id, item, describe(id, item, sound));
        }, Qt::QueuedConnection);
    });
    layer->prefetch();
    return layer;
}

QString Engine::describe(LayerId layer, int item, int sound) const
{
    const std::shared_ptr<Layer> &l = m_layers[layer];
    if (!l || item < 0 || item >= l->group().group.items.size())
        return {};
    const QList<SoundRef> &sounds = l->group().group.items.at(item).sounds;
    if (sound < 0 || sound >= sounds.size())
        return tr("item %1").arg(item + 1);
    const Clip clip = l->group().clip(sounds.at(sound));
    const SdtEntry *entry = m_data.entry(clip);
    const QString archive = clip.isValid() ? QFileInfo(m_data.archive(clip.archive).path()).completeBaseName() : QString();
    return tr("item %1: %2/%3").arg(item + 1).arg(archive, entry ? entry->name : QString());
}

} // namespace dk2
