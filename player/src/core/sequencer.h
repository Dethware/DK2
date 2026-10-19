#pragma once

#include "soundmaps.h"

#include <random>

namespace dk2 {

// Decides which item of a sound group plays next, as DK II does.
//
// Linked groups (music, ambience): the next item is picked uniformly among the current item's links
// that match the intensity. With no match the sequence goes silent and later resumes from the links
// of item 0, which is only an entry point and never played itself. Repeating a link weighs it more.
//
// Other groups (one shot atmosphere): a random item plays, and the next one starts
// StartDelayPaddingMs + rand(minDelay, maxDelay) after the previous one started.
class Sequencer {
public:
    static constexpr int StartDelayPaddingMs = 250;

    Sequencer(const QList<SoundItem> &items, quint32 seed);

    bool isLinked() const { return m_linked; }

    // The next item to play, or -1 to stay silent
    int next(int intensity);

    // Index of one of the item's sounds by their cumulative weights, -1 if none
    int pickSound(const SoundItem &item);
    float pickVolume(const SoundItem &item);
    int pickStartDelayMs(const SoundItem &item);

    // Items that may play next at any intensity, for preloading
    QList<int> candidates() const;

private:
    int randomBelow(int bound);
    int randomInRange(int min, int max);

    const QList<SoundItem> &m_items;
    std::mt19937 m_random;
    bool m_linked = false;
    int m_current = -1;
};

} // namespace dk2
