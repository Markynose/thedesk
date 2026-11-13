#ifndef COMBINEDICON_H
#define COMBINEDICON_H

#include <QObject>

struct CombinedIconPrivate;
class CombinedIcon : public QObject
{
    Q_OBJECT
public:
    explicit CombinedIcon(QObject *parent = nullptr);
    ~CombinedIcon();

    enum BaseIcon {
        PlatformSpecificIcon,
        CrossPlatformIcon,
        MacIcon
    };
    void setBaseIcon(BaseIcon base);
    void setGenerateBlueprintIcon(bool blueprint);
    void setIconGradientColors(QColor color1, QColor color2);

    void setOverlayIcon(QString overlayIcon);
    void setOverlayIconMac(QString overlayIcon);

    QString generatedIcon();

signals:

private:
    CombinedIconPrivate* d;

};

#endif // COMBINEDICON_H
