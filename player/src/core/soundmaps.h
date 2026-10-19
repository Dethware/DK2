#pragma once

#include <QList>
#include <QString>
#include <QStringList>

namespace dk2 {

// A possible follow-up of an item, valid while minIntensity <= intensity <= maxIntensity
struct SoundLink {
    int target = 0; // 0-based item index in the group
    int minIntensity = 0;
    int maxIntensity = 0;

    bool matches(int intensity) const { return minIntensity <= intensity && intensity <= maxIntensity; }
};

// One alternative sound of an item
struct SoundRef {
    int index = 0;      // 0-based entry index in the .sdt archive
    int bank = 0;       // 0-based index into the BANK.map archive list
    int weight = 0;     // cumulative selection weight, 0-65535
    int durationMs = 0;
};

// An item ("entry") of a sound group: what to play, and in sequenced groups what may follow
struct SoundItem {
    QList<SoundRef> sounds;
    QList<SoundLink> links;
    int minVolume = 100; // percent, the volume is picked from the range
    int maxVolume = 100;
    int minDelayMs = 0;  // start-to-start delay of delay-chained groups (one shot atmosphere)
    int maxDelayMs = 0;
};

struct SoundGroup {
    int id = 0;     // e.g. 343 music track 1, 345 music track 3, 341 ambience, 746 one shot atmosphere
    int flags = 0;  // playback class, e.g. 0x0606 linked sequence, 0x0204 delay-chained one shots
    QList<SoundItem> items;

    bool isLinked() const;
};

// <category>BANK.map: the list of archives (e.g. "Global\Track_1_1") a category uses
QStringList readBankMap(const QString &path, QString *error = nullptr);

// <category>SFX.map: the sound groups of a category
QList<SoundGroup> readSfxMap(const QString &path, QString *error = nullptr);

} // namespace dk2
