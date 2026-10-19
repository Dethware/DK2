#pragma once

#include <QList>

#include <algorithm>
#include <atomic>

namespace dk2 {

// The music level of DK II. Every game tick the game requests levels (the highest request wins),
// then publishes the level as intensity and lets anything above level one fall back to it. So a high
// level only lasts while it is requested, and after the first fight the music keeps playing quietly.
// Only set(Silent) stops it again, which the game does at level start.
class MusicLevel {
public:
    enum Level { Silent = 0, One, Two, Three, Four };
    static constexpr int IntensityPerLevel = 20;

    void raise(int level) { m_level = std::max(m_level, level); }
    void set(int level) { m_level = level; }
    int level() const { return m_level; }

    // Level requested by a battle with the given summed enemy threat (0 = no fight)
    static int levelForThreat(int threat, const QList<int> &thresholds)
    {
        if (threat <= 0)
            return Silent;
        int level = One;
        for (int threshold : thresholds) {
            if (threat >= threshold)
                ++level;
        }
        return std::min<int>(level, Four);
    }

    // Publish the requested level as intensity, then decay
    void update()
    {
        m_intensity.store(m_level * IntensityPerLevel, std::memory_order_relaxed);
        if (m_level > One)
            m_level = One;
    }

    // The intensity (0-100) the sequences use, safe to read from the audio thread
    int intensity() const { return m_intensity.load(std::memory_order_relaxed); }

private:
    int m_level = Silent;
    std::atomic<int> m_intensity{0};
};

} // namespace dk2
