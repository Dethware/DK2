#include "sequencer.h"

#include <algorithm>

namespace dk2 {

Sequencer::Sequencer(const QList<SoundItem> &items, quint32 seed)
    : m_items(items), m_random(seed)
{
    m_linked = std::any_of(items.cbegin(), items.cend(), [](const SoundItem &item) { return !item.links.isEmpty(); });
}

int Sequencer::next(int intensity)
{
    if (m_items.isEmpty())
        return -1;
    if (!m_linked) {
        m_current = randomBelow(int(m_items.size()));
        return m_current;
    }

    const QList<SoundLink> &links = m_items.at(m_current < 0 ? 0 : m_current).links;
    const auto matching = std::count_if(links.cbegin(), links.cend(),
                                        [intensity](const SoundLink &link) { return link.matches(intensity); });
    if (matching == 0) {
        m_current = -1;
        return -1;
    }

    int pick = randomBelow(int(matching));
    for (const SoundLink &link : links) {
        if (link.matches(intensity) && pick-- == 0) {
            m_current = link.target;
            break;
        }
    }
    if (m_current < 0 || m_current >= m_items.size())
        m_current = -1;
    return m_current;
}

int Sequencer::pickSound(const SoundItem &item)
{
    if (item.sounds.isEmpty())
        return -1;
    const int r = randomBelow(0x10000);
    for (qsizetype i = 0; i < item.sounds.size(); ++i) {
        if (r <= item.sounds.at(i).weight)
            return int(i);
    }
    return 0;
}

float Sequencer::pickVolume(const SoundItem &item)
{
    return randomInRange(item.minVolume, item.maxVolume) / 100.0f;
}

int Sequencer::pickStartDelayMs(const SoundItem &item)
{
    return StartDelayPaddingMs + randomInRange(item.minDelayMs, item.maxDelayMs);
}

QList<int> Sequencer::candidates() const
{
    QList<int> result;
    if (!m_linked) {
        for (int i = 0; i < m_items.size(); ++i)
            result.append(i);
        return result;
    }
    // Where the current item may go, and where a stopped sequence resumes
    for (int from : {m_current, 0}) {
        if (from < 0 || from >= m_items.size())
            continue;
        for (const SoundLink &link : m_items.at(from).links) {
            if (!result.contains(link.target))
                result.append(link.target);
        }
    }
    return result;
}

int Sequencer::randomBelow(int bound)
{
    return std::uniform_int_distribution<int>(0, bound - 1)(m_random);
}

int Sequencer::randomInRange(int min, int max)
{
    return max <= min ? min : min + randomBelow(max - min);
}

} // namespace dk2
