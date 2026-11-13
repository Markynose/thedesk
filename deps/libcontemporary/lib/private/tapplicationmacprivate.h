#ifndef TAPPLICATIONMACPRIVATE_H
#define TAPPLICATIONMACPRIVATE_H

#include <QObject>

class tApplication;
struct tApplicationMacPrivatePrivate;
class tApplicationMacPrivate : public QObject
{
    Q_OBJECT
public:
    explicit tApplicationMacPrivate(tApplication *parent);
    ~tApplicationMacPrivate();

    QString macOSBundlePath();
    QString macOSBundlePath(QString bundleIdentifier);

signals:

private:
    tApplicationMacPrivatePrivate* d;
};

#endif // TAPPLICATIONMACPRIVATE_H
