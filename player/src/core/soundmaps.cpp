#include "soundmaps.h"

#include <QFile>
#include <QtEndian>

#include <algorithm>

namespace dk2 {

namespace {

// Both map files start with a 16 byte GUID, two unused u32 and the entry count
constexpr qsizetype CountOffset = 0x18;
constexpr qsizetype FirstRecordOffset = 0x1c;
constexpr qsizetype BankJunkSize = 11;  // per archive, runtime data
constexpr qsizetype SfxEntrySize = 24;
constexpr qsizetype SfxGroupSize = 20;
constexpr qsizetype SfxItemSize = 42;

// Bounds checked little endian reader
class Reader {
public:
    explicit Reader(const QByteArray &data) : m_data(data) {}

    bool ok() const { return m_ok; }
    qsizetype pos() const { return m_pos; }
    void seek(qsizetype pos) { m_pos = pos; }

    template<typename T>
    T read()
    {
        if (m_pos < 0 || m_pos + qsizetype(sizeof(T)) > m_data.size()) {
            m_ok = false;
            m_pos = m_data.size();
            return T{};
        }
        T value = qFromLittleEndian<T>(m_data.constData() + m_pos);
        m_pos += sizeof(T);
        return value;
    }

    QByteArray bytes(qsizetype n)
    {
        if (n < 0 || m_pos + n > m_data.size()) {
            m_ok = false;
            m_pos = m_data.size();
            return {};
        }
        QByteArray b = m_data.mid(m_pos, n);
        m_pos += n;
        return b;
    }

private:
    const QByteArray &m_data;
    qsizetype m_pos = 0;
    bool m_ok = true;
};

bool readFile(const QString &path, QByteArray *data, QString *error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error)
            *error = file.errorString();
        return false;
    }
    *data = file.readAll();
    return true;
}

} // namespace

bool SoundGroup::isLinked() const
{
    return std::any_of(items.cbegin(), items.cend(), [](const SoundItem &item) { return !item.links.isEmpty(); });
}

QStringList readBankMap(const QString &path, QString *error)
{
    QByteArray data;
    if (!readFile(path, &data, error))
        return {};

    Reader r(data);
    r.seek(CountOffset);
    const quint32 count = r.read<quint32>();
    r.seek(FirstRecordOffset + qsizetype(count) * BankJunkSize);

    QStringList names;
    for (quint32 i = 0; i < count && r.ok(); ++i) {
        const quint32 length = r.read<quint32>();
        const QByteArray name = r.bytes(length);
        names.append(QString::fromLatin1(name.constData(), qstrnlen(name.constData(), name.size())));
    }
    if (!r.ok()) {
        if (error)
            *error = QStringLiteral("truncated bank map");
        return {};
    }
    return names;
}

QList<SoundGroup> readSfxMap(const QString &path, QString *error)
{
    QByteArray data;
    if (!readFile(path, &data, error))
        return {};

    Reader r(data);
    r.seek(CountOffset);
    const quint32 entryCount = r.read<quint32>();

    // Entries only carry group counts and 3D distance parameters
    QList<quint32> groupCounts;
    for (quint32 i = 0; i < entryCount && r.ok(); ++i) {
        groupCounts.append(r.read<quint32>());
        r.seek(r.pos() + SfxEntrySize - 4);
    }

    QList<SoundGroup> groups;
    QList<quint32> itemCounts;
    for (quint32 count : groupCounts) {
        for (quint32 i = 0; i < count && r.ok(); ++i) {
            SoundGroup group;
            group.id = int(r.read<quint32>());
            itemCounts.append(r.read<quint32>());
            r.read<quint32>(); // items pointer at runtime
            r.read<quint32>(); // priority?
            group.flags = int(r.read<quint32>() & 0xffff);
            groups.append(group);
        }
    }

    for (int g = 0; g < groups.size() && r.ok(); ++g) {
        SoundGroup &group = groups[g];
        QList<QPair<quint32, quint32>> counts; // sounds, links per item
        for (quint32 i = 0; i < itemCounts.at(g) && r.ok(); ++i) {
            const qsizetype start = r.pos();
            const quint32 soundCount = r.read<quint32>();
            const quint32 linkCount = r.read<quint32>();
            r.read<quint32>(); // sounds pointer at runtime
            SoundItem item;
            item.minVolume = r.read<quint8>();
            item.maxVolume = r.read<quint8>();
            r.read<quint8>(); // pan min?
            r.read<quint8>(); // pan max?
            item.minDelayMs = r.read<quint16>();
            item.maxDelayMs = r.read<quint16>();
            r.seek(start + SfxItemSize);
            group.items.append(item);
            counts.append({soundCount, linkCount});
        }
        for (qsizetype i = 0; i < group.items.size() && r.ok(); ++i) {
            SoundItem &item = group.items[i];
            for (quint32 s = 0; s < counts.at(i).first && r.ok(); ++s) {
                SoundRef sound;
                sound.index = int(r.read<quint32>()) - 1;
                sound.weight = int(r.read<quint32>() & 0xffff);
                sound.durationMs = int(r.read<quint32>());
                sound.bank = int(r.read<quint32>()) - 1;
                item.sounds.append(sound);
            }
            for (quint32 l = 0; l < counts.at(i).second && r.ok(); ++l) {
                SoundLink link;
                link.target = int(r.read<quint32>()) - 1;
                r.read<quint8>();
                r.read<quint8>();
                link.minIntensity = r.read<quint8>();
                link.maxIntensity = r.read<quint8>();
                item.links.append(link);
            }
        }
    }

    if (!r.ok() || r.pos() != data.size()) {
        if (error)
            *error = QStringLiteral("malformed sfx map");
        return {};
    }
    return groups;
}

} // namespace dk2
