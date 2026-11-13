/****************************************
 *
 *   INSERT-PROJECT-NAME-HERE - INSERT-GENERIC-NAME-HERE
 *   Copyright (C) 2022 Victor Tran
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * *************************************/
#include "keyboardlayoutchunk.h"

#include <QEvent>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <Wm/desktopwm.h>
#include <barmanager.h>
#include <m3motion.h>
#include <m3scheme.h>
#include <statemanager.h>
#include <tsettings.h>

namespace {
    constexpr int Height = 26;
    constexpr int Lead = 8, Symbol = 18, Gap = 6, Trail = 11;

    QFont label(const QFont& base) {
        QFont font = base;
        font.setFamily(QStringLiteral("Roboto Flex"));
        font.setPixelSize(13);
        font.setVariableAxis(QFont::Tag("wght"), 620);
        font.setLetterSpacing(QFont::AbsoluteSpacing, 0.6);
        return font;
    }

    // "us(dvorak)" shows as US: the menu and the tooltip carry the full name
    QString code(const QString& layout) {
        return layout.section('(', 0, 0).section(':', 0, 0).toUpper();
    }

    M3SpringAnimation* spring(QObject* parent, M3Motion::Token token, double from) {
        auto* animation = new M3SpringAnimation(parent);
        animation->setToken(token);
        animation->snapTo(from);
        return animation;
    }
} // namespace

struct KeyboardLayoutChunk::Private {
        tSettings settings;
        QMenu* menu;
        QList<QAction*> layoutsActions;
        BarManager::BarLockPtr barLocker;

        QString shown, leaving;
        int leavingDirection = 1;
        M3SpringAnimation* roll;  // 0 → 1 as the new code takes the old one's place
        M3SpringAnimation* width; // the text's width, so the pill springs to fit
        M3SpringAnimation* layer; // hover/press/open state layer opacity
        bool hovered = false;
};

KeyboardLayoutChunk::KeyboardLayoutChunk() :
    Chunk() {
    d = new Private();
    d->menu = new QMenu(this);
    this->setCursor(Qt::PointingHandCursor);
    this->setAttribute(Qt::WA_Hover);
    this->installEventFilter(this);

    d->roll = spring(this, M3Motion::SpatialDefault, 1.0);
    d->width = spring(this, M3Motion::SpatialFast, 0.0);
    d->layer = spring(this, M3Motion::EffectsFast, 0.0);
    connect(d->roll, &M3SpringAnimation::valueChanged, this, [this] {
        this->update();
    });
    connect(d->width, &M3SpringAnimation::valueChanged, this, [this] {
        this->updateGeometry();
        this->update();
    });
    connect(d->layer, &M3SpringAnimation::valueChanged, this, [this] {
        this->update();
    });

    connect(&d->settings, &tSettings::settingChanged, this, [this](QString key) {
        if (key == "Input/keyboard.layouts") updateLayouts();
    });
    updateLayouts();

    connect(DesktopWm::instance(), &DesktopWm::currentKeyboardLayoutChanged, this, &KeyboardLayoutChunk::updateCurrentLayout);
    updateCurrentLayout();

    connect(d->menu, &QMenu::aboutToShow, this, [this] {
        d->barLocker = StateManager::barManager()->acquireLock();
    });
    connect(d->menu, &QMenu::aboutToHide, this, [this] {
        d->barLocker->unlock();
        d->layer->animateTo(d->hovered ? 0.08 : 0.0);
    });
    connect(this, &Chunk::clicked, this, [this] {
        d->layer->animateTo(0.12);
        d->menu->popup(this->mapToGlobal(QPoint(0, this->height() + 6)));
    });
}

KeyboardLayoutChunk::~KeyboardLayoutChunk() {
    delete d;
}

void KeyboardLayoutChunk::updateLayouts() {
    for (QAction* action : d->layoutsActions) {
        d->menu->removeAction(action);
        action->deleteLater();
    }
    d->layoutsActions.clear();

    d->layoutsActions.append(d->menu->addSection(tr("Select Keyboard Layout")));
    for (QString layout : d->settings.delimitedList("Input/keyboard.layouts")) {
        QAction* action = new QAction(this);

        action->setText(DesktopWm::keyboardLayoutDescription(layout));
        action->setCheckable(true);

        connect(DesktopWm::instance(), &DesktopWm::currentKeyboardLayoutChanged, action, [=] {
            action->setChecked(layout == DesktopWm::currentKeyboardLayout());
        });
        action->setChecked(layout == DesktopWm::currentKeyboardLayout());

        connect(action, &QAction::triggered, this, [=] {
            DesktopWm::setCurrentKeyboardLayout(layout);
        });

        d->menu->addAction(action);
        d->layoutsActions.append(action);
    }
}

void KeyboardLayoutChunk::updateCurrentLayout() {
    const QString layout = DesktopWm::currentKeyboardLayout();
    const QString next = code(layout);
    this->setToolTip(DesktopWm::keyboardLayoutDescription(layout));
    const QFontMetrics metrics(label(this->font()));
    if (d->shown.isEmpty()) {
        d->shown = next;
        d->width->snapTo(double(metrics.horizontalAdvance(next)));
        this->updateGeometry();
        return;
    }
    if (next == d->shown) return;

    // Later layouts in the list roll up, earlier ones down, like a drum
    const QStringList layouts = d->settings.delimitedList("Input/keyboard.layouts");
    const auto index = [&](const QString& c) {
        for (int i = 0; i < layouts.size(); i++) if (code(layouts.at(i)) == c) return i;
        return -1;
    };
    d->leavingDirection = index(next) >= index(d->shown) ? 1 : -1;
    d->leaving = d->shown;
    d->shown = next;
    d->roll->snapTo(0.0);
    d->roll->animateTo(1.0);
    d->width->animateTo(double(metrics.horizontalAdvance(next)));
}

QSize KeyboardLayoutChunk::sizeHint() const {
    return QSize(Lead + Symbol + Gap + qMax(0, qRound(d->width->currentValue().toDouble())) + Trail, Height);
}

bool KeyboardLayoutChunk::eventFilter(QObject* watched, QEvent* event) {
    if (watched == this) {
        switch (event->type()) {
            case QEvent::HoverEnter:
                d->hovered = true;
                if (!d->menu->isVisible()) d->layer->animateTo(0.08);
                break;
            case QEvent::HoverLeave:
                d->hovered = false;
                if (!d->menu->isVisible()) d->layer->animateTo(0.0);
                break;
            case QEvent::MouseButtonPress:
                d->layer->animateTo(0.12);
                break;
            default:
                break;
        }
    }
    return Chunk::eventFilter(watched, event);
}

void KeyboardLayoutChunk::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event)
    const M3::Scheme s = M3::currentScheme();
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QRectF pill(0, (this->height() - Height) / 2.0, this->width(), Height);
    QPainterPath shape;
    shape.addRoundedRect(pill, Height / 2.0, Height / 2.0);

    p.setPen(Qt::NoPen);
    p.setBrush(s.secondaryContainer);
    p.drawPath(shape);
    const double layer = qBound(0.0, d->layer->currentValue().toDouble(), 1.0);
    if (layer > 0) {
        QColor state = s.onSecondaryContainer;
        state.setAlphaF(layer);
        p.setBrush(state);
        p.drawPath(shape);
    }

    QFont symbol(QStringLiteral("Material Symbols Rounded"));
    symbol.setPixelSize(Symbol);
    symbol.setVariableAxis(QFont::Tag("FILL"), 1);
    symbol.setVariableAxis(QFont::Tag("wght"), 500);
    symbol.setVariableAxis(QFont::Tag("opsz"), 20);
    p.setFont(symbol);
    p.setPen(s.onSecondaryContainer);
    p.drawText(QRectF(Lead, pill.top(), Symbol, Height), Qt::AlignCenter, QStringLiteral("keyboard"));

    // The codes roll inside the pill; opacity follows the roll but never
    // past its ends (only the movement may overshoot)
    p.setClipPath(shape);
    p.setFont(label(this->font()));
    const double roll = d->roll->currentValue().toDouble();
    const double travel = Height * 0.7;
    const QRectF text(Lead + Symbol + Gap, pill.top(), this->width(), Height);
    auto draw = [&](const QString& c, double offset, double opacity) {
        if (opacity <= 0) return;
        QColor colour = s.onSecondaryContainer;
        colour.setAlphaF(qBound(0.0, opacity, 1.0));
        p.setPen(colour);
        p.drawText(text.translated(0, offset), Qt::AlignVCenter | Qt::AlignLeft, c);
    };
    if (roll < 1 && !d->leaving.isEmpty()) draw(d->leaving, -roll * travel * d->leavingDirection, 1 - roll * 1.6);
    draw(d->shown, (1 - roll) * travel * d->leavingDirection, roll < 1 ? roll * 1.6 - 0.6 : 1);
}

QString KeyboardLayoutChunk::name() {
    return "keyboardlayout";
}

int KeyboardLayoutChunk::expandedHeight() {
    return Height + 12;
}

int KeyboardLayoutChunk::statusBarHeight() {
    return Height;
}
