#include "platformicongenerator.h"

#ifdef Q_OS_MAC
    #include "platformgenerators/macplatformicongenerator.h"
#elif defined(Q_OS_WIN)
    #include "platformgenerators/winplatformicongenerator.h"
#endif

struct PlatformIconGeneratorPrivate {
        QString inputSvg;
        QString outputFile;
};

PlatformIconGenerator::PlatformIconGenerator(QObject* parent) :
    QObject{parent} {
    d = new PlatformIconGeneratorPrivate();
}

PlatformIconGenerator::~PlatformIconGenerator() {
    delete d;
}

void PlatformIconGenerator::setInputSvg(QString svg) {
    d->inputSvg = svg;
}

QString PlatformIconGenerator::inputSvg() {
    return d->inputSvg;
}

void PlatformIconGenerator::setOutputFile(QString outputFile) {
    d->outputFile = outputFile;
}

QString PlatformIconGenerator::outputFile() {
    return d->outputFile;
}

PlatformIconGenerator* PlatformIconGenerator::iconGeneratorForPlatform() {
#ifdef Q_OS_MAC
    return new MacPlatformIconGenerator();
#elif defined(Q_OS_WIN)
    return new WinPlatformIconGenerator();
#endif
    return nullptr;
}
