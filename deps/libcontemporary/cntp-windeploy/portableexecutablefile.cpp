#include "portableexecutablefile.h"

#include <QFile>
#include <QtEndian>

struct PortableExecutableFilePrivate {
    QByteArray image;
    bool valid = false;

    TargetMachine targetMachine = TargetMachine::Unknown;
};

PortableExecutableFile::PortableExecutableFile(QString fileName, QObject* parent)
    : QObject{parent} {
    d = new PortableExecutableFilePrivate();
    this->loadPe(fileName);
}

PortableExecutableFile::~PortableExecutableFile() {
    delete d;
}

bool PortableExecutableFile::isValid() {
    return d->valid;
}

TargetMachine PortableExecutableFile::targetMachine() {
    return d->targetMachine;
}

void PortableExecutableFile::loadPe(QString fileName) {
    d->valid = false;

    QFile peFile(fileName);
    peFile.open(QFile::ReadOnly);
    d->image = peFile.readAll();
    peFile.close();

    if (d->image.length() < 2) {
        return;
    }

    auto magicNumber = d->image.left(2);
    if (magicNumber != "MZ") {
        return;
    }

    auto peOffset = qFromLittleEndian<quint16>(d->image.mid(0x3C, 2));
    auto peMagicNumber = d->image.mid(peOffset, 4);
    if (peMagicNumber != QByteArray::fromRawData("PE\0\0", 4)) {
        return;
    }

    auto coffHeader = peOffset + 4;
    d->targetMachine = static_cast<TargetMachine>(qFromLittleEndian<quint16>(d->image.mid(coffHeader, 2)));

    d->valid = true;
}
