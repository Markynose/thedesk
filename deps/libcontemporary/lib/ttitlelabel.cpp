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
#include "ttitlelabel.h"

#include "ticon.h"
#include <QApplication>
#include <QMetaMethod>
#include <QPainter>
#include <QToolButton>

struct tTitleLabelPrivate {
        QToolButton* backButton;
        bool backButtonShown = false;
        bool backButtonIsMenu = false;
        bool drawBottomBorder = true;
};

tTitleLabel::tTitleLabel(QWidget* parent) :
    QLabel(parent) {
    d = new tTitleLabelPrivate();

    d->backButton = new QToolButton(this);
    d->backButton->setIcon(tIcon::fromTheme("go-previous", QIcon(":/the-libs/icons/go-previous.svg")));
    d->backButton->setAutoRaise(true);
    d->backButton->move(0, 0);
    d->backButton->setFixedHeight(this->height() - 1);
    d->backButton->setFixedWidth(d->backButton->sizeHint().width());
    d->backButton->setVisible(false);
    connect(d->backButton, &QToolButton::clicked, this, &tTitleLabel::backButtonClicked);

    updateFont();
    updateDirection();

    this->setMargin(9);
    if (libContemporaryCommon::isMaterial3()) {
        // theDesk M3: a small top app bar (TitleLarge, round back button, no rule)
        d->drawBottomBorder = false;
        this->setMargin(12);
        this->setMinimumHeight(56);
    }
}

tTitleLabel::~tTitleLabel() {
    delete d;
}

QSize tTitleLabel::sizeHint() const {
    // TODO: Account for actions
    QSize labelSize = QLabel::sizeHint();
    labelSize.setHeight(qMax(labelSize.height(), d->backButton->sizeHint().height()));
    labelSize.rheight()++;
    return labelSize;
}

bool tTitleLabel::backButtonShown() {
    return d->backButtonShown;
}

void tTitleLabel::setBackButtonShown(bool backButtonShown) {
    d->backButtonShown = backButtonShown;

    d->backButton->setVisible(backButtonShown);
    if (this->layoutDirection() == Qt::RightToLeft) {
        this->setContentsMargins(0, 0, backButtonShown ? d->backButton->width() : 0, 0);
    } else {
        this->setContentsMargins(backButtonShown ? d->backButton->width() : 0, 0, 0, 0);
    }

    emit backButtonShownChanged(backButtonShown);
}

bool tTitleLabel::backButtonIsMenu() {
    return d->backButtonIsMenu;
}

void tTitleLabel::setBackButtonIsMenu(bool backButtonIsMenu) {
    d->backButtonIsMenu = backButtonIsMenu;
    if (backButtonIsMenu) {
        d->backButton->setIcon(tIcon::fromTheme("application-menu"));
    } else {
        d->backButton->setIcon(tIcon::fromTheme("go-previous"));
    }

    emit backButtonIsMenuChanged(backButtonIsMenu);
}

bool tTitleLabel::drawBottomBorder() {
    return d->drawBottomBorder;
}

void tTitleLabel::setDrawBottomBorder(bool drawBottomBorder) {
    d->drawBottomBorder = drawBottomBorder;
    emit drawBottomBorderChanged(drawBottomBorder);

    this->update();
}

void tTitleLabel::updateFont() {
    QFont font = qApp->font(this);
    if (libContemporaryCommon::isMaterial3()) {
        font.setPixelSize(22); // TitleLarge
        font.setWeight(QFont::Normal);
        this->setFont(font);
        return;
    }
    font.setPointSizeF(font.pointSizeF() * 1.5);
    this->setFont(font);
}

void tTitleLabel::updateDirection() {
    this->setAlignment(Qt::AlignVCenter | Qt::AlignAbsolute | (this->layoutDirection() == Qt::LeftToRight ? Qt::AlignLeft : Qt::AlignRight));
}

void tTitleLabel::paintEvent(QPaintEvent* event) {
    QLabel::paintEvent(event);

    if (d->drawBottomBorder) {
        QPainter painter(this);
        painter.setPen(libContemporaryCommon::lineColor(this->palette().color(QPalette::WindowText)));
        painter.drawLine(0, this->height() - 1, this->width(), this->height() - 1);
    }
}

void tTitleLabel::resizeEvent(QResizeEvent* event) {
    if (libContemporaryCommon::isMaterial3()) {
        // A round 40 dp icon button, centred vertically, 8 dp in
        d->backButton->setFixedSize(40, 40);
        int y = (this->height() - 40) / 2;
        d->backButton->move(this->layoutDirection() == Qt::RightToLeft ? this->width() - 48 : 8, y);
        int inset = d->backButtonShown ? 52 : 0;
        if (this->layoutDirection() == Qt::RightToLeft) this->setContentsMargins(0, 0, inset, 0);
        else this->setContentsMargins(inset, 0, 0, 0);
        return;
    }
    d->backButton->setFixedHeight(this->height() - 1);

    if (this->layoutDirection() == Qt::RightToLeft) {
        d->backButton->move(this->width() - d->backButton->width(), 0);
    }
}

void tTitleLabel::connectNotify(const QMetaMethod& signal) {
    if (signal == QMetaMethod::fromSignal(&tTitleLabel::backButtonClicked)) {
        this->setBackButtonShown(true);
    }
}

bool tTitleLabel::event(QEvent* event) {
    if (event->type() == QEvent::ApplicationFontChange) {
        updateFont();
    } else if (event->type() == QEvent::LanguageChange || event->type() == QEvent::LayoutDirectionChange) {
        updateDirection();
    }

    return QLabel::event(event);
}
