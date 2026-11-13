#ifndef PLATFORMICONGENERATOR_H
#define PLATFORMICONGENERATOR_H

#include <QObject>

struct PlatformIconGeneratorPrivate;
class PlatformIconGenerator : public QObject
{
    Q_OBJECT
public:
    explicit PlatformIconGenerator(QObject *parent = nullptr);
    ~PlatformIconGenerator();

    void setInputSvg(QString svg);
    QString inputSvg();

    void setOutputFile(QString outputFile);
    QString outputFile();

    static PlatformIconGenerator* iconGeneratorForPlatform();

    virtual void generateIcon() = 0;

signals:

private:
    PlatformIconGeneratorPrivate* d;
};

#endif // PLATFORMICONGENERATOR_H
