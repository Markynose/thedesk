#ifndef MACPLATFORMICONGENERATOR_H
#define MACPLATFORMICONGENERATOR_H

#include "platformicongenerator.h"

class MacPlatformIconGenerator : public PlatformIconGenerator
{
    Q_OBJECT
public:
    explicit MacPlatformIconGenerator(QObject *parent = nullptr);

    void generateIcon();

signals:

private:
    void renderPng(QString output, QSize size);
};

#endif // MACPLATFORMICONGENERATOR_H
