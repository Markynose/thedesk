#include "combinedicon.h"

#include <QColor>
#include <QFile>
#include <QRegularExpression>

struct CombinedIconPrivate {
    CombinedIcon::BaseIcon base = CombinedIcon::CrossPlatformIcon;
    bool blueprint = false;
    QColor color1, color2;
    QString overlayIcon;
    QString overlayIconMac;
};

CombinedIcon::CombinedIcon(QObject *parent)
    : QObject{parent}
{
    d = new CombinedIconPrivate();
}

CombinedIcon::~CombinedIcon()
{
    delete d;
}

void CombinedIcon::setBaseIcon(BaseIcon base)
{
    d->base = base;
}

void CombinedIcon::setGenerateBlueprintIcon(bool blueprint)
{
    d->blueprint = blueprint;
}

void CombinedIcon::setIconGradientColors(QColor color1, QColor color2)
{
    d->color1 = color1;
    d->color2 = color2;
}

void CombinedIcon::setOverlayIcon(QString overlayIcon)
{
    d->overlayIcon = overlayIcon;
}

void CombinedIcon::setOverlayIconMac(QString overlayIcon)
{
    d->overlayIconMac = overlayIcon;
}

QString CombinedIcon::generatedIcon()
{
    QString baseResourceFilePath;
    bool shouldSubstituteColour;

    if (d->blueprint) {
        shouldSubstituteColour = false;
        baseResourceFilePath = ":/assets/base-app-icon-blueprint%1.svg";
    } else {
        baseResourceFilePath = ":/assets/base-app-icon%1.svg";
        shouldSubstituteColour = true;
    }

    BaseIcon baseIcon = d->base;
#ifdef Q_OS_MAC
    if (baseIcon == PlatformSpecificIcon) baseIcon = MacIcon;
#else
    if (baseIcon == PlatformSpecificIcon) baseIcon = CrossPlatformIcon;
#endif
    if (baseIcon == MacIcon) {
        baseResourceFilePath = baseResourceFilePath.arg("-mac");
    } else {
        baseResourceFilePath = baseResourceFilePath.arg("");
    }

    QFile baseResourceFile(baseResourceFilePath);
    baseResourceFile.open(QFile::ReadOnly);
    QString baseResourceFileContents = baseResourceFile.readAll();
    baseResourceFile.close();

    //Substitute in the base colours
    if (shouldSubstituteColour) {
        baseResourceFileContents = baseResourceFileContents.arg(d->color1.name(), d->color2.name());
    }

    QString overlayIcon = d->overlayIcon;
    if (baseIcon == MacIcon) overlayIcon = d->overlayIconMac;

    QRegularExpression layerGroup("</g>.+(<g.+id=\"iconlayer\".+</g>)");
    layerGroup.setPatternOptions(QRegularExpression::DotMatchesEverythingOption);
    QRegularExpressionMatch match = layerGroup.match(overlayIcon);
    if (!match.hasMatch()) {
        return "";
    }

    QString captured = match.captured(1);
    baseResourceFileContents = baseResourceFileContents.arg(match.captured(1));

    return baseResourceFileContents;
}
