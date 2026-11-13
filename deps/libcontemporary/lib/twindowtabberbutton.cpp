//
// Created by Victor Tran on 13/4/2022.
//

#include "twindowtabberbutton.h"

#include "tstackedwidget.h"
#include "tvariantanimation.h"
#include "twindowtabber.h"
#include <QAction>
#include <QActionEvent>
#include <QBoxLayout>
#include <QPushButton>
#include <QToolButton>

struct tWindowTabberButtonPrivate {
        QPushButton* rootButton;
        QMap<QAction*, QToolButton*> buttons;

        QWidget* actionsWidget;
        QBoxLayout* actionsLayout;
        tVariantAnimation* actionsWidgetAnim;

        tWindowTabber* parent = nullptr;
};

tWindowTabberButton::tWindowTabberButton(tWindowTabber* parent) :
    QWidget(parent) {
    this->init();
    this->setParent(parent);
}

tWindowTabberButton::tWindowTabberButton(const QIcon& icon, const QString& text) {
    this->init();
    this->setIcon(icon);
    this->setText(text);
}

tWindowTabberButton::tWindowTabberButton(const QIcon& icon, const QString& text, tStackedWidget* toTrack, QWidget* widgetToTrack) {
    this->init();
    this->setIcon(icon);
    this->setText(text);
    this->syncWithStackedWidget(toTrack, widgetToTrack);
}

void tWindowTabberButton::init() {
    d = new tWindowTabberButtonPrivate();
    d->rootButton = new QPushButton(this);
    d->rootButton->setCheckable(true);
    d->rootButton->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);
    connect(d->rootButton, &QPushButton::clicked, this, [this] {
        d->rootButton->setChecked(false);
        this->setSelected(true);
    });

    d->actionsWidget = new QWidget(this);
    d->actionsLayout = new QBoxLayout(QBoxLayout::LeftToRight);
    d->actionsLayout->setSpacing(0);
    d->actionsLayout->setContentsMargins(0, 0, 0, 0);
    d->actionsWidget->setLayout(d->actionsLayout);

    QBoxLayout* layout = new QBoxLayout(QBoxLayout::LeftToRight);
    layout->setSpacing(0);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(d->rootButton);
    layout->addWidget(d->actionsWidget);
    this->setLayout(layout);

    d->actionsWidgetAnim = new tVariantAnimation(this);
    d->actionsWidgetAnim->setEasingCurve(QEasingCurve::OutCubic);
    d->actionsWidgetAnim->setDuration(250);
    connect(d->actionsWidgetAnim, &tVariantAnimation::valueChanged, this, [this](QVariant value) {
        d->actionsWidget->setFixedWidth(value.toInt());
    });
    d->actionsWidget->setFixedWidth(0);

    this->setAcceptDrops(true);
}

tWindowTabberButton::~tWindowTabberButton() {
    delete d;
}

void tWindowTabberButton::actionEvent(QActionEvent* event) {
    QAction* action = event->action();
    if (event->type() == QEvent::ActionAdded) {
        auto* button = new QToolButton(this);
        d->actionsLayout->addWidget(button);
        d->buttons.insert(action, button);

        connect(button, &QToolButton::clicked, action, &QAction::trigger);

        configureAction(action);
        configureActionStrip();
        emit changed();
    } else if (event->type() == QEvent::ActionChanged) {
        configureAction(action);
        emit changed();
    } else if (event->type() == QEvent::ActionRemoved) {
        auto* button = d->buttons.value(action);
        button->deleteLater();
        d->buttons.remove(action);
        configureActionStrip();
        emit changed();
    }
    QWidget::actionEvent(event);
}

void tWindowTabberButton::setText(const QString& text) {
    d->rootButton->setText(text);
    emit changed();
}

void tWindowTabberButton::setSupplementaryText(const QString& supplementaryText) {
    d->rootButton->setProperty("supplementaryText", supplementaryText);
    emit changed();
}

void tWindowTabberButton::setIcon(const QIcon& icon) {
    d->rootButton->setIcon(icon);
    emit changed();
}

bool tWindowTabberButton::isSelected() {
    return d->rootButton->isChecked();
}

void tWindowTabberButton::configureAction(QAction* action) {
    QToolButton* button = d->buttons.value(action);
    button->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    button->setIcon(action->icon());
    button->setText(action->text());
    button->setToolTip(action->text());
}

void tWindowTabberButton::setParent(tWindowTabber* tabber) {
    if (d->parent) {
    }
    QWidget::setParent(tabber);
    d->parent = tabber;
    if (d->parent) {
    }
}

void tWindowTabberButton::syncWithStackedWidget(tStackedWidget* stackedWidget, QWidget* widget) {
    connect(stackedWidget, &tStackedWidget::currentChanged, this, [this, widget, stackedWidget](int tab) {
        if (stackedWidget->widget(tab) == widget) this->setSelected(true);
    });
    connect(stackedWidget, &tStackedWidget::removingWidget, this, [this, widget](QWidget* removing) {
        if (removing == widget) this->setVisible(false);
    });
    connect(this, &tWindowTabberButton::activated, stackedWidget, [=] {
        stackedWidget->setCurrentWidget(widget);
    });
    connect(widget, &QWidget::destroyed, this, &tWindowTabberButton::deleteLater);
    if (stackedWidget->currentWidget() == widget) this->setSelected(true);
}

void tWindowTabberButton::setSelected(bool selected) {
    if (d->rootButton->isChecked() == selected) return;
    d->rootButton->setChecked(selected);
    if (selected) {
        emit activated();
        if (d->parent) d->parent->setCurrent(this);
    }
    configureActionStrip();
}

QString tWindowTabberButton::text() {
    return d->rootButton->text();
}

QString tWindowTabberButton::supplementaryText() {
    return d->rootButton->property("supplementaryText").toString();
}

QIcon tWindowTabberButton::icon() {
    return d->rootButton->icon();
}
void tWindowTabberButton::configureActionStrip() {
    d->actionsWidgetAnim->setStartValue(d->actionsWidget->width());
    d->actionsWidgetAnim->setEndValue(d->rootButton->isChecked() ? d->actionsWidget->sizeHint().width() : 0);
    d->actionsWidgetAnim->start();
}

void tWindowTabberButton::dragEnterEvent(QDragEnterEvent* event) {
    d->rootButton->click();
}
