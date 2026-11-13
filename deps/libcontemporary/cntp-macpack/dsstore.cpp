#include "dsstore.h"
#include "dsstoreentry.h"

#include "utils.h"
#include <QBuffer>
#include <QFile>
#include <QRect>
#include <QVariantMap>

struct DsStorePrivate {
        QList<DsStoreEntry> entries;
};

DsStore::DsStore(QObject* parent) :
    QObject{parent} {
    d = new DsStorePrivate();
}

DsStore::~DsStore() {
    delete d;
}

void DsStore::write(QString file) {
    QBuffer modified;
    modified.open(QBuffer::WriteOnly);
    auto currentPos = 0;
    auto P = 0;
    auto count = d->entries.length();
    modified.write(Utils::to32BigEndian(P));
    modified.write(Utils::to32BigEndian(count));

    for (auto entry : d->entries) {
        modified.write(entry.buffer());
    }

    modified.close();

    QFile dsstoreFile(":/DSStore-clean");
    dsstoreFile.open(QFile::ReadOnly);
    auto dsstoreFileContents = dsstoreFile.readAll();
    dsstoreFile.close();

    QBuffer buf(&dsstoreFileContents);
    buf.open(QBuffer::ReadWrite | QBuffer::Append);
    buf.seek(76);
    buf.write(Utils::to32BigEndian(d->entries.length()));
    buf.seek(4100);
    buf.write(modified.buffer());
    buf.close();

    QFile endFile(file);
    endFile.open(QFile::WriteOnly);
    endFile.write(buf.buffer());
    endFile.close();
}

void DsStore::vSrn(int value) {
    d->entries.append(DsStoreEntry::entryFor(".", "vSrn", {
                                                              {"value", value}
    }));
}

void DsStore::moveIcon(QString file, int x, int y) {
    d->entries.append(DsStoreEntry::entryFor(file, "Iloc", {
                                                               {"x", x},
                                                               {"y", y}
    }));
}

void DsStore::setWindowGeometry(QRect geometry) {
    d->entries.append(DsStoreEntry::entryFor(".", "bwsp", {
                                                              {"x",      geometry.x()          },
                                                              {"y",      geometry.y()          },
                                                              {"width",  geometry.width()      },
                                                              {"height", geometry.height() + 22}
    }));
}

void DsStore::setWindowProperties(int iconSize, QByteArray backgroundAlias) {
    d->entries.append(DsStoreEntry::entryFor(".", "icvp", {
                                                              {"iconSize", iconSize       },
                                                              {"rawAlias", backgroundAlias},
    }));
}
