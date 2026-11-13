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
#include "m3scheme.h"

#include "horizonclock.h"

#include <QtMath>
#include <tsettings.h>

#include "cpp/blend/blend.h"
#include "cpp/cam/hct.h"
#include "cpp/scheme/scheme_content.h"
#include "cpp/scheme/scheme_expressive.h"
#include "cpp/scheme/scheme_fidelity.h"
#include "cpp/scheme/scheme_fruit_salad.h"
#include "cpp/scheme/scheme_monochrome.h"
#include "cpp/scheme/scheme_neutral.h"
#include "cpp/scheme/scheme_rainbow.h"
#include "cpp/scheme/scheme_tonal_spot.h"
#include "cpp/scheme/scheme_vibrant.h"

#include <memory>

namespace mcu = material_color_utilities;

namespace {
    // Seeds for each scene (HorizonClock::Scene order), picked by
    // material-color-utilities' QuantizeCelebi(128) + RankedSuggestions on a
    // 1366x768 render of the scene. Re-run that if the scenes change.
    struct SceneSeed {
            double hue;
            double chroma;
    };

    constexpr SceneSeed sceneSeeds[] = {
        {68, 17},  // dusk-ocean: #d2b292 sand-gold glow
        {236, 26}, // dune: #95bad3 daytime sky
        {156, 18}, // aurora-night: #203626 aurora green
        {53, 44},  // ember: #995117 embers
    };

    double wrapHue(double hue) {
        hue = std::fmod(hue, 360.0);
        return hue < 0 ? hue + 360 : hue;
    }

    // Does travelling from `from` by `delta` degrees pass strictly through `hue`?
    bool arcContains(double from, double delta, double hue) {
        double offset = wrapHue(hue - from);
        if (delta >= 0) return offset > 0 && offset < delta;
        return offset > 0 && 360 - offset < -delta;
    }

    QColor fromArgb(mcu::Argb argb) {
        return QColor::fromRgba(argb);
    }

    std::unique_ptr<mcu::DynamicScheme> makeScheme(mcu::Hct hct, bool dark, M3::Variant variant, double contrast) {
        switch (variant) {
            case M3::Variant::Vibrant:
                return std::make_unique<mcu::SchemeVibrant>(hct, dark, contrast);
            case M3::Variant::Expressive:
                return std::make_unique<mcu::SchemeExpressive>(hct, dark, contrast);
            case M3::Variant::Fidelity:
                return std::make_unique<mcu::SchemeFidelity>(hct, dark, contrast);
            case M3::Variant::Content:
                return std::make_unique<mcu::SchemeContent>(hct, dark, contrast);
            case M3::Variant::Neutral:
                return std::make_unique<mcu::SchemeNeutral>(hct, dark, contrast);
            case M3::Variant::Monochrome:
                return std::make_unique<mcu::SchemeMonochrome>(hct, dark, contrast);
            case M3::Variant::Rainbow:
                return std::make_unique<mcu::SchemeRainbow>(hct, dark, contrast);
            case M3::Variant::FruitSalad:
                return std::make_unique<mcu::SchemeFruitSalad>(hct, dark, contrast);
            case M3::Variant::TonalSpot:
            default:
                return std::make_unique<mcu::SchemeTonalSpot>(hct, dark, contrast);
        }
    }
} // namespace

QColor M3::horizonSeed(QTime time) {
    HorizonClock::Blend blend = HorizonClock::blendForTime(time);
    return horizonSeed(blend.from, blend.to, blend.progress);
}

QColor M3::horizonSeed(int fromScene, int toScene, double progress) {
    constexpr int sceneCount = int(std::size(sceneSeeds));
    SceneSeed from = sceneSeeds[qBound(0, fromScene, sceneCount - 1)];
    SceneSeed to = sceneSeeds[qBound(0, toScene, sceneCount - 1)];
    progress = qBound(0.0, progress, 1.0);

    // Shortest way round the wheel, unless that crosses green (~150) between
    // two non-green colours: the sky goes blue -> violet -> peach, not via teal
    double delta = wrapHue(to.hue - from.hue);
    if (delta > 180) delta -= 360;
    auto nearGreen = [](double hue) {
        return std::abs(hue - 150) < 25;
    };
    if (arcContains(from.hue, delta, 150) && !nearGreen(from.hue) && !nearGreen(to.hue)) {
        delta = delta > 0 ? delta - 360 : delta + 360;
    }

    // Ease the crossfade so the tint lingers on each scene
    double t = progress * progress * (3 - 2 * progress);
    double hue = wrapHue(from.hue + delta * t);
    double chroma = from.chroma + (to.chroma - from.chroma) * t;
    return fromArgb(mcu::Hct(hue, chroma, 50).ToInt());
}

bool M3::horizonIsDay(QTime time) {
    return time >= QTime(8, 0) && time < QTime(18, 0);
}

M3::Scheme M3::Scheme::generate(QColor seed, bool dark, Variant variant, double contrast) {
    auto s = makeScheme(mcu::Hct(seed.rgba() | 0xff000000), dark, variant, qBound(-1.0, contrast, 1.0));

    Scheme scheme;
    scheme.dark = dark;
    scheme.seed = seed;
    scheme.primary = fromArgb(s->GetPrimary());
    scheme.onPrimary = fromArgb(s->GetOnPrimary());
    scheme.primaryContainer = fromArgb(s->GetPrimaryContainer());
    scheme.onPrimaryContainer = fromArgb(s->GetOnPrimaryContainer());
    scheme.inversePrimary = fromArgb(s->GetInversePrimary());
    scheme.secondary = fromArgb(s->GetSecondary());
    scheme.onSecondary = fromArgb(s->GetOnSecondary());
    scheme.secondaryContainer = fromArgb(s->GetSecondaryContainer());
    scheme.onSecondaryContainer = fromArgb(s->GetOnSecondaryContainer());
    scheme.tertiary = fromArgb(s->GetTertiary());
    scheme.onTertiary = fromArgb(s->GetOnTertiary());
    scheme.tertiaryContainer = fromArgb(s->GetTertiaryContainer());
    scheme.onTertiaryContainer = fromArgb(s->GetOnTertiaryContainer());
    scheme.error = fromArgb(s->GetError());
    scheme.onError = fromArgb(s->GetOnError());
    scheme.errorContainer = fromArgb(s->GetErrorContainer());
    scheme.onErrorContainer = fromArgb(s->GetOnErrorContainer());
    scheme.surface = fromArgb(s->GetSurface());
    scheme.surfaceDim = fromArgb(s->GetSurfaceDim());
    scheme.surfaceBright = fromArgb(s->GetSurfaceBright());
    scheme.surfaceContainerLowest = fromArgb(s->GetSurfaceContainerLowest());
    scheme.surfaceContainerLow = fromArgb(s->GetSurfaceContainerLow());
    scheme.surfaceContainer = fromArgb(s->GetSurfaceContainer());
    scheme.surfaceContainerHigh = fromArgb(s->GetSurfaceContainerHigh());
    scheme.surfaceContainerHighest = fromArgb(s->GetSurfaceContainerHighest());
    scheme.onSurface = fromArgb(s->GetOnSurface());
    scheme.surfaceVariant = fromArgb(s->GetSurfaceVariant());
    scheme.onSurfaceVariant = fromArgb(s->GetOnSurfaceVariant());
    scheme.inverseSurface = fromArgb(s->GetInverseSurface());
    scheme.inverseOnSurface = fromArgb(s->GetInverseOnSurface());
    scheme.outline = fromArgb(s->GetOutline());
    scheme.outlineVariant = fromArgb(s->GetOutlineVariant());
    scheme.shadow = fromArgb(s->GetShadow());
    scheme.scrim = fromArgb(s->GetScrim());
    scheme.surfaceTint = fromArgb(s->GetSurfaceTint());
    return scheme;
}

QPalette M3::Scheme::toPalette() const {
    QPalette pal;
    auto set = [&pal](QPalette::ColorRole role, QColor active, QColor disabled) {
        pal.setColor(QPalette::Active, role, active);
        pal.setColor(QPalette::Inactive, role, active);
        pal.setColor(QPalette::Disabled, role, disabled);
    };
    // M3 disabled: content at 38%, containers at 12% of on-surface
    QColor disabledText = layer(surface, onSurface, 0.38);
    QColor disabledContainer = layer(surface, onSurface, 0.12);

    set(QPalette::Window, surface, surface);
    set(QPalette::WindowText, onSurface, disabledText);
    set(QPalette::Base, surfaceContainerLow, surface);
    set(QPalette::AlternateBase, surfaceContainer, surface);
    set(QPalette::Text, onSurface, disabledText);
    set(QPalette::PlaceholderText, onSurfaceVariant, disabledText);
    set(QPalette::Button, surfaceContainerHigh, disabledContainer);
    set(QPalette::ButtonText, onSurface, disabledText);
    set(QPalette::BrightText, onSurface, disabledText);
    set(QPalette::Highlight, primary, disabledContainer);
    set(QPalette::HighlightedText, onPrimary, disabledText);
    set(QPalette::Accent, primary, disabledContainer);
    set(QPalette::Link, primary, disabledText);
    set(QPalette::LinkVisited, tertiary, disabledText);
    set(QPalette::ToolTipBase, inverseSurface, inverseSurface);
    set(QPalette::ToolTipText, inverseOnSurface, inverseOnSurface);
    // Bevel roles, for styles that still draw them
    set(QPalette::Light, surfaceBright, surfaceBright);
    set(QPalette::Midlight, surfaceContainerHighest, surfaceContainerHighest);
    set(QPalette::Mid, outlineVariant, outlineVariant);
    set(QPalette::Dark, surfaceDim, surfaceDim);
    set(QPalette::Shadow, shadow, shadow);
    return pal;
}

bool M3::Scheme::operator==(const Scheme& other) const {
    return dark == other.dark && primary == other.primary && secondary == other.secondary && tertiary == other.tertiary && surface == other.surface && surfaceContainer == other.surfaceContainer && onSurface == other.onSurface && error == other.error && outline == other.outline;
}

M3::Variant M3::Config::variantFromString(QString variant) {
    variant = variant.toLower().remove(QLatin1Char('-')).remove(QLatin1Char('_'));
    if (variant == QStringLiteral("vibrant")) return Variant::Vibrant;
    if (variant == QStringLiteral("expressive")) return Variant::Expressive;
    if (variant == QStringLiteral("fidelity")) return Variant::Fidelity;
    if (variant == QStringLiteral("content")) return Variant::Content;
    if (variant == QStringLiteral("neutral")) return Variant::Neutral;
    if (variant == QStringLiteral("monochrome")) return Variant::Monochrome;
    if (variant == QStringLiteral("rainbow")) return Variant::Rainbow;
    if (variant == QStringLiteral("fruitsalad")) return Variant::FruitSalad;
    return Variant::TonalSpot;
}

M3::Config M3::Config::fromSettings() {
    tSettings settings(QStringLiteral("theSuite"), QStringLiteral("theDesk.platform"));
    auto read = [&settings](QString key, QString fallback) {
        QString value = settings.value(QStringLiteral("Palette/") + key).toString().trimmed();
        return value.isEmpty() ? fallback : value;
    };

    Config config;
    config.seed = read(QStringLiteral("m3.seed"), QStringLiteral("horizon"));
    config.variant = variantFromString(read(QStringLiteral("m3.variant"), QStringLiteral("tonalspot")));
    QString mode = read(QStringLiteral("m3.mode"), QStringLiteral("dark")).toLower();
    config.mode = mode == QStringLiteral("light") ? Mode::Light : mode == QStringLiteral("horizon") ? Mode::Horizon
                                                                                                  : Mode::Dark;
    config.contrast = read(QStringLiteral("m3.contrast"), QStringLiteral("0")).toDouble();
    return config;
}

bool M3::Config::followsHorizon() const {
    return seed.compare(QStringLiteral("horizon"), Qt::CaseInsensitive) == 0 || mode == Mode::Horizon;
}

QColor M3::seedFor(const Config& config, QDateTime when) {
    QColor seed;
    if (config.seed.compare(QStringLiteral("horizon"), Qt::CaseInsensitive) != 0) {
        seed = QColor::fromString(config.seed);
    }
    return seed.isValid() ? seed : horizonSeed(when.time());
}

M3::Scheme M3::schemeFor(const Config& config, QDateTime when) {
    QTime time = when.time();
    const QColor seed = seedFor(config, when);

    bool dark = config.mode == Mode::Dark || (config.mode == Mode::Horizon && !horizonIsDay(time));
    return Scheme::generate(seed, dark, config.variant, config.contrast);
}

M3::Scheme M3::currentScheme() {
    return schemeFor(Config::fromSettings());
}

QList<QColor> M3::terminalColours(const Scheme& scheme, QColor seed, bool dark) {
    // HCT hues of the ANSI colours: red, green, yellow, blue, magenta, cyan
    static const double hues[] = {25, 142, 90, 262, 330, 196};
    const mcu::Argb key = seed.rgba() | 0xff000000;
    auto tone = [&](double hue, double chroma, double t) {
        // Harmonize: turn the hue up to 15 degrees toward the seed's
        const mcu::Argb design = mcu::Hct(hue, chroma, t).ToInt();
        const mcu::Hct harmonized(mcu::BlendHarmonize(design, key));
        return fromArgb(mcu::Hct(harmonized.get_hue(), chroma, t).ToInt());
    };
    QList<QColor> normal, bright;
    for (double hue : hues) {
        normal.append(tone(hue, 52, dark ? 75 : 40));
        bright.append(tone(hue, 40, dark ? 87 : 30));
    }
    QList<QColor> out;
    out << scheme.surfaceContainerHighest << normal << scheme.onSurfaceVariant;
    out << scheme.outline << bright << scheme.onSurface;
    return out;
}

QColor M3::layer(QColor base, QColor content, double opacity) {
    opacity = qBound(0.0, opacity * content.alphaF(), 1.0);
    return QColor::fromRgbF(base.redF() + (content.redF() - base.redF()) * opacity,
        base.greenF() + (content.greenF() - base.greenF()) * opacity,
        base.blueF() + (content.blueF() - base.blueF()) * opacity,
        base.alphaF());
}
