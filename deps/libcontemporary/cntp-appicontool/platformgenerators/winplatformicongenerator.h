#ifndef MACPLATFORMICONGENERATOR_H
#define MACPLATFORMICONGENERATOR_H

#include "platformicongenerator.h"

class WinPlatformIconGenerator : public PlatformIconGenerator {
    Q_OBJECT
public:
    explicit WinPlatformIconGenerator(QObject *parent = nullptr) : PlatformIconGenerator(parent) {
    }

    virtual void generateIcon() override;

private:
    void renderPng(QString output, int sideLength);
};

#endif// MACPLATFORMICONGENERATOR_H
