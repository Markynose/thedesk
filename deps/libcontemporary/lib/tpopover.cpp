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
#include "tpopover.h"

#include "tcsdtools.h"
#include "tpropertyanimation.h"
#include "tscrim.h"
#include <QFrame>
#include <QGraphicsBlurEffect>
#include <QGraphicsOpacityEffect>
#include <QPainter>
#include <QPainterPath>
#include <QPointer>
#include <QStyle>

namespace {
    // theDesk's Material3 style answers this hint; popovers then become M3
    // sheets: a rounded surface behind the content (ShapeTokens
    // CornerExtraLarge 28 dp for bottom sheets, CornerLarge 16 dp for side
    // sheets), a drag handle on bottom sheets, and no separator line.
    constexpr auto M3StyleHint = QStyle::StyleHint(QStyle::SH_CustomBase + 0x4D33);

    class M3SheetBackdrop : public QWidget {
        public:
            M3SheetBackdrop(QWidget* content, int side, bool onRight, QWidget* parent) :
                QWidget(parent), content(content), side(side), onRight(onRight) {
                this->setAttribute(Qt::WA_TransparentForMouseEvents);
                content->installEventFilter(this);
                sync();
            }

        protected:
            bool eventFilter(QObject* watched, QEvent* event) override {
                if (watched == content) {
                    switch (event->type()) {
                        case QEvent::Move:
                        case QEvent::Resize:
                        case QEvent::Show:
                        case QEvent::Hide:
                        case QEvent::ZOrderChange:
                            sync();
                            break;
                        default:
                            break;
                    }
                }
                return false;
            }

            void paintEvent(QPaintEvent*) override {
                QPainter painter(this);
                painter.setRenderHint(QPainter::Antialiasing);
                QRectF r = this->rect();
                const double big = 28, small = 16;
                QPainterPath path;
                if (side == tPopover::Bottom) {
                    path.moveTo(r.left(), r.bottom() + 1);
                    path.lineTo(r.left(), r.top() + big);
                    path.arcTo(r.left(), r.top(), 2 * big, 2 * big, 180, -90);
                    path.lineTo(r.right() - big, r.top());
                    path.arcTo(r.right() - 2 * big, r.top(), 2 * big, 2 * big, 90, -90);
                    path.lineTo(r.right(), r.bottom() + 1);
                    path.closeSubpath();
                } else if (onRight) {
                    path.addRoundedRect(r.adjusted(0, 0, small, 0), small, small);
                } else {
                    path.addRoundedRect(r.adjusted(-small, 0, 0, 0), small, small);
                }
                painter.fillPath(path, (content ? content->palette() : this->palette()).color(QPalette::Window));
                if (side == tPopover::Bottom) {
                    // Drag handle: 32 x 4 dp, OnSurfaceVariant at 40%
                    QColor handle = this->palette().color(QPalette::PlaceholderText);
                    handle.setAlphaF(0.4);
                    QPainterPath h;
                    h.addRoundedRect(QRectF(r.center().x() - 16, r.top() + 8, 32, 4), 2, 2);
                    painter.fillPath(h, handle);
                }
            }

        private:
            void sync() {
                if (!content) return;
                this->setGeometry(content->geometry());
                this->setVisible(content->isVisible());
                this->stackUnder(content);
                this->update();
            }

            QPointer<QWidget> content;
            int side;
            bool onRight;
    };
} // namespace
#include <QPointer>
#include <QTimer>

#ifdef Q_OS_MAC
    #include <QBoxLayout>
    #include <QDialog>
#endif

struct tPopoverPrivate {
        QWidget* popoverWidget;
        QPointer<QWidget> parentWidget;
        QFrame* verticalSeperator;
        QPointer<QWidget> m3Sheet;

#ifdef Q_OS_MAC
        QDialog* wrapperDialog;
#endif

        tPopover::PopoverSide side = tPopover::Trailing;

        int width = -1;
        bool showing = false;
        bool performBlanking = true;
        bool dismissable = true;

        const int blankerOverscan = 100;

        static QMap<QWidget*, tPopover*> activePopovers;
};
QMap<QWidget*, tPopover*> tPopoverPrivate::activePopovers = QMap<QWidget*, tPopover*>();

tPopover::tPopover(QWidget* popoverWidget, QObject* parent) :
    QObject(parent) {
    d = new tPopoverPrivate();
    d->popoverWidget = popoverWidget;
    popoverWidget->setAutoFillBackground(true);

    d->verticalSeperator = new QFrame();
}

tPopover::~tPopover() {
    tPopoverPrivate::activePopovers.remove(d->popoverWidget);
    delete d;
}

bool tPopover::isOpeningOnRight() {
    return (QApplication::layoutDirection() == Qt::LeftToRight && d->side == Trailing) ||
           (QApplication::layoutDirection() == Qt::RightToLeft && d->side == Leading);
}

void tPopover::updateGeometry() {
    if (d->width == -1) {
        d->popoverWidget->resize(d->parentWidget->width(), d->parentWidget->height());
    } else if (d->width < 0) {
        if (d->side == Bottom) {
            d->popoverWidget->setGeometry(0, -d->width, d->parentWidget->width(), d->parentWidget->height() + d->width);
        } else if (isOpeningOnRight()) {
            d->popoverWidget->setGeometry(-d->width, 0, d->parentWidget->width() + d->width, d->parentWidget->height());
        } else {
            d->popoverWidget->setGeometry(0, 0, d->parentWidget->width() - d->width, d->parentWidget->height());
        }
    } else {
        if (d->side == Bottom) {
            d->popoverWidget->setGeometry(0, d->parentWidget->height() - d->width, d->parentWidget->width(), d->width);
        } else if (isOpeningOnRight()) {
            d->popoverWidget->setGeometry(d->parentWidget->width() - d->width, 0, d->width, d->parentWidget->height());
        } else {
            d->popoverWidget->setGeometry(0, 0, d->width, d->parentWidget->height());
        }
    }

    if (d->side == Bottom) {
        d->verticalSeperator->setGeometry(0, d->popoverWidget->y() - 1, d->parentWidget->width(), 1);
    } else if (isOpeningOnRight()) {
        d->verticalSeperator->setGeometry(d->popoverWidget->x() - 1, 0, 1, d->parentWidget->height());
    } else {
        d->verticalSeperator->setGeometry(d->popoverWidget->geometry().right() + 1, 0, 1, d->parentWidget->height());
    }
}

void tPopover::setPopoverWidth(int width) {
    if (d->showing) {
        int endValue;

        if (width >= 0) {
            // Normal width
            endValue = width;
        } else if (width == -1) {
            // Full Screen
            endValue = d->parentWidget->width();
        } else {
            // Based on parent width
            // Add because width will be negative
            endValue = d->parentWidget->width() + width;
        }

        tVariantAnimation* animation = new tVariantAnimation(this);
        animation->setStartValue(d->popoverWidget->width());
        animation->setEndValue(endValue);
        animation->setDuration(250);
        animation->setEasingCurve(QEasingCurve::OutCubic);
        connect(animation, &tVariantAnimation::valueChanged, this, [this](QVariant value) {
            d->width = value.toInt();
            this->updateGeometry();
        });
        connect(animation, &tVariantAnimation::finished, this, [this, width, animation] {
            d->width = width;
            this->updateGeometry();
            animation->deleteLater();
        });
        animation->start();
    } else {
        d->width = width;
    }
}

void tPopover::setPopoverSide(PopoverSide side) {
    d->side = side;
}

void tPopover::setPerformBlanking(bool performBlanking) {
    d->performBlanking = performBlanking;
}

void tPopover::setDismissable(bool dismissable) {
    d->dismissable = dismissable;
}

void tPopover::setPerformBlur(bool performBlur) {
    tScrim::scrimForWidget(d->parentWidget)->setBlurEnabled(performBlur);
}

QWidget* tPopover::parentWidget() {
    return d->parentWidget;
}

void tPopover::show(QWidget* parent) {
    auto scrimWidget = tScrim::scrimWidget(parent);
    parent = tCsdTools::widgetForPopover(scrimWidget);
    if (d->showing) return;
    tPopoverPrivate::activePopovers.insert(d->popoverWidget, this);

#ifdef Q_OS_MAC
    // Just show the popover as a sheet

    d->wrapperDialog = new QDialog(parent);
    d->wrapperDialog->resize(d->popoverWidget->size());

    QBoxLayout* l = new QBoxLayout(QBoxLayout::TopToBottom);
    l->setContentsMargins(0, 0, 0, 0);
    l->addWidget(d->popoverWidget);
    d->wrapperDialog->setLayout(l);

    d->wrapperDialog->setWindowModality(Qt::ApplicationModal); // Keyboard focus issues if this is window modal
    d->wrapperDialog->setWindowFlag(Qt::Sheet);
    d->wrapperDialog->open();
#else
    d->popoverWidget->setWindowFlags(Qt::Widget);
    d->parentWidget = parent;
    d->parentWidget->installEventFilter(this);

    d->verticalSeperator->hide();
    d->popoverWidget->hide();

    d->popoverWidget->setParent(parent);
    d->verticalSeperator->setParent(parent);

    // Material 3: a rounded sheet behind transparent content, no separator
    if (parent->style()->styleHint(M3StyleHint, nullptr, parent)) {
        d->popoverWidget->setAutoFillBackground(false);
        // M3 sheets are SurfaceContainerLow (the platform maps that to Base);
        // content that paints a Window background matches it
        QPalette sheetPalette = d->popoverWidget->palette();
        for (auto group : {QPalette::Active, QPalette::Inactive, QPalette::Disabled}) {
            sheetPalette.setColor(group, QPalette::Window, sheetPalette.color(group, QPalette::Base));
        }
        d->popoverWidget->setPalette(sheetPalette);
        if (d->m3Sheet) d->m3Sheet->deleteLater();
        d->m3Sheet = new M3SheetBackdrop(d->popoverWidget, d->side, isOpeningOnRight(), parent);
        d->verticalSeperator->setFixedSize(0, 0);
    }

    if (d->side == Bottom) {
        d->verticalSeperator->resize(parent->width(), 1);
        d->verticalSeperator->setFrameShape(QFrame::HLine);
    } else {
        d->verticalSeperator->resize(1, parent->height());
        d->verticalSeperator->setFrameShape(QFrame::VLine);
    }

    if (d->width == -1) {
        d->popoverWidget->resize(parent->width(), parent->height());
    } else {
        if (d->side == Bottom) {
            if (d->width < 0) {
                d->popoverWidget->resize(parent->width(), parent->height() + d->width);
            } else {
                d->popoverWidget->resize(parent->width(), d->width);
            }
        } else {
            if (d->width < 0) {
                d->popoverWidget->resize(parent->width() + d->width, parent->height());
            } else {
                d->popoverWidget->resize(d->width, parent->height());
            }
        }
    }

    if (d->performBlanking) {
        tScrim* scrim = tScrim::scrimForWidget(parent);
        connect(scrim, &tScrim::scrimClicked, this, [this] {
            if (d->dismissable) this->dismiss();
        });
        scrim->show();
    }

    tVariantAnimation* popoverAnim = new tVariantAnimation();
    if (d->side == Bottom) {
        popoverAnim->setStartValue(parent->height());
        popoverAnim->setEndValue(parent->height() - d->popoverWidget->height());
    } else if (isOpeningOnRight()) {
        popoverAnim->setStartValue(parent->width());
        popoverAnim->setEndValue(parent->width() - d->popoverWidget->width());
    } else {
        popoverAnim->setStartValue(-d->popoverWidget->width());
        popoverAnim->setEndValue(0);
    }
    popoverAnim->setDuration(250);
    popoverAnim->setEasingCurve(QEasingCurve::OutCubic);
    connect(popoverAnim, &tVariantAnimation::valueChanged, [this](QVariant value) {
        if (d->side == Bottom) {
            d->popoverWidget->move(0, value.toInt());
            d->verticalSeperator->move(0, value.toInt() - 1);
        } else if (isOpeningOnRight()) {
            d->popoverWidget->move(value.toInt(), 0);
            d->verticalSeperator->move(value.toInt() - 1, 0);
        } else {
            d->popoverWidget->move(value.toInt(), 0);
            d->verticalSeperator->move(value.toInt() + d->popoverWidget->width(), 0);
        }
    });
    connect(popoverAnim, &tVariantAnimation::finished, popoverAnim, &tVariantAnimation::deleteLater);
    popoverAnim->start();
    popoverAnim->valueChanged(popoverAnim->startValue());

    d->verticalSeperator->show();
    d->popoverWidget->show();

    d->verticalSeperator->raise();
    d->popoverWidget->raise();

    d->popoverWidget->setFocus();
#endif

    d->showing = true;
}

void tPopover::dismiss() {
    if (!d->showing) return;

#ifdef Q_OS_MAC
    // d->popoverWidget->hide();
    d->wrapperDialog->close();
    d->wrapperDialog->deleteLater();
    d->showing = false;
    emit dismissed();
#else
    if (d->performBlanking) {
        tScrim* scrim = tScrim::scrimForWidget(d->parentWidget);
        scrim->disconnect(this);
        scrim->hide();
    }

    tVariantAnimation* popoverAnim = new tVariantAnimation();
    if (d->side == Bottom) {
        // Opening on the bottom
        popoverAnim->setStartValue(d->popoverWidget->y());
        popoverAnim->setEndValue(d->parentWidget->height());
    } else if (isOpeningOnRight()) {
        // Opening on the right
        popoverAnim->setStartValue(d->popoverWidget->x());
        popoverAnim->setEndValue(d->parentWidget->width());
    } else {
        // Opening on the left
        popoverAnim->setStartValue(d->popoverWidget->x());
        popoverAnim->setEndValue(-d->parentWidget->width());
    }
    popoverAnim->setDuration(250);
    popoverAnim->setEasingCurve(QEasingCurve::OutCubic);
    connect(popoverAnim, &tVariantAnimation::valueChanged, [this](QVariant value) {
        if (d->side == Bottom) {
            d->popoverWidget->move(0, value.toInt());
            d->verticalSeperator->move(0, value.toInt() - 1);
        } else if (isOpeningOnRight()) {
            d->popoverWidget->move(value.toInt(), 0);
            d->verticalSeperator->move(value.toInt() - 1, 0);
        } else {
            d->popoverWidget->move(value.toInt(), 0);
            d->verticalSeperator->move(value.toInt() + d->popoverWidget->width(), 0);
        }
    });
    connect(popoverAnim, &tVariantAnimation::finished, [this] {
        d->popoverWidget->hide();
        d->verticalSeperator->hide();

        if (d->parentWidget) {
            d->parentWidget->removeEventFilter(this);
        }
        d->parentWidget = nullptr;

        tPopoverPrivate::activePopovers.remove(d->popoverWidget);

        d->showing = false;
        emit dismissed();
    });
    popoverAnim->start();
#endif
}

bool tPopover::eventFilter(QObject* watched, QEvent* event) {
    if (watched == d->parentWidget) {
        if (event->type() == QEvent::Resize) {
            if (d->showing) {
                updateGeometry();
            }
        }
    }
    return false;
}

tPopover* tPopover::popoverForWidget(QWidget* popoverWidget) {
    auto scrimWidget = tScrim::scrimWidget(popoverWidget);
    return tPopoverPrivate::activePopovers.value(scrimWidget, nullptr);
}

tPopover* tPopover::popoverForPopoverWidget(QWidget* popoverWidget) {
    QWidget* checkWidget = popoverWidget;
    while (checkWidget) {
        for (tPopover* popover : tPopoverPrivate::activePopovers.values()) {
            if (popover->d->popoverWidget == checkWidget) return popover;
        }
        checkWidget = checkWidget->parentWidget();
    }
    return nullptr;
}
