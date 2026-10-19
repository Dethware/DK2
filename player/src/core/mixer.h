#pragma once

#include "layer.h"

#include <QAudioFormat>
#include <QIODevice>
#include <QList>
#include <QMutex>

#include <atomic>
#include <memory>

namespace dk2 {

// Mixes the layers and produces audio in the output format for a QAudioSink in pull mode.
// The layers render at Layer::SampleRate stereo; other output rates are resampled linearly.
class Mixer : public QIODevice {
    Q_OBJECT
public:
    explicit Mixer(QObject *parent = nullptr);

    void setOutputFormat(const QAudioFormat &format);
    QAudioFormat outputFormat() const { return m_format; }

    void setLayers(const QList<std::shared_ptr<Layer>> &layers);
    void setMasterGain(float gain) { m_masterGain.store(gain, std::memory_order_relaxed); }

    // Render stereo frames at Layer::SampleRate, used by readData() and for offline rendering
    void renderFrames(float *out, int frames);

    bool isSequential() const override { return true; }
    qint64 bytesAvailable() const override;

protected:
    qint64 readData(char *data, qint64 maxSize) override;
    qint64 writeData(const char *, qint64) override { return -1; }

private:
    void writeFrame(char *dst, float left, float right) const;

    QAudioFormat m_format;
    QMutex m_mutex;
    QList<std::shared_ptr<Layer>> m_layers;
    std::atomic<float> m_masterGain{1.0f};

    // Resampler state: rendered source frames not consumed yet, position within them
    QList<float> m_source;
    double m_phase = 0.0;
};

} // namespace dk2
