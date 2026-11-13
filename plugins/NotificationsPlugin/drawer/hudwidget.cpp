/****************************************
 *
 *   INSERT-PROJECT-NAME-HERE - INSERT-GENERIC-NAME-HERE
 *   Copyright (C) 2020 Victor Tran
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
#include "hudwidget.h"
#include "levelhud.h"
#include <QPushButton>

#include <QPainterPath>
#include <m3scheme.h>
#include <shapes/materialshapes.h>
#include <shapes/shapepath.h>

#include <m3motion.h>
#include "ui_hudwidget.h"

#include <statemanager.h>
#include <hudmanager.h>
#include <QIcon>
#include <QGraphicsOpacityEffect>
#include <tvariantanimation.h>
#include <QTimer>
#include <QPainter>

namespace {
    struct SnackbarColours {
            QColor container, content, accent, over;
    };

    // A dark snackbar in either scheme: the dark scheme's highest container,
    // or the light scheme's InverseSurface (which is dark there)
    SnackbarColours snackbarColours() {
        M3::Scheme s = M3::currentScheme();
        if (s.surface.lightnessF() < 0.5) return {s.surfaceContainerHighest, s.onSurface, s.primary, s.tertiary};
        return {s.inverseSurface, s.inverseOnSurface, s.inversePrimary, s.tertiaryContainer};
    }

    // The snackbar itself, in the Expressive manner: a pill, its icon in a
    // MaterialShape, the title over the text, and a tonal action. (M3's own
    // snackbar is a 4 dp rectangle with one line; the user chose this.)
    class SnackbarCard : public QWidget {
        public:
            explicit SnackbarCard(QWidget* parent) :
                QWidget(parent) {
                action = new QPushButton(this);
                action->setCursor(Qt::PointingHandCursor);
                action->setProperty("m3ButtonStyle", QStringLiteral("tonal"));
                QFont label(QStringLiteral("Roboto Flex"));
                label.setPixelSize(14);
                label.setVariableAxis(QFont::Tag("wght"), 560);
                action->setFont(label);
                action->setVisible(false);
            }

            QPixmap icon;            // drawn in the shape's content colour...
            bool ownColours = false; // ...unless it's a picture of its own (a logo)
            QColor accent;           // which then tints the shape
            bool warning = false;
            QString title, text;
            QPushButton* action;
            double pop = 1; // the shape's entrance: 0.6 to 1, with its bounce

            bool twoLines() const {
                return !title.isEmpty() && !text.isEmpty();
            }
            int shapeSize() const {
                return twoLines() ? 36 : 32;
            }
            int lead() const {
                if (icon.isNull()) return 22;
                const int inset = (this->cardHeight() - this->shapeSize()) / 2;
                return inset + this->shapeSize() + 12;
            }
            int trail() const {
                return action->isHidden() ? 24 : 12 + action->sizeHint().width() + 12;
            }
            int cardHeight() const {
                return twoLines() ? 60 : 48;
            }
            QFont titleFont() const {
                QFont font(QStringLiteral("Roboto Flex"));
                font.setPixelSize(twoLines() ? 15 : 14);
                font.setVariableAxis(QFont::Tag("wght"), twoLines() ? 650 : 500);
                if (twoLines()) font.setVariableAxis(QFont::Tag("wdth"), 105);
                return font;
            }
            QFont bodyFont() const {
                QFont font(QStringLiteral("Roboto Flex"));
                font.setPixelSize(twoLines() ? 13 : 14);
                font.setVariableAxis(QFont::Tag("wght"), 400);
                return font;
            }
            QSize sizeHint() const override {
                const int words = qMax(QFontMetrics(titleFont()).horizontalAdvance(title), QFontMetrics(bodyFont()).horizontalAdvance(text));
                return QSize(lead() + words + 2 + trail(), cardHeight());
            }

        protected:
            void resizeEvent(QResizeEvent*) override {
                const QSize size = action->sizeHint();
                action->setGeometry(this->width() - 12 - size.width(), (this->height() - size.height()) / 2, size.width(), size.height());
            }

            void paintEvent(QPaintEvent*) override {
                const SnackbarColours colours = snackbarColours();
                const M3::Scheme s = M3::currentScheme();
                QPainter p(this);
                p.setRenderHint(QPainter::Antialiasing);
                p.setRenderHint(QPainter::SmoothPixmapTransform);
                const double radius = this->height() / 2.0;
                p.setPen(Qt::NoPen);
                p.setBrush(colours.container);
                p.drawRoundedRect(QRectF(this->rect()), radius, radius);

                if (!icon.isNull()) {
                    const double size = shapeSize();
                    const double inset = (this->height() - size) / 2;
                    const QRectF box(inset, inset, size, size);
                    const QColor shape = warning ? s.errorContainer : ownColours ? M3::layer(colours.container, accent, 0.22) : s.primaryContainer;
                    const QColor content = warning ? s.onErrorContainer : s.onPrimaryContainer;
                    p.save();
                    p.translate(box.center());
                    p.rotate((pop - 1) * 60);
                    p.scale(pop, pop);
                    p.translate(-box.center());
                    p.fillPath(m3shapes::toPath(MaterialShapes::cookie9Sided(), box), shape);
                    p.restore();

                    const double glyph = 20;
                    const QRectF at(box.center().x() - glyph / 2, box.center().y() - glyph / 2, glyph, glyph);
                    if (ownColours) {
                        p.drawPixmap(at, icon, QRectF(icon.rect()));
                    } else {
                        QPixmap tinted = icon;
                        QPainter tint(&tinted);
                        tint.setCompositionMode(QPainter::CompositionMode_SourceIn);
                        tint.fillRect(tinted.rect(), content);
                        tint.end();
                        p.drawPixmap(at, tinted, QRectF(tinted.rect()));
                    }
                }

                const int left = lead();
                const int width = this->width() - left - trail();
                QColor quiet = colours.content;
                quiet.setAlphaF(0.78);
                if (twoLines()) {
                    p.setFont(titleFont());
                    p.setPen(colours.content);
                    p.drawText(QRect(left, 10, width + 2, 22), Qt::AlignLeft | Qt::AlignVCenter, p.fontMetrics().elidedText(title, Qt::ElideRight, width + 2));
                    p.setFont(bodyFont());
                    p.setPen(quiet);
                    p.drawText(QRect(left, 31, width + 2, 20), Qt::AlignLeft | Qt::AlignVCenter, p.fontMetrics().elidedText(text, Qt::ElideRight, width + 2));
                } else {
                    const bool isTitle = !title.isEmpty();
                    p.setFont(isTitle ? titleFont() : bodyFont());
                    p.setPen(colours.content);
                    p.drawText(QRect(left, 0, width + 2, this->height()), Qt::AlignLeft | Qt::AlignVCenter, p.fontMetrics().elidedText(isTitle ? title : text, Qt::ElideRight, width + 2));
                }
            }
    };
} // namespace

struct HudWidgetPrivate {
    SnackbarCard* card = nullptr;
    M3SpringAnimation* pop = nullptr;
    QTimer* hideTimer;
    LevelHud* levels = nullptr;
    QPushButton* action = nullptr;
    QString actionId;
    bool shouldShow = false;
    bool risesFromBelow = false;

    int state = 0;
    double value = 0;
    QColor color;
};

HudWidget::HudWidget(QWidget* parent) :
    QWidget(parent),
    ui(new Ui::HudWidget) {
    ui->setupUi(this);
    d = new HudWidgetPrivate();

    this->setFixedHeight(0);
    // Snackbar titles keep their own type (not M3 section headers)
    ui->iconValueTitle->setProperty("m3NoHeader", true);
    ui->iconTextTitle->setProperty("m3NoHeader", true);

    // Snackbar look, always dark (user's choice over M3's InverseSurface,
    // which is light in a dark scheme); the level as heavy Roboto Flex
    auto applyColours = [this] {
        SnackbarColours s = snackbarColours();
        for (QWidget* w : {static_cast<QWidget*>(ui->iconValuePage), static_cast<QWidget*>(ui->iconTextPage)}) {
            QPalette pal = w->palette();
            for (auto group : {QPalette::Active, QPalette::Inactive, QPalette::Disabled}) {
                pal.setColor(group, QPalette::WindowText, s.content);
                pal.setColor(group, QPalette::Text, s.content);
            }
            w->setPalette(pal);
        }
    };
    applyColours();
    QFont valueFont = ui->iconValueValue->font();
    valueFont.setPixelSize(22);
    valueFont.setVariableAxis(QFont::Tag("wght"), 850);
    valueFont.setVariableAxis(QFont::Tag("wdth"), 115);
    ui->iconValueValue->setFont(valueFont);
    ui->stackedWidget->setAutoFillBackground(false);

    d->hideTimer = new QTimer(this);
    d->hideTimer->setInterval(3000);
    d->hideTimer->setSingleShot(true);
    connect(d->hideTimer, &QTimer::timeout, this, &HudWidget::animateHide);

    // Levels (volume, brightness) are the M3E slider at the screen's edge;
    // everything else is an M3 snackbar here
    d->levels = new LevelHud();

    // The snackbar is its own widget now; the old pages stay unseen
    ui->stackedWidget->hide();
    d->card = new SnackbarCard(this);
    d->action = d->card->action;
    connect(d->action, &QPushButton::clicked, this, [this] {
        StateManager::hudManager()->triggerAction(d->actionId);
        this->animateHide();
    });
    d->pop = new M3SpringAnimation(this);
    d->pop->setToken(M3Motion::SpatialFast);
    d->pop->snapTo(1.0);
    connect(d->pop, &M3SpringAnimation::valueChanged, this, [this](const QVariant& value) {
        d->card->pop = value.toDouble();
        d->card->update();
    });

    connect(StateManager::hudManager(), &HudManager::requestHud, this, [ = ](QVariantMap params) {
        // A level on its own (volume, brightness) is the slider; a message
        // that happens to carry a level (battery charging, the suspend
        // countdown) stays a snackbar, the level as text
        if (params.contains(QStringLiteral("value")) && params.value(QStringLiteral("text")).toString().isEmpty()) {
            d->levels->showLevel(params);
            return;
        }
        QPixmap icon = QIcon::fromTheme(params.value("icon", "").toString()).pixmap(SC_DPI_T(QSize(32, 32), QSize));
        QString title = params.value("title", "").toString().toUpper();
        QString text = params.value("text", "").toString();
        double value = params.value("value", 0).toDouble();
        int timeout = params.value("timeout", 3000).toInt();
        QColor color = params.value("color", QColor(Qt::white)).value<QColor>();

        ui->iconValueIcon->setPixmap(icon);
        ui->iconTextIcon->setPixmap(icon);
        ui->iconValueTitle->setText(title);
        ui->iconTextTitle->setText(title);
        {
            // The title over the text; a level that rides on a message
            // (battery charging) joins the text
            QString message = text;
            if (params.contains(QStringLiteral("value")) && params.value(QStringLiteral("icon")).toString().contains(QStringLiteral("battery"))) {
                message += QStringLiteral(" \u00b7 %1%").arg(qRound(params.value(QStringLiteral("value")).toDouble() * 100));
            }
            const QString plainTitle = params.value("title", "").toString();
            const QString iconName = params.value(QStringLiteral("icon")).toString();
            d->card->title = plainTitle;
            d->card->text = message;
            // A picture of the caller's own (a logo), or a themed icon
            const QPixmap own = params.value(QStringLiteral("pixmap")).value<QPixmap>();
            d->card->ownColours = !own.isNull();
            d->card->accent = params.value(QStringLiteral("accent")).value<QColor>();
            d->card->icon = own.isNull() ? QIcon::fromTheme(iconName).pixmap(QSize(20, 20), this->devicePixelRatioF()) : own;
            d->card->warning = own.isNull() && (iconName.contains(QStringLiteral("warning")) || iconName.contains(QStringLiteral("error")));
            const QString action = params.value(QStringLiteral("action")).toString();
            d->actionId = params.value(QStringLiteral("actionId")).toString();
            d->action->setText(action);
            d->action->setVisible(!action.isEmpty());
            this->placeCard();
            d->card->update();
        }
        ui->iconValueValue->setText(QLocale().toString(value * 100, 'f', 0) + "%");

        d->hideTimer->setInterval(timeout);

        color.setAlpha(50);
        d->color = color;

        d->value = value;
        // Already up: one line may have become two (or the reverse)
        if (d->state == 2) {
            this->setFixedHeight(d->card->sizeHint().height() + SC_DPI(9));
            emit shouldShowChanged();
        }

        this->animateShow();
    }, Qt::QueuedConnection);
    connect(StateManager::hudManager(), &HudManager::requestHideHud, this, [ = ] {
        //Immediately hide the HUD
        this->animateHide();
    });

    ui->iconValuePage->installEventFilter(this);
    ui->iconTextPage->installEventFilter(this);
}

HudWidget::~HudWidget() {
    if (d->levels) d->levels->deleteLater();
    delete d;
    delete ui;
}

void HudWidget::setRisesFromBelow(bool rises) {
    d->risesFromBelow = rises;
}

QRect HudWidget::cardRect() const {
    return d->card->geometry().intersected(this->rect());
}

int HudWidget::preferredWidth() const {
    return d->card->sizeHint().width() + SC_DPI(18);
}

void HudWidget::placeCard() {
    const QSize hint = d->card->sizeHint();
    const int width = qMin(hint.width(), this->width() - SC_DPI(18));
    d->card->setGeometry((this->width() - width) / 2, d->risesFromBelow ? 0 : this->height() - hint.height(), width, hint.height());
}

bool HudWidget::shouldShow() {
    return d->shouldShow;
}

void HudWidget::resizeEvent(QResizeEvent* event) {
    Q_UNUSED(event)
    this->placeCard();
}

bool HudWidget::eventFilter(QObject* watched, QEvent* event) {
    if (event->type() == QEvent::Paint && (watched == ui->iconValuePage || watched == ui->iconTextPage)) {
        // Snackbar shape (SnackbarTokens: CornerExtraSmall), dark container
        QWidget* w = static_cast<QWidget*>(watched);
        SnackbarColours s = snackbarColours();
        QPainter painter(w);
        painter.setRenderHint(QPainter::Antialiasing);
        QPainterPath container;
        container.addRoundedRect(QRectF(w->rect()), 4, 4);
        painter.fillPath(container, s.container);

        if (watched == ui->iconValuePage) {
            // The level: an M3 linear indicator along the bottom edge, with
            // the 4 dp gap and stop dot (LinearProgressIndicatorTokens)
            const double inset = 16, thickness = 4, gap = 4;
            QRectF track(inset, w->height() - 10 - thickness, w->width() - 2 * inset, thickness);
            double value = qBound(0.0, d->value, 1.0);
            double end = track.width() * value;
            auto pill = [&](double from, double to, QColor colour) {
                if (to - from < 0.5) return;
                QPainterPath p;
                p.addRoundedRect(QRectF(track.left() + from, track.top(), to - from, thickness), thickness / 2, thickness / 2);
                painter.fillPath(p, colour);
            };
            QColor faint = s.content;
            faint.setAlphaF(0.24);
            pill(0, end, d->value > 1 ? s.over : s.accent);
            pill(end > 0 ? end + gap : 0, track.width(), faint);
            if (end + gap < track.width() - thickness) pill(track.width() - thickness, track.width(), s.accent);
        }
        return true;
    }
    return false;
}

void HudWidget::animateShow() {
    //Make sure we're idle
    if (d->state != 0) {
        if (d->state == 2) {
            d->hideTimer->stop();
            d->hideTimer->start();
        }
        return;
    }
    d->state = 1;

    d->hideTimer->stop();

    d->shouldShow = true;
    emit shouldShowChanged();

    tVariantAnimation* anim = new tVariantAnimation(this);
    anim->setStartValue(0);
    anim->setEndValue(d->card->sizeHint().height() + SC_DPI(9));
    // The icon's shape turns and grows into place as the pill rises
    if (M3Motion::expressive()) {
        d->pop->snapTo(0.6);
        d->pop->animateTo(1.0);
    }
    anim->setEasingCurve(M3Motion::easing(M3Motion::SpatialDefault));
    anim->setDuration(M3Motion::duration(M3Motion::SpatialDefault));
    connect(anim, &tVariantAnimation::valueChanged, this, [ = ](QVariant value) {
        this->setFixedHeight(value.toInt());
        emit shouldShowChanged();
    });
    connect(anim, &tVariantAnimation::finished, this, [ = ] {
        anim->deleteLater();
        d->hideTimer->start();
        d->state = 2;
    });
    anim->start();
}

void HudWidget::animateHide() {
    if (d->state != 2) return; //Make sure we're showing
    d->state = 3;

    d->hideTimer->stop();

    tVariantAnimation* anim = new tVariantAnimation(this);
    anim->setStartValue(this->height());
    anim->setEndValue(0);
    anim->setEasingCurve(M3Motion::easing(M3Motion::SpatialDefault));
    anim->setDuration(M3Motion::duration(M3Motion::SpatialDefault));
    connect(anim, &tVariantAnimation::valueChanged, this, [ = ](QVariant value) {
        this->setFixedHeight(value.toInt());
        emit shouldShowChanged();
    });
    connect(anim, &tVariantAnimation::finished, this, [ = ] {
        anim->deleteLater();

        d->state = 0;
        d->shouldShow = false;
        emit shouldShowChanged();
    });
    anim->start();
}
