#include "selectionbutton.h"

#include "../clipboard/clipboardhistory.h"
#include "assistantface.h"
#include "assistantpanel.h"

#include <QClipboard>
#include <QCursor>
#include <QElapsedTimer>
#include <QGuiApplication>
#include <QPainter>
#include <QTimer>
#include <compositor.h>
#include <m3motion.h>
#include <shapes/m3loadingindicator.h>
#include <tsettings.h>

namespace {
    constexpr int Size = 40;
} // namespace

struct PebbleSelectionButton::Private {
        QTimer settle;   // selections change while the mouse drags: wait for a pause
        QTimer linger;   // then it goes by itself
        QTimer frame;
        QElapsedTimer clock;
        M3SpringAnimation* pop;
        bool hovered = false;
};

void PebbleSelectionButton::setUp() {
    static auto* button = new PebbleSelectionButton();
    connect(QGuiApplication::clipboard(), &QClipboard::selectionChanged, button, &PebbleSelectionButton::selectionChanged);
}

PebbleSelectionButton::PebbleSelectionButton() :
    QWidget(nullptr, Qt::ToolTip | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::WindowDoesNotAcceptFocus) {
    d = new Private();
    this->setAttribute(Qt::WA_ShowWithoutActivating);
    this->setAttribute(Qt::WA_TranslucentBackground);
    this->setFixedSize(Size + 8, Size + 8);
    this->setCursor(Qt::PointingHandCursor);
    this->setToolTip(tr("Ask Pebble about this"));
    // without a compositor nothing is see-through: cut it round
    if (!Compositor::isActive()) this->setMask(QRegion(4, 4, Size, Size, QRegion::Ellipse));
    d->clock.start();
    d->settle.setSingleShot(true);
    d->settle.setInterval(700);
    connect(&d->settle, &QTimer::timeout, this, &PebbleSelectionButton::appear);
    d->linger.setSingleShot(true);
    d->linger.setInterval(4000);
    connect(&d->linger, &QTimer::timeout, this, [this] {
        if (!d->hovered) this->hide();
    });
    d->frame.setInterval(16);
    connect(&d->frame, &QTimer::timeout, this, qOverload<>(&QWidget::update));
    d->pop = new M3SpringAnimation(this);
    d->pop->setToken(M3Motion::SpatialFast);
    d->pop->snapTo(0.0);
    connect(d->pop, &M3SpringAnimation::valueChanged, this, qOverload<>(&QWidget::update));
}

void PebbleSelectionButton::selectionChanged() {
    this->hide();
    d->settle.start();
}

void PebbleSelectionButton::appear() {
    if (!tSettings().value(QStringLiteral("Assistant/selectionButton")).toBool()) return;
    const QString text = QGuiApplication::clipboard()->text(QClipboard::Selection).trimmed();
    // not for a stray click, a novel or something that looks like a password
    if (text.size() < 3 || text.size() > 20000 || ClipboardHistory::looksLikePassword(text)) return;
    if (QGuiApplication::focusWindow()) return; // selecting inside theDesk itself
    this->move(QCursor::pos() + QPoint(12, 14));
    d->pop->snapTo(0.0);
    d->pop->animateTo(1.0);
    this->show();
    this->raise();
    d->frame.start();
    d->linger.start();
}

void PebbleSelectionButton::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event)
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const M3::Scheme s = M3::currentScheme();
    const auto [ink, fill] = pebbleFaceColours(s, false);
    const double k = qMax(0.0, d->pop->currentValue().toDouble()) * (d->hovered ? 1.1 : 1);
    p.translate(this->rect().center());
    p.scale(k, k);
    const QRectF face(-Size / 2.0, -Size / 2.0, Size, Size);
    if (d->hovered) M3LoadingIndicator::paint(&p, face, d->clock.elapsed(), ink, fill);
    else M3LoadingIndicator::paintDeterminate(&p, face, 0.0f, ink, fill);
    if (!this->isVisible()) d->frame.stop();
}

void PebbleSelectionButton::mousePressEvent(QMouseEvent* event) {
    Q_UNUSED(event)
    this->hide();
    d->frame.stop();
    AssistantPanel::instance()->openWithSelection();
}

void PebbleSelectionButton::enterEvent(QEnterEvent* event) {
    Q_UNUSED(event)
    d->hovered = true; // it thinks while you point at it
    this->update();
}

void PebbleSelectionButton::leaveEvent(QEvent* event) {
    Q_UNUSED(event)
    d->hovered = false;
    d->linger.start(1500);
}
