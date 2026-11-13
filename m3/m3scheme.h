/****************************************
 *
 *   theDesk - Material 3 colour schemes
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 * *************************************/
#ifndef M3SCHEME_H
#define M3SCHEME_H

#include <QColor>
#include <QList>
#include <QDateTime>
#include <QPalette>
#include <QString>

/**
 * Material 3 dynamic colour for theDesk (https://m3.material.io/styles/color).
 *
 * The seed is the living horizon by default: each scene has a seed picked by
 * Material's own wallpaper scoring (quantize + score on the rendered scene),
 * and between keyframes the seed's hue travels around the colour wheel the
 * way the sky does (blue -> violet -> peach at dusk, never through green).
 *
 * Settings (theSuite/theDesk.platform, group Palette):
 *   base=m3                    use this instead of a palettes.json palette
 *   m3.seed=horizon | #rrggbb  where the seed comes from
 *   m3.variant=tonalspot       tonalspot vibrant expressive fidelity content
 *                              neutral monochrome rainbow fruitsalad
 *   m3.mode=dark               dark | light | horizon (light in daytime)
 *   m3.contrast=0              -1 (reduced) .. 0 (standard) .. 1 (high)
 */
namespace M3 {
    enum class Variant {
        TonalSpot,
        Vibrant,
        Expressive,
        Fidelity,
        Content,
        Neutral,
        Monochrome,
        Rainbow,
        FruitSalad
    };

    enum class Mode {
        Dark,
        Light,
        Horizon
    };

    struct Scheme {
            bool dark = true;
            QColor seed;

            QColor primary, onPrimary, primaryContainer, onPrimaryContainer, inversePrimary;
            QColor secondary, onSecondary, secondaryContainer, onSecondaryContainer;
            QColor tertiary, onTertiary, tertiaryContainer, onTertiaryContainer;
            QColor error, onError, errorContainer, onErrorContainer;
            QColor surface, surfaceDim, surfaceBright;
            QColor surfaceContainerLowest, surfaceContainerLow, surfaceContainer, surfaceContainerHigh, surfaceContainerHighest;
            QColor onSurface, surfaceVariant, onSurfaceVariant, inverseSurface, inverseOnSurface;
            QColor outline, outlineVariant, shadow, scrim, surfaceTint;

            static Scheme generate(QColor seed, bool dark, Variant variant = Variant::TonalSpot, double contrast = 0);

            // Qt palette roles mapped from the M3 roles; disabled colours use
            // M3's 38% / 12% opacities pre-blended onto the surface.
            QPalette toPalette() const;

            bool operator==(const Scheme& other) const;
    };

    struct Config {
            QString seed = QStringLiteral("horizon");
            Variant variant = Variant::TonalSpot;
            Mode mode = Mode::Dark;
            double contrast = 0;

            static Config fromSettings(); // reads theSuite/theDesk.platform
            static Variant variantFromString(QString variant);
            bool followsHorizon() const; // changes with the time of day
    };

    QColor horizonSeed(QTime time);
    // The same seed for a blend between two horizon scenes (HorizonClock's
    // scene ids), so the wallpaper can follow it exactly
    QColor horizonSeed(int fromScene, int toScene, double progress);
    // The seed a config resolves to at a time: the chosen colour, or the horizon's
    QColor seedFor(const Config& config, QDateTime when = QDateTime::currentDateTime());
    bool horizonIsDay(QTime time);

    Scheme schemeFor(const Config& config, QDateTime when = QDateTime::currentDateTime());
    Scheme currentScheme();

    // Sixteen terminal colours (ANSI order: black, red, green, yellow, blue,
    // magenta, cyan, white, then the bright ones) for a scheme: the ANSI hues
    // harmonized toward the seed so they sit with the scheme, black and white
    // from its surfaces
    QList<QColor> terminalColours(const Scheme& scheme, QColor seed, bool dark);

    // Draws `content` at `opacity` over `base` (state layers, disabled colours)
    QColor layer(QColor base, QColor content, double opacity);
} // namespace M3

#endif // M3SCHEME_H
