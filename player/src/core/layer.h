#pragma once

#include "clipcache.h"
#include "gamedata.h"
#include "sequencer.h"

#include <atomic>
#include <functional>
#include <memory>

namespace dk2 {

// Plays one sequenced sound group (music, ambience or one shot atmosphere) gaplessly.
// render() runs on the audio thread; the intensity function must be thread safe.
class Layer {
public:
    static constexpr int SampleRate = 22050;

    using IntensityFunction = std::function<int()>;
    // Called on the audio thread when an item starts playing
    using StartedCallback = std::function<void(int item, int sound)>;

    Layer(const LoadedGroup &group, ClipCache *cache, IntensityFunction intensity, quint32 seed);

    const LoadedGroup &group() const { return m_group; }

    void setGain(float gain) { m_gain.store(gain, std::memory_order_relaxed); }
    void setStartedCallback(StartedCallback callback) { m_started = std::move(callback); }

    // Request the clips that may play next
    void prefetch();

    // Mix `frames` stereo frames at SampleRate into `out`
    void render(float *out, int frames);

private:
    void startNext();
    void mix(float *out, int frames);

    const LoadedGroup m_group;
    ClipCache *m_cache;
    IntensityFunction m_intensity;
    Sequencer m_sequencer;
    StartedCallback m_started;
    std::atomic<float> m_gain{1.0f};

    std::shared_ptr<const Pcm> m_pcm; // playing clip
    qsizetype m_position = 0;
    float m_volume = 1.0f;
    qint64 m_silenceFrames = 0;
    qint64 m_framesToNextStart = 0;   // delay-chained groups

    int m_pendingItem = -1;           // chosen, waiting for its clip to decode
    int m_pendingSound = -1;
};

} // namespace dk2
