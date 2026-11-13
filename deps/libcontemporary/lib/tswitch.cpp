/*******************************************************************************
 *
 * libcontemporary - Common libraries
 * Copyright (C) 2022 Victor Tran
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 ******************************************************************************/

#include "tswitch.h"
#include <QPaintEvent>
#include <QPainter>
#include <QStaticText>
#include <QStyle>
#include <QStyleOptionButton>
#include <tvariantanimation.h>

namespace {
    // theDesk's Material 3 style (style-m3) draws M3 switches itself when it
    // answers this style hint; any other style gets the classic tSwitch.
    constexpr auto M3StyleHint = QStyle::StyleHint(QStyle::SH_CustomBase + 0x4D33);
    constexpr auto PE_M3Switch = QStyle::PrimitiveElement(QStyle::PE_CustomBase + 0x4D33);

    bool m3Style(const QWidget* widget) {
        return widget->style()->styleHint(M3StyleHint, nullptr, widget);
    }
} // namespace

tSwitch::tSwitch(QWidget* parent) :
    QPushButton(parent) {
    this->setCheckable(true);
    this->setFixedSize(this->sizeHint());

    innerRect = QRect(0, 0, this->width(), this->height());
}

QPalette::ColorGroup tSwitch::IsActiveColorRole() {
    if (this->isEnabled()) {
        return QPalette::Active;
    } else {
        return QPalette::Disabled;
    }
}

void tSwitch::paintEvent(QPaintEvent* event) {
    if (m3Style(this)) {
        // While dragging, tell the style where the handle is (0 = off, 1 = on)
        int travel = this->width() - innerRect.width();
        this->setProperty("m3DragPosition", this->isDown() && travel > 0 ? double(innerRect.x()) / travel : -1.0);

        QPainter painter(this);
        QStyleOptionButton option;
        option.initFrom(this);
        if (this->isChecked()) option.state |= QStyle::State_On;
        if (this->isDown()) option.state |= QStyle::State_Sunken;
        option.icon = this->iIcon;
        this->style()->drawPrimitive(PE_M3Switch, &option, &painter, this);
        return;
    }

    QPainter painter(this);

    painter.setFont(this->font());
    QFontMetrics metrics(this->font());

    painter.setBrush(this->palette().brush(IsActiveColorRole(), QPalette::Highlight));
    painter.setPen(this->palette().color(IsActiveColorRole(), QPalette::WindowText));
    painter.drawRect(0, 0, innerRect.left(), this->height());

    painter.setBrush(this->palette().brush(IsActiveColorRole(), QPalette::HighlightedText));
    painter.drawRect(innerRect);

    if (iText == "") {
        painter.drawPixmap(innerRect.left() - 10 - this->height() / 2, (this->height() / 2 - 16 / 2), 16, 16, this->iIcon.pixmap(16, 16));
    } else {
        painter.drawStaticText(innerRect.left() - metrics.horizontalAdvance(iText) - this->height() / 2, (this->height() / 2 - metrics.height() / 2), QStaticText(iText));
    }
    painter.setBrush(this->palette().brush(IsActiveColorRole(), QPalette::WindowText));
    painter.drawStaticText(innerRect.right() + this->height() / 2, (this->height() / 2 - metrics.height() / 2), QStaticText(oText));

    painter.setPen(this->palette().color(IsActiveColorRole(), QPalette::WindowText));
    painter.setBrush(QBrush(Qt::transparent));
    painter.drawRect(0, 0, this->width() - 1, this->height() - 1);
}

void tSwitch::mousePressEvent(QMouseEvent* event) {
    mouseClickPoint = event->position().toPoint().x();
    initialPoint = mouseClickPoint;
}

void tSwitch::mouseMoveEvent(QMouseEvent* event) {
    if (event->position().toPoint().x() < mouseClickPoint) {
        mouseMovedLeft = true;
    } else {
        mouseMovedLeft = false;
    }

    innerRect.translate(event->position().toPoint().x() - mouseClickPoint, 0);
    if (innerRect.x() <= 0) {
        innerRect.moveTo(0, 0);
    } else if (innerRect.right() >= this->width()) {
        innerRect.moveTo(this->width() - innerRect.width(), 0);
    }

    mouseClickPoint = event->position().toPoint().x();
    this->repaint();
}

void tSwitch::mouseReleaseEvent(QMouseEvent* event) {
    if (initialPoint - 2 < mouseClickPoint && initialPoint + 2 > mouseClickPoint) {
        bool checked = !this->isChecked();
        this->setChecked(checked);
    } else {
        if (mouseMovedLeft) {
            this->setChecked(false);
        } else {
            this->setChecked(true);
        }
    }
}

void tSwitch::checkChanging(bool checked) {
    tVariantAnimation* animation = new tVariantAnimation(this);
    animation->setStartValue(innerRect);
    animation->setDuration(250);

    if (checked) {
        animation->setEndValue(QRect(this->width() - this->height(), 0, this->height(), this->height()));
    } else {
        animation->setEndValue(QRect(0, 0, this->height(), this->height()));
    }

    animation->setEasingCurve(QEasingCurve::OutCubic);
    connect(animation, &tVariantAnimation::valueChanged, this, [this](QVariant value) {
        innerRect = value.toRect();
        this->update();
    });
    connect(animation, &tVariantAnimation::finished, this, [this, animation] {
        innerRect = animation->endValue().toRect();
        this->update();
    });
    connect(animation, &tVariantAnimation::finished, animation, &QObject::deleteLater);
    animation->start();
}

QSize tSwitch::sizeHint() const {
    // M3 switch: 52x32 track plus room for the 40 px state layer
    if (m3Style(this)) return QSize(60, 40);

    QFontMetrics metrics(this->font());
    int width = 33;
    if (iText == "") {
        width += 8;
    } else {
        width += metrics.horizontalAdvance(iText);
    }

    width += metrics.horizontalAdvance(oText);
    return QSize(width, 22);
}

QString tSwitch::OnText() {
    return iText;
}

QString tSwitch::OffText() {
    return oText;
}

void tSwitch::setOnText(QString text) {
    iText = text;
    this->iIcon = QIcon();
    this->setFixedSize(this->sizeHint());
    this->update();
}

void tSwitch::setOffText(QString text) {
    oText = text;
    this->setFixedSize(this->sizeHint());
    this->update();
}

void tSwitch::setOnIcon(QIcon icon) {
    this->iIcon = icon;
    iText = "";
    this->setFixedSize(this->sizeHint());
    this->update();
}

QIcon tSwitch::OnIcon() {
    return this->iIcon;
}

void tSwitch::setChecked(bool checked) {
    QPushButton::setChecked(checked);
    this->checkChanging(this->isChecked());
}
