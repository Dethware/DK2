#pragma once

#include "gamedata.h"

#include <QHash>
#include <QList>
#include <QMutex>
#include <QObject>
#include <QSet>

#include <memory>

class QAudioDecoder;
class QBuffer;

namespace dk2 {

// A decoded sound, interleaved float samples at the clip's sample rate
struct Pcm {
    int channels = 0;
    int sampleRate = 0;
    QList<float> samples;

    qsizetype frames() const { return channels > 0 ? samples.size() / channels : 0; }
};

// Decodes clips with QAudioDecoder and keeps them in memory.
// request() and get() may be called from any thread (the audio thread); decoding runs on the
// thread the cache lives in, which needs an event loop.
class ClipCache : public QObject {
    Q_OBJECT
public:
    explicit ClipCache(const GameData *data, QObject *parent = nullptr);
    ~ClipCache() override;

    // Queue a clip for decoding unless it is cached, queued or failed
    void request(const Clip &clip);

    // The decoded clip, or null while it is not available
    std::shared_ptr<const Pcm> get(const Clip &clip);

    // True if decoding the clip failed (it will never become available)
    bool hasFailed(const Clip &clip) const;

    void clear();

    // Memory limit for the decoded clips, least recently used ones are dropped
    void setLimitBytes(qsizetype bytes) { m_limitBytes = bytes; }

signals:
    void decoded(const dk2::Clip &clip);
    void decodeFailed(const dk2::Clip &clip, const QString &error);

private:
    void enqueue(const Clip &clip);
    void startNext();
    void finish(bool ok, const QString &error = {});
    void evict();

    struct Cached {
        std::shared_ptr<const Pcm> pcm;
        quint64 lastUse = 0;
    };

    const GameData *m_data;
    mutable QMutex m_mutex;
    QHash<Clip, Cached> m_cache;
    QSet<Clip> m_pending;  // queued or decoding
    QSet<Clip> m_failed;
    quint64 m_useCounter = 0;
    qsizetype m_limitBytes = 512 * 1024 * 1024;

    // Decoder state, cache thread only
    QList<Clip> m_queue;
    QAudioDecoder *m_decoder = nullptr;
    QBuffer *m_buffer = nullptr;
    Clip m_decoding;
    std::shared_ptr<Pcm> m_output;
};

} // namespace dk2
