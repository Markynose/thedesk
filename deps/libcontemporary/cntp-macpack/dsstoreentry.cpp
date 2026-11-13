#include "dsstoreentry.h"

#include "utils.h"
#include <QStringEncoder>
#include <QVariantMap>
#include <QtEndian>

struct DsStoreEntryPrivate {
        QString filename;
        QString structureId;
        QString dataType;
        QByteArray buffer;
};

DsStoreEntry::~DsStoreEntry() {
    delete d;
}

DsStoreEntry& DsStoreEntry::operator=(const DsStoreEntry& other) {
    d->filename = other.d->filename;
    d->structureId = other.d->structureId;
    d->dataType = other.d->dataType;
    d->buffer = other.d->buffer;
    return *this;
}

DsStoreEntry DsStoreEntry::entryFor(QString filename, QString structureId, QVariantMap options) {
    QString dataType;
    QBuffer blob;
    blob.open(QBuffer::WriteOnly);
    if (structureId == "BKGD") {
        dataType = "blob";

        blob.write(Utils::to32BigEndian(8));

        if (options.contains("pictureByteLength")) {
            blob.write(QStringLiteral("PctB").toLatin1());

            blob.write(Utils::to32BigEndian(options.value("pictureByteLength").toInt()));
        } else {
            blob.write(QStringLiteral("DefB").toLatin1());
        }
    } else if (structureId == "Iloc") {
        dataType = "blob";

        blob.write(Utils::to32BigEndian(12));

        blob.write(Utils::to32BigEndian(options.value("x").toInt()));
        blob.write(Utils::to32BigEndian(options.value("y").toInt()));
        const char* sentinel = "\xFF\xFF\xFF\x00";
        blob.write(sentinel, 4);
    } else if (structureId == "vSrn") {
        dataType = "long";
        blob.write(Utils::to32BigEndian(options.value("value").toInt()));
    } else if (structureId == "bwsp") {
        dataType = "blob";
        Utils::writeBinaryPlist(blob, {
                                          {"ContainerShowSidebar", true                                                                                       },
                                          {"ShowPathbar",          false                                                                                      },
                                          {"ShowSidebar",          true                                                                                       },
                                          {"ShowStatusBar",        false                                                                                      },
                                          {"ShowTabView",          false                                                                                      },
                                          {"ShowToolbar",          false                                                                                      },
                                          {"SidebarWidth",         0                                                                                          },
                                          {"WindowBounds",         QStringLiteral("{{%1,%2},{%3,%4}}")
                                                               .arg(options.value("x").toInt())
                                                               .arg(options.value("y").toInt())
                                                               .arg(options.value("width").toInt())
                                                               .arg(options.value("height").toInt())}
        });
    } else if (structureId == "icvp") {
        dataType = "blob";
        Utils::writeBinaryPlist(blob, {
                                          {"backgroundType",       2                                      },
                                          {"backgroundColorRed",   1.0                                    },
                                          {"backgroundColorGreen", 1.0                                    },
                                          {"backgroundColorBlue",  1.0                                    },
                                          {"showIconPreview",      true                                   },
                                          {"showItemInfo",         false                                  },
                                          {"textSize",             12                                     },
                                          {"iconSize",             options.value("iconSize").toInt()      },
                                          {"viewOptionsVersion",   1                                      },
                                          {"gridSpacing",          100.0                                  },
                                          {"gridOffsetX",          0.0                                    },
                                          {"gridOffsetY",          0.0                                    },
                                          {"labelOnBottom",        true                                   },
                                          {"arrangeBy",            "none"                                 },
                                          {"backgroundImageAlias", options.value("rawAlias").toByteArray()}
        });
    }

    blob.close();
    return DsStoreEntry(filename, structureId, dataType, blob.data());
}

quint64 DsStoreEntry::length() {
    return d->buffer.length();
}

QByteArray DsStoreEntry::buffer() {
    return d->buffer;
}

DsStoreEntry::DsStoreEntry(QString filename, QString structureId, QString dataType, QByteArray blob) {
    d = new DsStoreEntryPrivate();
    d->filename = filename;
    d->structureId = structureId;
    d->dataType = dataType;

    for (auto c : qAsConst(filename)) {
        if (c.decompositionTag() == QChar::Canonical) {
            d->filename += c.decomposition();
        } else {
            d->filename += c;
        }
    }

    QStringEncoder converter(QStringEncoder::Utf16BE);

    auto filenameLength = filename.length();

    QBuffer buf(&d->buffer);
    buf.open(QBuffer::WriteOnly);
    buf.write(Utils::to32BigEndian(filenameLength));
    buf.write(converter.encode(filename));
    buf.write(structureId.toLatin1());
    buf.write(dataType.toLatin1());
    buf.write(blob);
    buf.close();
}

DsStoreEntry::DsStoreEntry(const DsStoreEntry& other) {
    d = new DsStoreEntryPrivate();
    d->filename = other.d->filename;
    d->structureId = other.d->structureId;
    d->dataType = other.d->dataType;
    d->buffer = other.d->buffer;
}

DsStoreEntry::DsStoreEntry(DsStoreEntry&& other) {
    d = other.d;
    other.d = new DsStoreEntryPrivate();
}
