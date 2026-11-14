/****************************************
 *
 *   theDesk - Material 3 colour settings
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 * *************************************/
#include "m3coloursettings.h"

#include <QAbstractButton>
#include <QComboBox>
#include <QPushButton>
#include <QEvent>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QTime>
#include <m3motion.h>
#include <m3scheme.h>
#include <shapes/materialshapes.h>
#include <shapes/morph.h>
#include <shapes/shapepath.h>
#include <tsettings.h>
#include <wallpaperview.h>

namespace {
    // Seeds after the horizon: M3's baseline purple and a spread of hues
    const QList<QColor>& presetSeeds() {
        static const QList<QColor> seeds = {QColor(0x6750a4), QColor(0x3a5ba9), QColor(0x00696e), QColor(0x386a20),
            QColor(0x7c5800), QColor(0xa8364a), QColor(0x8c4a60), QColor(0x5d5f5f)};
        return seeds;
    }

    struct VariantName {
            const char* key;
            const char* label;
    };
    const QList<VariantName>& variants() {
        static const QList<VariantName> list = {
            {"tonalspot", QT_TRANSLATE_NOOP("M3ColourSettings", "Tonal spot")}, {"vibrant", QT_TRANSLATE_NOOP("M3ColourSettings", "Vibrant")},
            {"expressive", QT_TRANSLATE_NOOP("M3ColourSettings", "Expressive")}, {"fidelity", QT_TRANSLATE_NOOP("M3ColourSettings", "Fidelity")},
            {"content", QT_TRANSLATE_NOOP("M3ColourSettings", "Content")}, {"neutral", QT_TRANSLATE_NOOP("M3ColourSettings", "Neutral")},
            {"monochrome", QT_TRANSLATE_NOOP("M3ColourSettings", "Monochrome")}, {"rainbow", QT_TRANSLATE_NOOP("M3ColourSettings", "Rainbow")},
            {"fruitsalad", QT_TRANSLATE_NOOP("M3ColourSettings", "Fruit salad")}};
        return list;
    }

    M3::Variant variantFor(const QString& key) {
        const QStringList keys = {"tonalspot", "vibrant", "expressive", "fidelity", "content", "neutral", "monochrome", "rainbow", "fruitsalad"};
        int i = keys.indexOf(key.toLower());
        return i < 0 ? M3::Variant::TonalSpot : M3::Variant(i);
    }

    // One swatch: a circle in three of its scheme's colours (Android's colour
    // options); chosen, it morphs into a turning cookie inside a ring
    class Swatch : public QAbstractButton {
        public:
            Swatch(QString value, QWidget* parent) :
                QAbstractButton(parent), value(std::move(value)) {
                this->setCheckable(true);
                this->setAutoExclusive(true);
                this->setFixedSize(60, 60);
                this->setAttribute(Qt::WA_Hover);
                this->setCursor(Qt::PointingHandCursor);
                this->setProperty("m3HoverMerge", false);
                select = new M3SpringAnimation(this);
                select->setToken(M3Motion::SpatialFast);
                select->snapTo(0.0);
                hover = new M3SpringAnimation(this);
                hover->setToken(M3Motion::EffectsFast);
                hover->snapTo(0.0);
                for (auto* s : {select, hover}) QObject::connect(s, &M3SpringAnimation::valueChanged, this, qOverload<>(&QWidget::update));
                QObject::connect(this, &QAbstractButton::toggled, this, [this](bool on) {
                    select->animateTo(on ? 1.0 : 0.0);
                });
            }

            QString value; // "horizon" or #rrggbb
            M3::Variant variant = M3::Variant::TonalSpot;
            bool dark = true;

        protected:
            bool event(QEvent* e) override {
                if (e->type() == QEvent::HoverEnter) hover->animateTo(1.0);
                if (e->type() == QEvent::HoverLeave) hover->animateTo(0.0);
                return QAbstractButton::event(e);
            }

            void paintEvent(QPaintEvent*) override {
                QPainter p(this);
                p.setRenderHint(QPainter::Antialiasing);
                const M3::Scheme current = M3::currentScheme();
                const QColor seed = value == QStringLiteral("horizon") ? M3::horizonSeed(QTime::currentTime()) : QColor::fromString(value);
                const M3::Scheme s = M3::Scheme::generate(seed, dark, variant);
                const double k = select->currentValue().toDouble(); // the spring may overshoot: the morph follows
                const float shapeK = float(qBound(0.0, k, 1.0));
                static const m3shapes::Morph morph(MaterialShapes::circle(), MaterialShapes::cookie9Sided());
                const QPointF c = QRectF(this->rect()).center();

                p.translate(c);
                QColor layer = current.onSurface;
                layer.setAlphaF(0.08 * hover->currentValue().toDouble());
                p.setPen(Qt::NoPen);
                p.setBrush(layer);
                p.drawEllipse(QPointF(0, 0), 29, 29);
                p.rotate(40 * k);
                if (k > 0.01) {
                    QColor ring = current.onSurface;
                    ring.setAlphaF(qBound(0.0, k, 1.0));
                    p.setPen(QPen(ring, 2));
                    p.setBrush(Qt::NoBrush);
                    p.drawPath(m3shapes::toPath(morph, shapeK, QRectF(-28, -28, 56, 56)));
                    p.setPen(Qt::NoPen);
                }
                const double d = 46 + 2 * qBound(0.0, k, 1.2);
                const QPainterPath shape = m3shapes::toPath(morph, shapeK, QRectF(-d / 2, -d / 2, d, d));
                p.save();
                p.setClipPath(shape);
                p.fillRect(QRectF(-d / 2, -d / 2, d, d / 2), s.primary);
                p.fillRect(QRectF(-d / 2, 0, d / 2, d / 2), s.secondaryContainer);
                p.fillRect(QRectF(0, 0, d / 2, d / 2), s.tertiaryContainer);
                p.restore();
                p.rotate(-40 * k);
                if (value == QStringLiteral("horizon")) {
                    // The horizon's: a setting sun
                    QFont f(QStringLiteral("Material Symbols Rounded"));
                    f.setPixelSize(20);
                    f.setVariableAxis(QFont::Tag("FILL"), 1);
                    p.setFont(f);
                    p.setPen(s.onPrimary);
                    p.drawText(QRectF(-12, -16, 24, 24), Qt::AlignCenter, QStringLiteral("wb_twilight"));
                }
                if (this->hasFocus() && this->testAttribute(Qt::WA_KeyboardFocusChange)) {
                    p.setPen(QPen(current.secondary, 3));
                    p.setBrush(Qt::NoBrush);
                    p.drawEllipse(QPointF(0, 0), 29, 29);
                }
            }

        private:
            M3SpringAnimation* select;
            M3SpringAnimation* hover;
    };
    const m3shapes::RoundedPolygon& shapeNamed(const QString& name) {
        for (const auto& named : MaterialShapes::all()) {
            if (name.compare(QLatin1String(named.name), Qt::CaseInsensitive) == 0) return named.shape();
        }
        return MaterialShapes::circle();
    }

    // A live picture of the desktop in the chosen colours, like Android's
    // wallpaper and style preview: the wallpaper's shapes, the bar, a window
    class PreviewCard : public QWidget {
        public:
            explicit PreviewCard(QWidget* parent) :
                QWidget(parent) {
                this->setFixedHeight(200);
                auto* desk = new tSettings(QStringLiteral("theSuite"), QStringLiteral("theDesk"), this);
                QObject::connect(desk, &tSettings::settingChanged, this, [this](QString key) {
                    if (key == QStringLiteral("Wallpaper/shapes")) this->update();
                });
            }

        protected:
            void paintEvent(QPaintEvent*) override {
                QPainter p(this);
                p.setRenderHint(QPainter::Antialiasing);
                const M3::Scheme s = M3::currentScheme();
                const QRectF r(this->rect());
                QPainterPath clip;
                clip.addRoundedRect(r, 28, 28);
                p.setClipPath(clip);
                p.fillRect(r, s.surfaceContainerLowest);
                auto shape = [&](const m3shapes::RoundedPolygon& poly, QPointF at, double size, double turn, QColor colour) {
                    p.save();
                    p.translate(at);
                    p.rotate(turn);
                    colour.setAlphaF(0.7);
                    p.fillPath(m3shapes::toPath(poly, QRectF(-size / 2, -size / 2, size, size)), colour);
                    p.restore();
                };
                const QStringList picked = WallpaperView::shapeNames();
                shape(shapeNamed(picked.at(0)), r.bottomLeft() + QPointF(40, -10), 220, 12, s.primaryContainer);
                shape(shapeNamed(picked.at(1)), r.topRight() + QPointF(-30, 20), 180, -8, s.tertiaryContainer);
                const QRectF bar(r.center().x() - 120, r.top() + 12, 240, 26);
                p.setPen(Qt::NoPen);
                p.setBrush(s.surfaceContainer);
                p.drawRoundedRect(bar, 8, 8);
                shape(MaterialShapes::sunny(), QPointF(bar.left() + 16, bar.center().y()), 14, 0, s.primary);
                const QRectF window(r.center().x() - 170, r.top() + 54, 340, 130);
                p.setBrush(s.surfaceContainerLow);
                p.drawRoundedRect(window, 16, 16);
                QFont f = this->font();
                f.setPixelSize(22);
                f.setWeight(QFont::Bold);
                p.setFont(f);
                p.setPen(s.onSurface);
                p.drawText(QRectF(window.left() + 16, window.top() + 12, 200, 24), Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("Aa"));
                p.setPen(Qt::NoPen);
                p.setBrush(s.surfaceContainerHigh);
                for (int i = 0; i < 3; i++) p.drawRoundedRect(QRectF(window.left() + 16, window.top() + 48 + i * 18, 180 - i * 40, 10), 5, 5);
                p.setBrush(s.primaryContainer);
                p.drawEllipse(QPointF(window.right() - 40, window.top() + 30), 16, 16);
                const QRectF button(window.right() - 110, window.bottom() - 46, 94, 32);
                p.setBrush(s.primary);
                p.drawRoundedRect(button, 16, 16);
                f.setPixelSize(13);
                f.setWeight(QFont::DemiBold);
                p.setFont(f);
                p.setPen(s.onPrimary);
                p.drawText(button, Qt::AlignCenter, M3ColourSettings::tr("Button"));
                // A switch, on (SwitchTokens: 52 x 32, the thumb 24 with a check)
                const QRectF track(window.right() - 176, window.bottom() - 46, 52, 32);
                p.setPen(Qt::NoPen);
                p.setBrush(s.primary);
                p.drawRoundedRect(track, 16, 16);
                p.setBrush(s.onPrimary);
                p.drawEllipse(QPointF(track.right() - 16, track.center().y()), 12, 12);
                p.setPen(QPen(s.onPrimaryContainer, 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
                const QPointF o(track.right() - 16, track.center().y());
                p.drawPolyline(QPolygonF({o + QPointF(-5, 0), o + QPointF(-1.5, 3.5), o + QPointF(5, -3.5)}));
            }
    };
} // namespace

QWidget* M3ColourSettings::previewCard(QWidget* parent) {
    return new PreviewCard(parent);
}

struct M3ColourSettingsPrivate {
        tSettings* settings;
        tSettings* desk;
        QList<Swatch*> swatches;
        QComboBox* shapes[2];
        QComboBox* style;
        QComboBox* mode;
        QComboBox* contrast;
        QComboBox* density;
        bool loading = false;
};

M3ColourSettings::M3ColourSettings(QGridLayout* grid, int row, tSettings* settings, QWidget* parent) :
    QObject(parent) {
    d = new M3ColourSettingsPrivate();
    d->settings = settings;
    d->desk = new tSettings(QStringLiteral("theSuite"), QStringLiteral("theDesk"), this);

    auto* swatchRow = new QWidget(parent);
    swatchRow->setProperty("m3Tile", true); // one card of swatches
    auto* swatchLayout = new QHBoxLayout(swatchRow);
    swatchLayout->setContentsMargins(0, 10, 0, 10);
    swatchLayout->setSpacing(0);
    QStringList values = {QStringLiteral("horizon")};
    for (const QColor& c : presetSeeds()) values.append(c.name());
    for (const QString& value : values) {
        auto* swatch = new Swatch(value, swatchRow);
        swatch->setToolTip(value == QStringLiteral("horizon") ? tr("Time of day: the colour follows the clock") : value);
        if (!d->swatches.isEmpty()) swatchLayout->addStretch();
        swatchLayout->addWidget(swatch);
        d->swatches.append(swatch);
        connect(swatch, &QAbstractButton::clicked, this, [this, value] {
            if (!d->loading) d->settings->setValue(QStringLiteral("Palette/m3.seed"), value);
        });
    }

    d->style = new QComboBox(parent);
    for (const VariantName& v : variants()) d->style->addItem(tr(v.label), QString::fromLatin1(v.key));
    d->mode = new QComboBox(parent);
    d->mode->addItem(tr("Dark"), QStringLiteral("dark"));
    d->mode->addItem(tr("Light"), QStringLiteral("light"));
    d->mode->addItem(tr("Time of day (light by day)"), QStringLiteral("horizon"));
    d->contrast = new QComboBox(parent);
    d->contrast->addItem(tr("Standard"), 0.0);
    d->contrast->addItem(tr("Medium"), 0.5);
    d->contrast->addItem(tr("High"), 1.0);
    // The wallpaper's two shapes, as Pixel's shape wallpaper picks them
    auto* shapeRow = new QWidget(parent);
    auto* shapeLayout = new QHBoxLayout(shapeRow);
    shapeLayout->setContentsMargins(0, 0, 0, 0);
    shapeLayout->addStretch();
    for (QComboBox*& combo : d->shapes) {
        combo = new QComboBox(shapeRow);
        combo->setIconSize(QSize(24, 24));
        const QColor colour = M3::currentScheme().primary;
        const qreal dpr = parent->devicePixelRatioF();
        for (const auto& named : MaterialShapes::all()) {
            QPixmap icon(QSize(24, 24) * dpr);
            icon.setDevicePixelRatio(dpr);
            icon.fill(Qt::transparent);
            QPainter p(&icon);
            p.setRenderHint(QPainter::Antialiasing);
            p.fillPath(m3shapes::toPath(named.shape(), QRectF(2, 2, 20, 20)), colour);
            p.end();
            combo->addItem(QIcon(icon), QString::fromLatin1(named.name), QString::fromLatin1(named.name));
        }
        shapeLayout->addWidget(combo);
        connect(combo, &QComboBox::currentIndexChanged, this, [this] {
            if (!d->loading) d->desk->setValue(QStringLiteral("Wallpaper/shapes"), d->shapes[0]->currentData().toString() + QLatin1Char(',') + d->shapes[1]->currentData().toString());
        });
    }

    d->density = new QComboBox(parent);
    d->density->addItem(tr("Default"), 0);
    d->density->addItem(tr("Compact"), -1);
    d->density->addItem(tr("Dense"), -2);

    auto addRow = [&](int r, const QString& label, const QString& icon, QWidget* w) {
        auto* l = new QLabel(label, parent);
        l->setProperty("m3Icon", icon);
        grid->addWidget(l, r, 0);
        grid->addWidget(w, r, 1);
    };
    // Mode and Contrast as M3E connected button groups (as in Android's
    // settings); the combo boxes stay, hidden, as their model
    auto connectedGroup = [&](QComboBox* combo, const QStringList& labels, const QStringList& tooltips) {
        auto* group = new QWidget(parent);
        group->setProperty("m3ButtonGroup", QStringLiteral("connected"));
        auto* layout = new QHBoxLayout(group);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(2);
        combo->setParent(group);
        combo->hide();
        QList<QPushButton*> buttons;
        for (int i = 0; i < labels.size(); i++) {
            auto* button = new QPushButton(labels.at(i), group);
            button->setCheckable(true);
            button->setAutoExclusive(true);
            if (!tooltips.value(i).isEmpty()) button->setToolTip(tooltips.value(i));
            layout->addWidget(button);
            buttons.append(button);
            connect(button, &QPushButton::clicked, combo, [combo, i] {
                combo->setCurrentIndex(i);
            });
        }
        layout->insertStretch(0); // at the row's end
        auto sync = [combo, buttons] {
            if (QPushButton* b = buttons.value(combo->currentIndex())) b->setChecked(true);
        };
        connect(combo, &QComboBox::currentIndexChanged, group, sync);
        sync();
        return group;
    };

    grid->addWidget(swatchRow, row, 0, 1, grid->columnCount());
    addRow(++row, tr("Wallpaper shapes"), QStringLiteral("interests"), shapeRow);
    auto* appearance = new QLabel(tr("Appearance"), parent);
    appearance->setProperty("m3Header", true);
    grid->addWidget(appearance, row + 1, 0, 1, grid->columnCount());
    addRow(row + 2, tr("Mode"), QStringLiteral("dark_mode"), connectedGroup(d->mode, {tr("Dark"), tr("Light"), tr("Time of day")}, {QString(), QString(), tr("Light by day, dark at night")}));
    addRow(row + 3, tr("Contrast"), QStringLiteral("contrast"), connectedGroup(d->contrast, {tr("Standard"), tr("Medium"), tr("High")}, {}));
    addRow(row + 4, tr("Style"), QStringLiteral("style"), d->style);
    // M3 density for a mouse or touchpad: each step makes controls 4 px shorter (apps pick it up when they start)
    addRow(row + 5, tr("Density"), QStringLiteral("density_medium"), connectedGroup(d->density, {tr("Default"), tr("Compact"), tr("Dense")}, {QString(), tr("4 px shorter controls, for a mouse or touchpad"), tr("8 px shorter controls, for a mouse or touchpad")}));

    connect(d->style, &QComboBox::currentIndexChanged, this, [this] {
        if (!d->loading) d->settings->setValue(QStringLiteral("Palette/m3.variant"), d->style->currentData());
    });
    connect(d->mode, &QComboBox::currentIndexChanged, this, [this] {
        if (!d->loading) d->settings->setValue(QStringLiteral("Palette/m3.mode"), d->mode->currentData());
    });
    connect(d->contrast, &QComboBox::currentIndexChanged, this, [this] {
        if (!d->loading) d->settings->setValue(QStringLiteral("Palette/m3.contrast"), d->contrast->currentData());
    });
    connect(d->density, &QComboBox::currentIndexChanged, this, [this] {
        if (!d->loading) d->settings->setValue(QStringLiteral("Platform/density"), d->density->currentData());
    });
    connect(settings, &tSettings::settingChanged, this, [this](QString key) {
        if (key.startsWith(QStringLiteral("Palette/m3.")) || key == QStringLiteral("Platform/density")) load();
    });
    connect(d->desk, &tSettings::settingChanged, this, [this](QString key) {
        if (key == QStringLiteral("Wallpaper/shapes")) load();
    });
    load();
}

M3ColourSettings::~M3ColourSettings() {
    delete d;
}

void M3ColourSettings::load() {
    d->loading = true;
    const QString seed = d->settings->value(QStringLiteral("Palette/m3.seed")).toString().trimmed();
    const QString variant = d->settings->value(QStringLiteral("Palette/m3.variant")).toString().trimmed().toLower();
    const QString mode = d->settings->value(QStringLiteral("Palette/m3.mode")).toString().trimmed().toLower();
    const double contrast = d->settings->value(QStringLiteral("Palette/m3.contrast")).toDouble();

    bool found = false;
    for (Swatch* s : d->swatches) {
        s->variant = variantFor(variant);
        s->dark = mode != QStringLiteral("light");
        bool on = s->value.compare(seed.isEmpty() ? QStringLiteral("horizon") : seed, Qt::CaseInsensitive) == 0;
        if (on) found = true;
        s->setChecked(on);
        s->update();
    }
    if (!found && !d->swatches.isEmpty()) {
        // A custom seed from the config: show it as an extra swatch's value
        d->swatches.first()->setChecked(seed.isEmpty() || seed == QStringLiteral("horizon"));
    }
    int i = d->style->findData(variant.isEmpty() ? QStringLiteral("tonalspot") : variant);
    d->style->setCurrentIndex(qMax(0, i));
    i = d->mode->findData(mode.isEmpty() ? QStringLiteral("dark") : mode);
    d->mode->setCurrentIndex(qMax(0, i));
    d->contrast->setCurrentIndex(contrast >= 0.75 ? 2 : contrast >= 0.25 ? 1 : 0);
    d->density->setCurrentIndex(qBound(0, -d->settings->value(QStringLiteral("Platform/density")).toInt(), 2));
    const QStringList picked = WallpaperView::shapeNames();
    for (int s = 0; s < 2; s++) d->shapes[s]->setCurrentIndex(qMax(0, d->shapes[s]->findData(picked.at(s))));
    d->loading = false;
}
