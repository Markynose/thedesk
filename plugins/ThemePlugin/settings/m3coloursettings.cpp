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
#include <tsettings.h>

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

    // One swatch: a circle in three of its scheme's colours, a ring and a
    // check when chosen, springs on hover and selection
    class Swatch : public QAbstractButton {
        public:
            Swatch(QString value, QWidget* parent) :
                QAbstractButton(parent), value(std::move(value)) {
                this->setCheckable(true);
                this->setAutoExclusive(true);
                this->setFixedSize(52, 52);
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
                const double sel = qBound(0.0, select->currentValue().toDouble(), 1.2);
                const QRectF r = QRectF(this->rect()).adjusted(2, 2, -2, -2);

                // Container: a tonal square that rounds into a circle when chosen
                QPainterPath box;
                double corner = 14 + (r.height() / 2 - 14) * qMin(1.0, sel);
                box.addRoundedRect(r, corner, corner);
                p.fillPath(box, current.surfaceContainerHighest);
                QColor layer = current.onSurface;
                layer.setAlphaF(0.08 * hover->currentValue().toDouble());
                p.fillPath(box, layer);

                // The swatch: top half primary, bottom-left secondary, bottom-right tertiary
                const QRectF c = r.adjusted(8, 8, -8, -8);
                p.setPen(Qt::NoPen);
                p.setBrush(s.primary);
                p.drawPie(c, 0, 180 * 16);
                p.setBrush(s.secondaryContainer);
                p.drawPie(c, 180 * 16, 90 * 16);
                p.setBrush(s.tertiaryContainer);
                p.drawPie(c, 270 * 16, 90 * 16);

                if (value == QStringLiteral("horizon")) {
                    // The horizon's: a small sun on its rim
                    QPainterPath sun;
                    sun.addEllipse(QPointF(c.center().x(), c.center().y()), 5, 5);
                    p.fillPath(sun, s.onPrimary);
                }
                if (sel > 0.01) {
                    // A check in a Primary dot springs in
                    double rad = 9 * qMin(1.2, sel);
                    QPainterPath dot;
                    dot.addEllipse(c.center(), rad, rad);
                    p.fillPath(dot, current.primary);
                    p.setPen(QPen(current.onPrimary, 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
                    QPointF o = c.center();
                    QPainterPath check;
                    check.moveTo(o + QPointF(-4, 0) * sel);
                    check.lineTo(o + QPointF(-1, 3) * sel);
                    check.lineTo(o + QPointF(4.5, -3) * sel);
                    p.drawPath(check);
                }
                if (this->hasFocus() && this->testAttribute(Qt::WA_KeyboardFocusChange)) {
                    p.setPen(QPen(current.secondary, 2));
                    p.setBrush(Qt::NoBrush);
                    p.drawPath(box);
                }
            }

        private:
            M3SpringAnimation* select;
            M3SpringAnimation* hover;
    };
} // namespace

struct M3ColourSettingsPrivate {
        tSettings* settings;
        QList<Swatch*> swatches;
        QComboBox* style;
        QComboBox* mode;
        QComboBox* contrast;
        bool loading = false;
};

M3ColourSettings::M3ColourSettings(QGridLayout* grid, int row, tSettings* settings, QWidget* parent) :
    QObject(parent) {
    d = new M3ColourSettingsPrivate();
    d->settings = settings;

    auto* swatchRow = new QWidget(parent);
    auto* swatchLayout = new QHBoxLayout(swatchRow);
    swatchLayout->setContentsMargins(0, 4, 0, 4);
    swatchLayout->setSpacing(6);
    QStringList values = {QStringLiteral("horizon")};
    for (const QColor& c : presetSeeds()) values.append(c.name());
    for (const QString& value : values) {
        auto* swatch = new Swatch(value, swatchRow);
        swatch->setToolTip(value == QStringLiteral("horizon") ? tr("Living horizon: follows the time of day") : value);
        swatchLayout->addWidget(swatch);
        d->swatches.append(swatch);
        connect(swatch, &QAbstractButton::clicked, this, [this, value] {
            if (!d->loading) d->settings->setValue(QStringLiteral("Palette/m3.seed"), value);
        });
    }
    swatchLayout->addStretch();

    d->style = new QComboBox(parent);
    for (const VariantName& v : variants()) d->style->addItem(tr(v.label), QString::fromLatin1(v.key));
    d->mode = new QComboBox(parent);
    d->mode->addItem(tr("Dark"), QStringLiteral("dark"));
    d->mode->addItem(tr("Light"), QStringLiteral("light"));
    d->mode->addItem(tr("Follow the horizon (light by day)"), QStringLiteral("horizon"));
    d->contrast = new QComboBox(parent);
    d->contrast->addItem(tr("Standard"), 0.0);
    d->contrast->addItem(tr("Medium"), 0.5);
    d->contrast->addItem(tr("High"), 1.0);

    auto addRow = [&](int r, const QString& label, QWidget* w) {
        auto* l = new QLabel(label, parent);
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
        layout->addStretch();
        auto sync = [combo, buttons] {
            if (QPushButton* b = buttons.value(combo->currentIndex())) b->setChecked(true);
        };
        connect(combo, &QComboBox::currentIndexChanged, group, sync);
        sync();
        return group;
    };

    addRow(row, tr("Colour"), swatchRow);
    addRow(row + 1, tr("Style"), d->style);
    addRow(row + 2, tr("Mode"), connectedGroup(d->mode, {tr("Dark"), tr("Light"), tr("Horizon")}, {QString(), QString(), tr("Follow the horizon: light by day, dark at night")}));
    addRow(row + 3, tr("Contrast"), connectedGroup(d->contrast, {tr("Standard"), tr("Medium"), tr("High")}, {}));

    connect(d->style, &QComboBox::currentIndexChanged, this, [this] {
        if (!d->loading) d->settings->setValue(QStringLiteral("Palette/m3.variant"), d->style->currentData());
    });
    connect(d->mode, &QComboBox::currentIndexChanged, this, [this] {
        if (!d->loading) d->settings->setValue(QStringLiteral("Palette/m3.mode"), d->mode->currentData());
    });
    connect(d->contrast, &QComboBox::currentIndexChanged, this, [this] {
        if (!d->loading) d->settings->setValue(QStringLiteral("Palette/m3.contrast"), d->contrast->currentData());
    });
    connect(settings, &tSettings::settingChanged, this, [this](QString key) {
        if (key.startsWith(QStringLiteral("Palette/m3."))) load();
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
    d->loading = false;
}
