#include "sdtarchive.h"

#include <QFile>
#include <QtEndian>

namespace dk2 {

namespace {

constexpr int EntryHeaderSize = 40;

template<typename T>
T readLe(const char *p)
{
    return qFromLittleEndian<T>(p);
}

} // namespace

bool SdtArchive::open(const QString &path, QString *error)
{
    m_path = path;
    m_entries.clear();

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error)
            *error = file.errorString();
        return false;
    }

    const QByteArray countBytes = file.read(4);
    if (countBytes.size() != 4) {
        if (error)
            *error = QStringLiteral("truncated header");
        return false;
    }
    const quint32 count = readLe<quint32>(countBytes.constData());
    const QByteArray offsets = file.read(qint64(count) * 4);
    if (offsets.size() != qsizetype(count) * 4) {
        if (error)
            *error = QStringLiteral("truncated offset table");
        return false;
    }

    m_entries.reserve(count);
    for (quint32 i = 0; i < count; ++i) {
        const quint32 offset = readLe<quint32>(offsets.constData() + i * 4);
        if (!file.seek(offset)) {
            if (error)
                *error = QStringLiteral("bad entry offset");
            return false;
        }
        const QByteArray h = file.read(EntryHeaderSize);
        if (h.size() != EntryHeaderSize) {
            if (error)
                *error = QStringLiteral("truncated entry header");
            return false;
        }

        SdtEntry entry;
        const quint32 headerSize = readLe<quint32>(h.constData());
        entry.dataSize = readLe<quint32>(h.constData() + 4);
        entry.name = QString::fromLatin1(h.constData() + 8, qstrnlen(h.constData() + 8, 16));
        entry.sampleRate = readLe<quint16>(h.constData() + 24);
        entry.bitsPerSample = quint8(h[26]);
        entry.type = quint8(h[27]);
        entry.pcmBytes = readLe<quint32>(h.constData() + 32);
        entry.dataOffset = qint64(offset) + headerSize;
        m_entries.append(entry);
    }
    return true;
}

QByteArray SdtArchive::read(int index) const
{
    if (index < 0 || index >= m_entries.size())
        return {};
    const SdtEntry &entry = m_entries.at(index);
    QFile file(m_path);
    if (!file.open(QIODevice::ReadOnly) || !file.seek(entry.dataOffset))
        return {};
    return file.read(entry.dataSize);
}

} // namespace dk2
