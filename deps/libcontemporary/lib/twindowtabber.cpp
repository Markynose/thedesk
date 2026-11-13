//
// Created by Victor Tran on 13/4/2022.
//

#include "twindowtabber.h"
#include "tapplication.h"
#include "tlogger.h"
#include "twindowtabberbutton.h"
#include <QBoxLayout>
#include <QScrollArea>
#include <QScrollBar>
#include <QScroller>
#include <QTimer>
#include <QToolButton>

struct tWindowTabberPrivate {
        QList<tWindowTabberButton*> buttons;
        QBoxLayout* tabLayout;
        QScrollArea* scrollArea;

        QToolButton* newTabButton;

        bool scrollLeft = false;
        QTimer* scrollTimer;

        bool updateWindowTitle = false;
};

tWindowTabber::tWindowTabber(QWidget* parent) {
    d = new tWindowTabberPrivate();

    QPalette pal = this->palette();
    pal.setColor(QPalette::Window, Qt::transparent);

    d->tabLayout = new QBoxLayout(QBoxLayout::LeftToRight);
    d->tabLayout->setSpacing(0);
    d->tabLayout->setContentsMargins(0, 0, 0, 0);

    QBoxLayout* spacerLayout = new QBoxLayout(QBoxLayout::LeftToRight);
    spacerLayout->setSpacing(0);
    spacerLayout->setContentsMargins(0, 0, 0, 0);
    spacerLayout->addLayout(d->tabLayout);
    spacerLayout->addStretch();

    d->scrollArea = new QScrollArea(this);
    d->scrollArea->setPalette(pal);
    d->scrollArea->setFrameShape(QScrollArea::NoFrame);
    d->scrollArea->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);
    d->scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    d->scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    d->scrollArea->setWidget(new QWidget());
    d->scrollArea->setWidgetResizable(true);
    d->scrollArea->setMouseTracking(true);
    d->scrollArea->widget()->setLayout(spacerLayout);
    tApplication::instance()->installEventFilter(this);

    d->newTabButton = new QToolButton(this);
    d->newTabButton->setIcon(QIcon::fromTheme("list-add"));
    d->newTabButton->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    d->newTabButton->setText(tr("New Tab"));
    d->newTabButton->setToolTip(tr("New Tab"));
    d->newTabButton->setVisible(false);
    connect(d->newTabButton, &QToolButton::clicked, this, &tWindowTabber::newTabRequested);

    QBoxLayout* layout = new QBoxLayout(QBoxLayout::LeftToRight);
    layout->setSpacing(0);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(d->scrollArea);
    layout->addWidget(d->newTabButton);
    this->setLayout(layout);

    libContemporaryCommon::fixateHeight(d->scrollArea, [this] {
        return fontMetrics().height() + 28;
    });

    d->scrollTimer = new QTimer(this);
    connect(d->scrollTimer, &QTimer::timeout, this, [this] {
        bool scrollLeft = d->scrollLeft;
        if (this->layoutDirection() == Qt::RightToLeft) scrollLeft = !scrollLeft;
        d->scrollArea->horizontalScrollBar()->setValue(d->scrollArea->horizontalScrollBar()->value() + (scrollLeft ? -1 : 1));
        if (!d->scrollArea->underMouse()) d->scrollTimer->stop();
    });
}

tWindowTabber::~tWindowTabber() {
    delete d;
}

void tWindowTabber::addButton(tWindowTabberButton* button) {
    button->setParent(this);
    d->buttons.append(button);
    d->tabLayout->addWidget(button);
    connect(button, &tWindowTabberButton::changed, this, &tWindowTabber::doUpdateWindowTitle);
    connect(button, &tWindowTabberButton::destroyed, this, [this, button] {
        removeButton(button);
    });
}

void tWindowTabber::setCurrent(tWindowTabberButton* button) {
    for (tWindowTabberButton* tabberButton : d->buttons) {
        tabberButton->setSelected(button == tabberButton);
    }

    QScroller::scroller(d->scrollArea->viewport())->ensureVisible(button->geometry(), 0, 0);
    this->doUpdateWindowTitle();
}

bool tWindowTabber::eventFilter(QObject* watched, QEvent* event) {
    if (QWidget* widget = qobject_cast<QWidget*>(watched)) {
        if ((d->scrollArea->isAncestorOf(widget) || widget == d->scrollArea) && (event->type() == QEvent::MouseMove || event->type() == QEvent::Leave)) {
            if (!d->scrollArea->underMouse()) {
                d->scrollTimer->stop();
                return QObject::eventFilter(watched, event);
            }

            const int scrollDistance = 100;
            int x = d->scrollArea->mapFromGlobal(QCursor::pos()).x();
            if (x >= scrollDistance && x <= d->scrollArea->width() - scrollDistance) {
                d->scrollTimer->stop();
                return QObject::eventFilter(watched, event);
            }

            // Scroll!
            d->scrollLeft = x < scrollDistance;
            int distance = x;
            if (x > d->scrollArea->width() / 2) {
                distance = d->scrollArea->width() - x;
            }
            d->scrollTimer->setInterval(distance / 4);
            if (!d->scrollTimer->isActive()) d->scrollTimer->start();
        }
    }
    return QObject::eventFilter(watched, event);
}

void tWindowTabber::doUpdateWindowTitle() {
    if (!d->updateWindowTitle) return;

    QWidget* window = this->window();
    if (!window) return;

    for (auto button : d->buttons) {
        if (button->isSelected()) {
            window->setWindowTitle(QStringLiteral("%1 - %2").arg(button->text(), tApplication::applicationDisplayName()));
            return;
        }
    }
    window->setWindowTitle(tApplication::applicationDisplayName());
}

void tWindowTabber::setShowNewTabButton(bool showNewTabButton) {
    d->newTabButton->setVisible(showNewTabButton);
}

void tWindowTabber::setUpdateWindowTitle(bool updateWindowTitle) {
    d->updateWindowTitle = updateWindowTitle;
    this->doUpdateWindowTitle();
}

void tWindowTabber::removeButton(tWindowTabberButton* button) {
    d->tabLayout->removeWidget(button);
    d->buttons.removeOne(button);
    button->deleteLater();
}
