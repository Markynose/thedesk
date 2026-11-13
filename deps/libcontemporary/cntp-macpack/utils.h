#ifndef UTILS_H
#define UTILS_H

#include <QBuffer>
#include <QJsonObject>

namespace Utils {
    QByteArray to8(quint8 value);
    QByteArray to16BigEndian(quint16 value);
    QByteArray to16SignedBigEndian(qint16 value);
    QByteArray to32BigEndian(quint32 value);
    void writeBinaryPlist(QBuffer& buffer, QVariantMap data);
    QByteArray toBinaryPlist(QVariantMap data);
    QByteArray padBytes(QByteArray data, int length);
}; // namespace Utils

#endif // UTILS_H
