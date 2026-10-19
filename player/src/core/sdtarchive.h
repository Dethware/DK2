#pragma once

#include <QByteArray>
#include <QList>
#include <QString>

namespace dk2 {

// One sound in an .sdt archive
struct SdtEntry {
    QString name;
    qint64 dataOffset = 0;
    quint32 dataSize = 0;
    quint16 sampleRate = 0;
    quint8 bitsPerSample = 0;
    quint8 type = 0;      // 0x24 MPEG mono, 0x25 MPEG stereo, 0 blank
    quint32 pcmBytes = 0; // length of the decoded 16 bit PCM; sequenced sounds are cut to this
};

// Reader for DK II .sdt sound archives:
//   u32 count, u32 offsets[count], then per entry a 40 byte header followed by the data
//   header: u32 headerSize (0x28), u32 dataSize, char name[16], u16 sampleRate, u8 bits,
//           u8 type, u32 0, u32 pcmBytes, u32 0
class SdtArchive {
public:
    bool open(const QString &path, QString *error = nullptr);

    QString path() const { return m_path; }
    const QList<SdtEntry> &entries() const { return m_entries; }

    // Raw (MPEG audio) data of an entry, empty on failure
    QByteArray read(int index) const;

private:
    QString m_path;
    QList<SdtEntry> m_entries;
};

} // namespace dk2
