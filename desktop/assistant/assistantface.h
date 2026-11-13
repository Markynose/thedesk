#ifndef ASSISTANTFACE_H
#define ASSISTANTFACE_H

#include <QPair>
#include <m3scheme.h>
#include <tsettings.h>

// Pebble's face colours (ink, fill): the one picked in its tour; thinking
// harder takes another so the switch shows
inline QPair<QColor, QColor> pebbleFaceColours(const M3::Scheme& s, bool deep) {
    const QString face = tSettings().value(QStringLiteral("Assistant/face")).toString();
    QPair<QColor, QColor> pick{s.onPrimaryContainer, s.primaryContainer};
    if (face == "secondary") pick = {s.onSecondaryContainer, s.secondaryContainer};
    else if (face == "tertiary") pick = {s.onTertiaryContainer, s.tertiaryContainer};
    else if (face == "ink") pick = {s.inverseOnSurface, s.inverseSurface};
    if (!deep) return pick;
    if (face == "tertiary") return {s.onPrimaryContainer, s.primaryContainer};
    return {s.onTertiaryContainer, s.tertiaryContainer};
}

#endif // ASSISTANTFACE_H
