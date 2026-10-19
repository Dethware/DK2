#include "gamedata.h"

#include <QDir>
#include <QFile>
#include <QtEndian>

#include <algorithm>

namespace dk2 {

namespace {

constexpr int ThresholdVariableId = 215; // MUSIC_LEVEL_TWO_THREAT_THRESHOLD, 216 and 217 follow
const QList<int> DefaultThresholds = {100, 500, 1500};

} // namespace

QString GameData::findPath(const QString &base, const QString &relative)
{
    QString current = QDir(base).absolutePath();
    // The game data uses backslashes, e.g. "Global\Track_1_1"
    QString normalized = relative;
    normalized.replace(u'\\', u'/');
    const QStringList parts = normalized.split(u'/', Qt::SkipEmptyParts);
    for (const QString &part : parts) {
        const QDir dir(current);
        if (dir.exists(part)) {
            current = dir.filePath(part);
            continue;
        }
        const QStringList names = dir.entryList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden);
        auto it = std::find_if(names.cbegin(), names.cend(), [&part](const QString &name) {
            return name.compare(part, Qt::CaseInsensitive) == 0;
        });
        if (it == names.cend())
            return {};
        current = dir.filePath(*it);
    }
    return current;
}

bool GameData::open(const QString &gameDir, QString *error)
{
    m_gameDir = gameDir;
    m_archives.clear();
    m_archiveIds.clear();
    m_sfxDir = findPath(gameDir, QStringLiteral("Data/Sound/Sfx"));
    if (m_sfxDir.isEmpty()) {
        if (error)
            *error = QStringLiteral("%1 is not a Dungeon Keeper II folder (Data/Sound/Sfx not found)").arg(gameDir);
        return false;
    }
    return true;
}

std::optional<LoadedGroup> GameData::loadGroup(const QString &category, int groupId, QString *error)
{
    const QString bankPath = findPath(m_sfxDir, QStringLiteral("Global/%1BANK.map").arg(category));
    const QString sfxPath = findPath(m_sfxDir, QStringLiteral("Global/%1SFX.map").arg(category));
    if (bankPath.isEmpty() || sfxPath.isEmpty()) {
        if (error)
            *error = QStringLiteral("sound category %1 not found").arg(category);
        return std::nullopt;
    }

    const QStringList banks = readBankMap(bankPath, error);
    const QList<SoundGroup> groups = readSfxMap(sfxPath, error);
    for (const SoundGroup &group : groups) {
        if (group.id != groupId)
            continue;
        LoadedGroup loaded;
        loaded.group = group;
        for (const QString &bank : banks)
            loaded.archives.append(archiveId(bank));
        return loaded;
    }
    if (error && error->isEmpty())
        *error = QStringLiteral("sound group %1 not found in %2").arg(groupId).arg(category);
    return std::nullopt;
}

int GameData::archiveId(const QString &bankName)
{
    const QString key = bankName.toLower();
    if (auto it = m_archiveIds.constFind(key); it != m_archiveIds.cend())
        return it.value();

    // As in the game, prefer the HD (software mixed) variant over HW
    int id = -1;
    for (const QString &suffix : {QStringLiteral("HD.sdt"), QStringLiteral("HW.sdt")}) {
        const QString path = findPath(m_sfxDir, bankName + suffix);
        SdtArchive archive;
        if (!path.isEmpty() && archive.open(path)) {
            id = int(m_archives.size());
            m_archives.append(archive);
            break;
        }
    }
    m_archiveIds.insert(key, id);
    return id;
}

const SdtEntry *GameData::entry(const Clip &clip) const
{
    if (clip.archive < 0 || clip.archive >= m_archives.size())
        return nullptr;
    const QList<SdtEntry> &entries = m_archives.at(clip.archive).entries();
    if (clip.entry < 0 || clip.entry >= entries.size())
        return nullptr;
    return &entries.at(clip.entry);
}

QList<int> GameData::threatThresholds() const
{
    // GlobalVariables.kwd holds 16 byte records {u32 id, u32 value, u32, u32}; look for 215, 216, 217 in a row
    QFile file(findPath(m_gameDir, QStringLiteral("Data/editor/GlobalVariables.kwd")));
    if (!file.open(QIODevice::ReadOnly))
        return DefaultThresholds;
    const QByteArray data = file.readAll();
    for (qsizetype i = 0; i + 48 <= data.size(); i += 4) {
        const char *p = data.constData() + i;
        if (qFromLittleEndian<quint32>(p) == ThresholdVariableId
                && qFromLittleEndian<quint32>(p + 16) == ThresholdVariableId + 1
                && qFromLittleEndian<quint32>(p + 32) == ThresholdVariableId + 2) {
            return {int(qFromLittleEndian<quint32>(p + 4)), int(qFromLittleEndian<quint32>(p + 20)),
                    int(qFromLittleEndian<quint32>(p + 36))};
        }
    }
    return DefaultThresholds;
}

} // namespace dk2
