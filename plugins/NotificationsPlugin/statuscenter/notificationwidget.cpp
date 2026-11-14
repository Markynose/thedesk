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
#include "notificationwidget.h"
#include "ui_notificationwidget.h"

#include "pebble.h"
#include <QDateTime>
#include <QHBoxLayout>
#include <QPainter>
#include <QPushButton>
#include <QToolButton>

struct NotificationWidgetPrivate {
    NotificationPtr notification;
    QToolButton* dismissButton;
    QWidget* actions = nullptr;
    QDateTime posted = QDateTime::currentDateTime();
    double dealt = 1;
};

NotificationWidget::NotificationWidget(NotificationPtr notification, QWidget* parent) :
    QWidget(parent),
    ui(new Ui::NotificationWidget) {
    ui->setupUi(this);

    d = new NotificationWidgetPrivate();
    d->notification = notification;

    // A card in its app's deck: title (and when, at the end), text, actions
    const M3::Scheme s = M3::currentScheme();
    ui->verticalLayout->setContentsMargins(16, 12, 16, 12);
    ui->verticalLayout->setSpacing(2);
    auto recolour = [](QLabel* label, const QFont& font, const QColor& colour) {
        label->setFont(font);
        QPalette pal = label->palette();
        for (auto group : {QPalette::Active, QPalette::Inactive}) pal.setColor(group, QPalette::WindowText, colour);
        label->setPalette(pal);
    };
    recolour(ui->summaryLabel, Pebble::flex(15, 600), s.onSurface);
    recolour(ui->bodyLabel, Pebble::flex(14, 400), s.onSurfaceVariant);
    ui->summaryLabel->setWordWrap(true);
    ui->summaryLabel->setContentsMargins(0, 0, 52, 0); // when it came, or the close button

    connect(notification, &Notification::summaryChanged, this, [ = ](QString summary) {
        ui->summaryLabel->setText(summary);
    });
    connect(notification, &Notification::bodyChanged, this, [ = ](QString body) {
        ui->bodyLabel->setText(body);
        ui->bodyLabel->setVisible(!body.isEmpty());
    });
    ui->summaryLabel->setText(notification->summary());
    ui->bodyLabel->setText(notification->body());
    ui->bodyLabel->setVisible(!notification->body().isEmpty());

    d->dismissButton = new QToolButton(this);
    d->dismissButton->setIcon(QIcon::fromTheme("window-close"));
    d->dismissButton->setAutoRaise(true);
    d->dismissButton->setProperty("m3ButtonSize", QStringLiteral("xs"));
    d->dismissButton->setProperty("m3IconButtonStyle", QStringLiteral("tonal"));
    d->dismissButton->setToolTip(tr("Dismiss"));
    d->dismissButton->setFixedSize(d->dismissButton->sizeHint());
    connect(d->dismissButton, &QToolButton::clicked, this, &NotificationWidget::closeRequested);
    d->dismissButton->hide();

    // Its actions, as in the pop-up (a pairing code needs its Accept here
    // too: the pop-up may never have shown)
    connect(notification, &Notification::actionsChanged, this, &NotificationWidget::setupActions);
    setupActions();
}

void NotificationWidget::setupActions() {
    if (d->actions) {
        ui->verticalLayout->removeWidget(d->actions);
        d->actions->deleteLater();
        d->actions = nullptr;
    }
    const QList<Notification::Action> actions = d->notification->actions();
    if (actions.isEmpty()) return;
    d->actions = new QWidget(this);
    d->actions->setProperty("m3ButtonGroup", QStringLiteral("connected"));
    auto* layout = new QHBoxLayout(d->actions);
    layout->setContentsMargins(0, 8, 0, 0);
    layout->setSpacing(2);
    for (const Notification::Action& action : actions) {
        auto* button = new QPushButton(d->actions);
        button->setText(action.text);
        button->setIcon(action.icon);
        button->setProperty("m3ButtonSize", QStringLiteral("xs"));
        button->setProperty("m3ButtonStyle", QStringLiteral("tonal"));
        connect(button, &QPushButton::clicked, this, [this, action] {
            emit d->notification->actionInvoked(action);
            // Answered: it can't be answered twice
            if (d->actions) d->actions->setEnabled(false);
        });
        layout->addWidget(button);
    }
    ui->verticalLayout->addWidget(d->actions, 0, Qt::AlignLeft);
}

NotificationWidget::~NotificationWidget() {
    delete d;
    delete ui;
}

void NotificationWidget::setDealt(double dealt) {
    if (qFuzzyCompare(dealt, d->dealt)) return;
    d->dealt = dealt;
    this->update();
}

void NotificationWidget::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event)
    const M3::Scheme s = M3::currentScheme();
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    // tucked under the top card it's a shade lighter
    const double t = qBound(0.0, d->dealt, 1.0);
    const QColor a = s.surfaceContainerHighest, b = s.surfaceContainerHigh;
    QPainterPath card;
    card.addRoundedRect(QRectF(this->rect()), 20, 20);
    p.fillPath(card, QColor::fromRgbF(a.redF() + (b.redF() - a.redF()) * t, a.greenF() + (b.greenF() - a.greenF()) * t, a.blueF() + (b.blueF() - a.blueF()) * t));
    if (d->dismissButton->isVisible()) return;
    // when it came
    const qint64 secs = d->posted.secsTo(QDateTime::currentDateTime());
    const QString when = secs < 60 ? tr("now") : secs < 3600 ? tr("%n min", nullptr, int(secs / 60)) : QLocale().toString(d->posted.time(), QLocale::ShortFormat);
    p.setFont(Pebble::flex(12, 500));
    p.setPen(s.onSurfaceVariant);
    p.drawText(QRectF(this->width() - 16 - 60, 12, 60, 22), Qt::AlignRight | Qt::AlignVCenter, when);
}

void NotificationWidget::resizeEvent(QResizeEvent* event) {
    Q_UNUSED(event)
    d->dismissButton->move(this->width() - d->dismissButton->width() - 8, 8);
}

void NotificationWidget::enterEvent(QEnterEvent* event) {
    Q_UNUSED(event)
    d->dismissButton->show();
    this->update();
}

void NotificationWidget::leaveEvent(QEvent* event) {
    Q_UNUSED(event)
    d->dismissButton->hide();
    this->update();
}
