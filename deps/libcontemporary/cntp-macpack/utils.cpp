#include "utils.h"

#include <QJsonDocument>
#include <QProcess>
#include <QVariantMap>
#include <QXmlStreamWriter>
#include <QtEndian>

QByteArray Utils::toBinaryPlist(QVariantMap data) {
    QBuffer payload;
    payload.open(QBuffer::WriteOnly);
    payload.write(R"(<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">)");

    QXmlStreamWriter writer(&payload);
    writer.writeStartElement("plist");
    writer.writeAttribute("version", "1.0");
    writer.writeStartElement("dict");

    for (const auto &key : data.keys()) {
        auto value = data.value(key);
        writer.writeStartElement("key");
        writer.writeCharacters(key);
        writer.writeEndElement();

        switch (value.typeId()) {
            case QMetaType::Type::QString:
                writer.writeStartElement("string");
                writer.writeCharacters(value.toString());
                writer.writeEndElement();
                break;
            case QMetaType::Type::Int:
                writer.writeStartElement("integer");
                writer.writeCharacters(QString::number(value.toInt()));
                writer.writeEndElement();
                break;
            case QMetaType::Type::Double:
            case QMetaType::Type::Float:
                writer.writeStartElement("real");
                writer.writeCharacters(QString::number(value.toDouble()));
                writer.writeEndElement();
                break;
            case QMetaType::Type::Bool:
                if (value.toBool()) {
                    writer.writeEmptyElement("true");
                } else {
                    writer.writeEmptyElement("false");
                }
                break;
            case QMetaType::Type::QByteArray:
                writer.writeStartElement("data");
                writer.writeCharacters(value.toByteArray().toBase64());
                writer.writeEndElement();
                break;
        }
    }

    writer.writeEndElement(); // dict
    writer.writeEndElement(); // plist

    payload.close();

    auto payloadData = payload.buffer();
    QProcess proc;
    proc.start("plutil", {"-convert", "binary1", "-o", "-", "--", "-"});
    proc.write(payloadData);
    proc.closeWriteChannel();
    proc.waitForFinished(-1);
    return proc.readAll();
}

void Utils::writeBinaryPlist(QBuffer& buffer, QVariantMap data) {
    auto plistContents = toBinaryPlist(data);
    buffer.write(to32BigEndian(plistContents.length()));
    buffer.write(plistContents);
}

QByteArray Utils::to32BigEndian(quint32 value) {
    auto be = qToBigEndian(value);
    return QByteArray(reinterpret_cast<const char*>(&be), sizeof(be));
}

QByteArray Utils::to16BigEndian(quint16 value) {
    auto be = qToBigEndian(value);
    return QByteArray(reinterpret_cast<const char*>(&be), sizeof(be));
}

QByteArray Utils::to8(quint8 value) {
    return QByteArray(reinterpret_cast<const char*>(&value), sizeof(value));
}

QByteArray Utils::padBytes(QByteArray data, int length) {
    return data.append(QByteArray(length - data.length(), '\0'));
}

QByteArray Utils::to16SignedBigEndian(qint16 value) {
    auto be = qToBigEndian(value);
    return QByteArray(reinterpret_cast<const char*>(&be), sizeof(be));
}
