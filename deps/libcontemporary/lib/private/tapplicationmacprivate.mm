#include "tapplicationmacprivate.h"

#import <AppKit/AppKit.h>
#include "tapplication.h"

struct tApplicationMacPrivatePrivate {

};

tApplicationMacPrivate::tApplicationMacPrivate(tApplication *parent)
    : QObject{parent}
{
    d = new tApplicationMacPrivatePrivate();
}

tApplicationMacPrivate::~tApplicationMacPrivate()
{
    delete d;
}

QString tApplicationMacPrivate::macOSBundlePath()
{
    NSBundle* mainBundle = [NSBundle mainBundle];
    return QString::fromNSString([mainBundle bundlePath]);
}

QString tApplicationMacPrivate::macOSBundlePath(QString bundleIdentifier)
{
    NSBundle* bundle = [NSBundle bundleWithIdentifier:bundleIdentifier.toNSString()];
    return QString::fromNSString([bundle bundlePath]);
}
