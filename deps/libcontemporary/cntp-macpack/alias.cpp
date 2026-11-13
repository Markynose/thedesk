#include "alias.h"

#include "utils.h"
#include <QByteArray>
#include <QDir>
#include <QFileInfo>
#include <QStorageInfo>
#include <QStringEncoder>
#include <sys/stat.h>

#include <CoreFoundation/CFString.h>
#include <CoreFoundation/CFURL.h>

struct AliasPrivate {
        const int version = 2;

        QString volumeName;
        struct timespec volumeCreated;

        quint32 parentInode;

        quint32 targetInode;
        QString targetFilename;
        struct timespec targetCreated;
        const int targetType = 0; // file

        QList<QPair<int, QByteArray>> extras;
};

Alias::Alias(QObject* parent) :
    QObject{parent} {
    d = new AliasPrivate();
}

Alias::~Alias() {
    delete d;
}

QByteArray Alias::aliasFor(QString path) {
    QStringEncoder converter(QStringEncoder::Utf16BE);
    Alias alias;

    auto parentDir = QFileInfo(path).dir();
    auto volumePath = QStorageInfo(path).rootPath();
    struct stat targetStat, parentStat, volumeStat;
    stat(path.toLatin1().data(), &targetStat);
    stat(parentDir.absolutePath().toLatin1().data(), &parentStat);
    stat(volumePath.toLatin1().data(), &volumeStat);

    alias.d->volumeName = volumeName(volumePath);
    alias.d->volumeCreated = volumeStat.st_ctimespec;

    alias.d->parentInode = parentStat.st_ino;

    alias.d->targetInode = targetStat.st_ino;
    alias.d->targetFilename = QFileInfo(path).fileName();
    alias.d->targetCreated = targetStat.st_ctimespec;

    alias.d->extras.append({0, QFileInfo(parentDir.absolutePath()).fileName().toUtf8()});
    alias.d->extras.append({1, Utils::to32BigEndian(alias.d->parentInode)});

    auto targetFilenameUtf16 = QByteArray(converter.encode(alias.d->targetFilename));
    alias.d->extras.append({14, Utils::to16BigEndian(alias.d->targetFilename.length()).append(targetFilenameUtf16)});

    auto volumeNameUtf16 = QByteArray(converter.encode(alias.d->volumeName));
    alias.d->extras.append({15, Utils::to16BigEndian(alias.d->volumeName.length()).append(volumeNameUtf16)});

    alias.d->extras.append({18, path.mid(volumePath.length()).toUtf8()});
    alias.d->extras.append({19, volumePath.toUtf8()});

    return alias.data();
}

QByteArray Alias::data() {
    auto baseLength = 150;
    auto extraLength = 0;
    for (const auto& extra : d->extras) {
        auto padding = extra.second.length() % 2;
        extraLength += 4 + extra.second.length() + padding;
    }
    auto trailerLength = 4;

    QBuffer buf;
    buf.open(QBuffer::WriteOnly);

    const quint64 hfsEpoch = 2082844800;

    buf.write(Utils::to32BigEndian(0));
    buf.write(Utils::to16BigEndian(baseLength + extraLength + trailerLength));
    buf.write(Utils::to16BigEndian(d->version));
    buf.write(Utils::to16BigEndian(d->targetType));

    buf.write(Utils::to8(d->volumeName.length()));
    buf.write(Utils::padBytes(d->volumeName.toUtf8(), 27));
    buf.write(Utils::to32BigEndian(d->volumeCreated.tv_sec + hfsEpoch));
    buf.write("H+");
    buf.write(Utils::to16BigEndian(5)); // Type: other

    buf.write(Utils::to32BigEndian(d->parentInode));
    buf.write(Utils::to8(d->targetFilename.length()));
    buf.write(Utils::padBytes(d->targetFilename.toUtf8(), 63));
    buf.write(Utils::to32BigEndian(d->targetInode));
    buf.write(Utils::to32BigEndian(d->targetCreated.tv_sec + hfsEpoch));

    buf.write("\0\0\0\0\0\0\0\0", 8);
    buf.write(Utils::to16BigEndian(-1));
    buf.write(Utils::to16BigEndian(-1));
    buf.write(Utils::to32BigEndian(0x00000D02));
    buf.write(Utils::to16BigEndian(0));
    buf.write(QByteArray(10, '\0'));

    // We should now be 150 bytes in, ending the base.

    for (auto [type, bytes] : d->extras) {
        buf.write(Utils::to16SignedBigEndian(type));
        buf.write(Utils::to16BigEndian(bytes.length()));
        buf.write(bytes);
        if (bytes.length() % 2 == 1) {
            buf.write("\0", 1);
        }
    }

    buf.write(Utils::to16SignedBigEndian(-1));
    buf.write(Utils::to16BigEndian(0));

    buf.close();

    return buf.buffer();
}

QString Alias::volumeName(QString volumePath) {
    CFStringRef out;
    CFErrorRef error;

    CFURLRef url = CFURLCreateWithFileSystemPath(NULL, volumePath.toCFString(), kCFURLPOSIXPathStyle, true);

    if (!CFURLCopyResourcePropertyForKey(url, kCFURLVolumeNameKey, &out, &error)) {
        return "";
    }

    return QString::fromCFString(out);
}
