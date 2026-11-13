/****************************************
 *
 *   theDesk - how to use a newly connected display
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 * *************************************/
#include "displaychooser.h"

#include <QBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <m3motion.h>
#include <m3scheme.h>

namespace {
    QFont flex(double px, double wght, double wdth = 100) {
        QFont font(QStringLiteral("Roboto Flex"));
        font.setPixelSize(qRound(px));
        font.setVariableAxis(QFont::Tag("opsz"), float(qBound(8.0, px, 144.0)));
        font.setVariableAxis(QFont::Tag("wght"), float(wght));
        font.setVariableAxis(QFont::Tag("wdth"), float(wdth));
        return font;
    }

    QColor withAlpha(QColor colour, double alpha) {
        colour.setAlphaF(float(colour.alphaF() * alpha));
        return colour;
    }

    // A laptop (screen and a flat base) or a monitor (screen and a stand),
    // its screen lit or not; returns the screen's rectangle
    QRectF drawDevice(QPainter& p, QRectF box, bool laptop, bool lit, QColor ink, QColor glass) {
        QRectF screen;
        QPainterPath body;
        if (laptop) {
            screen = QRectF(box.left() + box.width() * 0.1, box.top(), box.width() * 0.8, box.height() * 0.78);
            body.addRoundedRect(screen, 4, 4);
            QRectF base(box.left(), screen.bottom() + box.height() * 0.06, box.width(), box.height() * 0.12);
            body.addRoundedRect(base, base.height() / 2, base.height() / 2);
        } else {
            screen = QRectF(box.left(), box.top(), box.width(), box.height() * 0.76);
            body.addRoundedRect(screen, 4, 4);
            QRectF neck(box.center().x() - box.width() * 0.05, screen.bottom(), box.width() * 0.1, box.height() * 0.14);
            body.addRect(neck);
            QRectF foot(box.center().x() - box.width() * 0.2, neck.bottom(), box.width() * 0.4, box.height() * 0.08);
            body.addRoundedRect(foot, foot.height() / 2, foot.height() / 2);
        }
        if (lit) {
            p.fillPath(body, ink);
            QPainterPath face;
            face.addRoundedRect(screen.adjusted(3, 3, -3, -3), 2, 2);
            p.fillPath(face, glass);
        } else {
            // Off: just its outline, faint
            p.setPen(QPen(withAlpha(ink, 0.45), 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            p.drawPath(body);
            p.setPen(Qt::NoPen);
        }
        return screen.adjusted(3, 3, -3, -3);
    }
} // namespace

struct DisplayChoiceTilePrivate {
        DisplayChoiceTile::Kind kind;
        bool laptop;
        bool recommended;
        M3SpringAnimation* hover;
        M3SpringAnimation* press;
};

DisplayChoiceTile::DisplayChoiceTile(Kind kind, bool laptop, QString label, bool recommended, QWidget* parent) :
    QAbstractButton(parent) {
    d = new DisplayChoiceTilePrivate();
    d->kind = kind;
    d->laptop = laptop;
    d->recommended = recommended;
    this->setText(label);
    this->setFocusPolicy(Qt::StrongFocus);
    this->setAttribute(Qt::WA_Hover);
    this->setProperty("m3HoverMerge", false); // it paints its own states
    this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    this->setFixedHeight(184);

    d->hover = new M3SpringAnimation(this);
    d->hover->setToken(M3Motion::EffectsFast);
    d->hover->snapTo(0.0);
    d->press = new M3SpringAnimation(this);
    d->press->setToken(M3Motion::SpatialFast);
    d->press->snapTo(0.0);
    for (auto* spring : {d->hover, d->press}) connect(spring, &M3SpringAnimation::valueChanged, this, qOverload<>(&QWidget::update));
    connect(this, &QAbstractButton::pressed, this, [this] {
        d->press->animateTo(1.0);
    });
    connect(this, &QAbstractButton::released, this, [this] {
        d->press->animateTo(0.0);
    });
}

DisplayChoiceTile::~DisplayChoiceTile() {
    delete d;
}

QSize DisplayChoiceTile::sizeHint() const {
    return QSize(176, 184);
}

bool DisplayChoiceTile::event(QEvent* event) {
    switch (event->type()) {
        case QEvent::HoverEnter:
            d->hover->animateTo(1.0);
            break;
        case QEvent::HoverLeave:
            d->hover->animateTo(0.0);
            d->press->animateTo(0.0);
            break;
        default:
            break;
    }
    return QAbstractButton::event(event);
}

void DisplayChoiceTile::keyPressEvent(QKeyEvent* event) {
    switch (event->key()) {
        case Qt::Key_Return:
        case Qt::Key_Enter:
            this->animateClick();
            return;
        case Qt::Key_Left:
        case Qt::Key_Up:
            this->focusPreviousChild();
            return;
        case Qt::Key_Right:
        case Qt::Key_Down:
            this->focusNextChild();
            return;
        default:
            QAbstractButton::keyPressEvent(event);
    }
}

void DisplayChoiceTile::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event)
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const M3::Scheme s = M3::currentScheme();

    const QColor container = d->recommended ? s.primaryContainer : s.surfaceContainerHighest;
    const QColor content = d->recommended ? s.onPrimaryContainer : s.onSurface;

    // Corners: ExtraLarge, squishing to Medium while pressed
    const QRectF r = QRectF(this->rect()).adjusted(2, 2, -2, -2);
    const double press = qBound(-0.2, d->press->currentValue().toDouble(), 1.2);
    const double hover = qBound(0.0, d->hover->currentValue().toDouble(), 1.2);
    const double corner = qBound(8.0, 28 + (12 - 28) * press, r.height() / 2);
    QPainterPath shape;
    shape.addRoundedRect(r, corner, corner);
    p.fillPath(shape, container);
    p.fillPath(shape, withAlpha(content, 0.08 * hover + 0.10 * qBound(0.0, press, 1.0)));
    if (this->hasFocus()) {
        p.setPen(QPen(s.secondary, 2));
        p.drawPath(shape);
        p.setPen(Qt::NoPen);
    }

    // The drawing: the main screen (laptop or monitor) and the new display
    const bool mainLit = d->kind != OnlyOther;
    const bool otherLit = d->kind != OnlyMain;
    const QColor ink = content;
    const QColor glass = d->recommended ? s.primary : s.surfaceContainerLow;
    const QColor sun = d->recommended ? s.onPrimary : s.primary;
    const double top = r.top() + 30;
    const QRectF mainBox(r.center().x() - 70, top + 12, 62, 50);
    const QRectF otherBox(r.center().x() + 2, top, 72, 64);
    const QRectF mainFace = drawDevice(p, mainBox, d->laptop, mainLit, ink, glass);
    const QRectF otherFace = drawDevice(p, otherBox, false, otherLit, ink, glass);

    // What's on them: one sun on each lit screen; extending, a single sun
    // across the gap between them
    auto drawSun = [&](QPointF centre, double radius, QRectF clip) {
        p.save();
        p.setClipRect(clip);
        QPainterPath dot;
        dot.addEllipse(centre, radius, radius);
        p.fillPath(dot, sun);
        p.restore();
    };
    switch (d->kind) {
        case Extend: {
            // A horizon running through both: the sun sets in one, rises in the other
            const double y = (mainFace.center().y() + otherFace.center().y()) / 2 + 4;
            drawSun(QPointF(mainFace.right() + 4, y), 13, mainFace);
            drawSun(QPointF(otherFace.left() - 4, y), 13, otherFace);
            break;
        }
        case Mirror:
            drawSun(mainFace.center(), 8, mainFace);
            drawSun(otherFace.center(), 10, otherFace);
            break;
        case OnlyMain:
            drawSun(mainFace.center(), 9, mainFace);
            break;
        case OnlyOther:
            drawSun(otherFace.center(), 11, otherFace);
            break;
    }

    // Label: Roboto Flex that thickens and widens as you hover, and squeezes
    // as you press
    const double squeeze = qBound(0.0, press, 1.0);
    p.setPen(content);
    p.setFont(flex(16, 600 + 250 * hover, 100 + 14 * hover - 12 * squeeze));
    const QRectF text(r.left() + 14, r.bottom() - 62, r.width() - 28, 48);
    p.drawText(text, Qt::AlignCenter | Qt::TextWordWrap, this->text());
}

struct DisplayChooserPrivate {
        QList<DisplayChoiceTile*> tiles;
};

DisplayChooser::DisplayChooser(QString otherName, bool laptop, QWidget* parent) :
    QWidget(parent) {
    d = new DisplayChooserPrivate();
    const M3::Scheme s = M3::currentScheme();

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(8);

    auto* title = new QLabel(tr("A display is connected"), this);
    title->setFont(flex(28, 600));
    QPalette titlePalette = title->palette();
    titlePalette.setColor(QPalette::WindowText, s.onSurface);
    title->setPalette(titlePalette);
    title->setProperty("m3NoHeader", true);
    layout->addWidget(title);

    auto* body = new QLabel(otherName.isEmpty() ? tr("Choose how to use it. theDesk remembers, so next time it's set up the same way.")
                                                : tr("Choose how to use %1. theDesk remembers, so next time it's set up the same way.").arg(otherName),
        this);
    body->setFont(flex(16, 400));
    body->setWordWrap(true);
    QPalette bodyPalette = body->palette();
    bodyPalette.setColor(QPalette::WindowText, s.onSurfaceVariant);
    body->setPalette(bodyPalette);
    layout->addWidget(body);
    layout->addSpacing(16);

    auto* row = new QHBoxLayout();
    row->setSpacing(12);
    const QString onlyOther = otherName.isEmpty() ? tr("Only the new display") : tr("Only %1").arg(otherName);
    const QList<QPair<DisplayChoiceTile::Kind, QString>> kinds = {
        {DisplayChoiceTile::Extend,    tr("Extend")                                                    },
        {DisplayChoiceTile::Mirror,    tr("Mirror")                                                    },
        {DisplayChoiceTile::OnlyMain,  laptop ? tr("Only the laptop") : tr("Only the main screen")},
        {DisplayChoiceTile::OnlyOther, onlyOther                                                       },
    };
    for (const auto& [kind, label] : kinds) {
        auto* tile = new DisplayChoiceTile(kind, laptop, label, kind == DisplayChoiceTile::Extend, this);
        connect(tile, &QAbstractButton::clicked, this, [this, kind] {
            emit chosen(int(kind));
        });
        row->addWidget(tile);
        d->tiles.append(tile);
    }
    layout->addLayout(row);
}

DisplayChooser::~DisplayChooser() {
    delete d;
}

int DisplayChooser::preferredHeight() const {
    return this->sizeHint().height() + 16;
}

QRect DisplayChooser::tileRect(int choice) const {
    if (choice < 0 || choice >= d->tiles.count()) return QRect();
    DisplayChoiceTile* tile = d->tiles.at(choice);
    return QRect(tile->mapToGlobal(QPoint(0, 0)), tile->size());
}

void DisplayChooser::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    // Extend is ready for Enter
    if (!d->tiles.isEmpty()) d->tiles.first()->setFocus();
}
