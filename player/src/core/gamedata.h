#pragma once

#include "sdtarchive.h"
#include "soundmaps.h"

#include <QHash>
#include <QList>
#include <QString>

#include <optional>

namespace dk2 {

// A decodable sound: entry `entry` of archive `archive` (see GameData::archive)
struct Clip {
    int archive = -1;
    int entry = -1;

    bool isValid() const { return archive >= 0 && entry >= 0; }
    friend bool operator==(const Clip &, const Clip &) = default;
};

inline size_t qHash(const Clip &clip, size_t seed = 0)
{
    return qHashMulti(seed, clip.archive, clip.entry);
}

// A sound group with its bank list resolved to the archives in GameData
struct LoadedGroup {
    SoundGroup group;
    QList<int> archives; // bank index -> GameData archive id, -1 if missing

    Clip clip(const SoundRef &sound) const { return {archives.value(sound.bank, -1), sound.index}; }
};

// Access to the sound data of a DK II installation
class GameData {
public:
    bool open(const QString &gameDir, QString *error = nullptr);

    QString gameDir() const { return m_gameDir; }

    // Loads a group of a global sound category, e.g. ("music", 343).
    // Reads Data/Sound/Sfx/Global/<category>BANK.map and <category>SFX.map
    std::optional<LoadedGroup> loadGroup(const QString &category, int groupId, QString *error = nullptr);

    const SdtArchive &archive(int id) const { return m_archives.at(id); }
    const SdtEntry *entry(const Clip &clip) const;

    // Music threat thresholds (global variables 215-217), defaults if not found
    QList<int> threatThresholds() const;

    // Case insensitive lookup of a path below `base`, empty if not found
    static QString findPath(const QString &base, const QString &relative);

private:
    int archiveId(const QString &bankName);

    QString m_gameDir;
    QString m_sfxDir;
    QList<SdtArchive> m_archives;
    QHash<QString, int> m_archiveIds; // lower case bank name -> id
};

} // namespace dk2
