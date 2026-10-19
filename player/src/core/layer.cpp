#include "layer.h"

#include <algorithm>

namespace dk2 {

namespace {

// How long a stopped linked sequence stays silent before trying to resume
constexpr qint64 RetryFrames = Layer::SampleRate / 10;
// How long to wait for a clip that is still decoding before checking again
constexpr qint64 WaitFrames = Layer::SampleRate / 100;

constexpr qint64 msToFrames(qint64 ms)
{
    return ms * Layer::SampleRate / 1000;
}

} // namespace

Layer::Layer(const LoadedGroup &group, ClipCache *cache, IntensityFunction intensity, quint32 seed)
    : m_group(group), m_cache(cache), m_intensity(std::move(intensity)), m_sequencer(m_group.group.items, seed)
{
}

void Layer::prefetch()
{
    const QList<SoundItem> &items = m_group.group.items;
    for (int item : m_sequencer.candidates()) {
        for (const SoundRef &sound : items.at(item).sounds)
            m_cache->request(m_group.clip(sound));
    }
}

void Layer::render(float *out, int frames)
{
    int done = 0;
    while (done < frames) {
        if (m_silenceFrames > 0) {
            const int n = int(std::min<qint64>(frames - done, m_silenceFrames));
            m_silenceFrames -= n;
            m_framesToNextStart -= n;
            done += n;
        } else if (m_pcm) {
            const int n = int(std::min<qsizetype>(frames - done, m_pcm->frames() - m_position));
            mix(out + done * 2, n);
            m_position += n;
            m_framesToNextStart -= n;
            done += n;
            if (m_position >= m_pcm->frames()) {
                m_pcm.reset();
                if (!m_sequencer.isLinked())
                    m_silenceFrames = std::max<qint64>(0, m_framesToNextStart);
            }
        } else {
            startNext();
        }
    }
}

void Layer::startNext()
{
    const QList<SoundItem> &items = m_group.group.items;
    if (m_pendingItem < 0) {
        m_pendingItem = m_sequencer.next(m_intensity());
        if (m_pendingItem < 0) {
            prefetch();
            m_silenceFrames = RetryFrames;
            return;
        }
        const SoundItem &item = items.at(m_pendingItem);
        m_pendingSound = m_sequencer.pickSound(item);
        m_volume = m_sequencer.pickVolume(item);
        m_framesToNextStart = msToFrames(m_sequencer.pickStartDelayMs(item));
    }

    const QList<SoundRef> &sounds = items.at(m_pendingItem).sounds;
    const Clip clip = m_pendingSound >= 0 ? m_group.clip(sounds.at(m_pendingSound)) : Clip();
    std::shared_ptr<const Pcm> pcm = clip.isValid() ? m_cache->get(clip) : nullptr;
    if (!pcm) {
        if (!clip.isValid() || m_cache->hasFailed(clip)) {
            // Nothing playable here, move on
            m_pendingItem = -1;
            m_silenceFrames = RetryFrames;
        } else {
            m_cache->request(clip);
            m_silenceFrames = WaitFrames;
        }
        return;
    }

    m_pcm = std::move(pcm);
    m_position = 0;
    const int item = m_pendingItem;
    m_pendingItem = -1;
    if (m_started)
        m_started(item, m_pendingSound);
    prefetch();
}

void Layer::mix(float *out, int frames)
{
    const float gain = m_volume * m_gain.load(std::memory_order_relaxed);
    const float *src = m_pcm->samples.constData() + m_position * m_pcm->channels;
    if (m_pcm->channels == 1) {
        for (int i = 0; i < frames; ++i) {
            out[i * 2] += src[i] * gain;
            out[i * 2 + 1] += src[i] * gain;
        }
    } else {
        const int channels = m_pcm->channels;
        for (int i = 0; i < frames; ++i) {
            out[i * 2] += src[i * channels] * gain;
            out[i * 2 + 1] += src[i * channels + 1] * gain;
        }
    }
}

} // namespace dk2
