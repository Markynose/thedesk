#include "selectionbutton.h"

#include "../clipboard/clipboardhistory.h"
#include "assistantface.h"
#include "assistantpanel.h"

#include <QClipboard>
#include <QCursor>
#include <QElapsedTimer>
#include <QGuiApplication>
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QScreen>
#include <QToolTip>
#include <QTimer>
#include <compositor.h>
#include <m3motion.h>
#include <shapes/m3loadingindicator.h>
#include <tsettings.h>

namespace {
    // M3E: the face, and on hover a pill (SurfaceContainerHigh, 48 dp,
    // fully round) springing open with four icon actions behind it
    constexpr int Margin = 4, Pill = 48, Face = 32, Action = 40, Step = 44;
    constexpr int OpenWidth = Pill + 4 * Step + 4;

    struct ActionInfo {
            const char* symbol;
            const char* name;
            const char* ask;
    };
    const ActionInfo Actions[] = {
        {"lightbulb", QT_TRANSLATE_NOOP("PebbleSelectionButton", "Explain"), QT_TRANSLATE_NOOP("PebbleSelectionButton", "Explain this")},
        {"short_text", QT_TRANSLATE_NOOP("PebbleSelectionButton", "Summarise"), QT_TRANSLATE_NOOP("PebbleSelectionButton", "Summarise it")},
        {"edit", QT_TRANSLATE_NOOP("PebbleSelectionButton", "Rewrite"), QT_TRANSLATE_NOOP("PebbleSelectionButton", "Rewrite it nicer")},
        {"translate", QT_TRANSLATE_NOOP("PebbleSelectionButton", "Translate"), QT_TRANSLATE_NOOP("PebbleSelectionButton", "Translate to English")},
    };

    double c01(double v) { return qBound(0.0, v, 1.0); }
} // namespace

struct PebbleSelectionButton::Private {
        QTimer settle; // selections change while the mouse drags: wait for a pause
        QTimer linger; // then it goes by itself
        QTimer close;  // leaving folds it back after a moment
        QTimer frame;  // the face thinks while pointed at
        QElapsedTimer clock;
        M3SpringAnimation* pop;  // the face popping in
        M3SpringAnimation* open; // the pill opening
        int hovered = -2;        // -2 nothing, -1 the face, 0.. an action
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
    this->setMouseTracking(true);
    this->setCursor(Qt::PointingHandCursor);
    d->clock.start();
    d->settle.setSingleShot(true);
    d->settle.setInterval(700);
    connect(&d->settle, &QTimer::timeout, this, &PebbleSelectionButton::appear);
    d->linger.setSingleShot(true);
    d->linger.setInterval(4000);
    connect(&d->linger, &QTimer::timeout, this, [this] {
        if (d->hovered == -2) this->hide();
    });
    d->close.setSingleShot(true);
    d->close.setInterval(350);
    connect(&d->close, &QTimer::timeout, this, [this] {
        d->open->animateTo(0.0);
    });
    d->frame.setInterval(16);
    connect(&d->frame, &QTimer::timeout, this, qOverload<>(&QWidget::update));
    d->pop = new M3SpringAnimation(this);
    d->pop->setToken(M3Motion::SpatialFast);
    d->pop->snapTo(0.0);
    connect(d->pop, &M3SpringAnimation::valueChanged, this, qOverload<>(&QWidget::update));
    d->open = new M3SpringAnimation(this);
    d->open->setToken(M3Motion::SpatialFast);
    d->open->snapTo(0.0);
    connect(d->open, &M3SpringAnimation::valueChanged, this, &PebbleSelectionButton::fit);
    fit();
}

void PebbleSelectionButton::fit() {
    // the window is as wide as the pill is open (nothing see-through to click on)
    const double k = qMax(0.0, d->open->currentValue().toDouble());
    const int width = qRound(Pill + (OpenWidth - Pill) * k);
    this->setFixedSize(width + 2 * Margin, Pill + 2 * Margin);
    if (!Compositor::isActive()) {
        // without a compositor nothing is see-through: cut it to the pill
        QPainterPath shape;
        shape.addRoundedRect(QRectF(Margin, Margin, width, Pill), Pill / 2.0, Pill / 2.0);
        this->setMask(QRegion(shape.toFillPolygon().toPolygon()));
    } else {
        this->clearMask(); // the compositor may have come up after a mask was cut
    }
    this->update();
}

void PebbleSelectionButton::selectionChanged() {
    this->hide();
    d->hovered = -2; // hidden under the pointer: no leave event comes
    d->frame.stop();
    d->open->snapTo(0.0);
    fit();
    d->settle.start();
}

void PebbleSelectionButton::appear() {
    if (!tSettings().value(QStringLiteral("Assistant/selectionButton")).toBool()) return;
    const QString text = QGuiApplication::clipboard()->text(QClipboard::Selection).trimmed();
    // not for a stray click, a novel or something that looks like a password
    if (text.size() < 3 || text.size() > 20000 || ClipboardHistory::looksLikePassword(text)) return;
    if (QGuiApplication::focusWindow()) return; // selecting inside theDesk itself
    // by the pointer, with room to open to the right
    QPoint at = QCursor::pos() + QPoint(12, 10);
    if (QScreen* screen = QGuiApplication::screenAt(QCursor::pos())) {
        const QRect area = screen->geometry();
        at.setX(qMin(at.x(), area.right() - OpenWidth - 2 * Margin));
        at.setY(qMin(at.y(), area.bottom() - Pill - 2 * Margin));
    }
    this->move(at);
    d->pop->snapTo(0.0);
    d->pop->animateTo(1.0);
    this->show();
    this->raise();
    d->linger.start();
}

QRectF PebbleSelectionButton::actionRect(int i) const {
    return QRectF(Margin + Pill + i * Step, Margin + (Pill - Action) / 2.0, Action, Action);
}

void PebbleSelectionButton::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event)
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const M3::Scheme s = M3::currentScheme();
    const double pop = qMax(0.0, d->pop->currentValue().toDouble());
    const double open = qMax(0.0, d->open->currentValue().toDouble());

    // the pill: just the face's own circle when closed
    const QRectF pill(Margin, Margin, this->width() - 2 * Margin, Pill);
    p.setPen(Qt::NoPen);
    QColor container = s.surfaceContainerHigh;
    container.setAlphaF(c01(open * 3) * c01(pop));
    p.setBrush(container);
    p.drawRoundedRect(pill, Pill / 2.0, Pill / 2.0);

    // the actions pop in one after another as it opens
    QFont symbols(QStringLiteral("Material Symbols Rounded"));
    symbols.setPixelSize(20);
    p.setFont(symbols);
    for (int i = 0; i < 4; i++) {
        const double k = c01((open - i * 0.12) / 0.6);
        if (k <= 0.01) continue;
        const QRectF r = actionRect(i);
        p.save();
        p.setOpacity(k);
        p.translate(r.center());
        p.scale(0.6 + 0.4 * k, 0.6 + 0.4 * k);
        const QRectF local(-Action / 2.0, -Action / 2.0, Action, Action);
        if (d->hovered == i) {
            p.setPen(Qt::NoPen);
            p.setBrush(s.secondaryContainer);
            p.drawEllipse(local);
        }
        p.setPen(d->hovered == i ? s.onSecondaryContainer : s.onSurfaceVariant);
        p.drawText(local, Qt::AlignCenter, QString::fromLatin1(Actions[i].symbol));
        p.restore();
    }

    // the face, thinking while pointed at
    const auto [ink, fill] = pebbleFaceColours(s, false);
    const double k = pop * (d->hovered == -1 ? 1.1 : 1);
    p.translate(QPointF(Margin + Pill / 2.0, Margin + Pill / 2.0));
    p.scale(k, k);
    const QRectF face(-Face / 2.0, -Face / 2.0, Face, Face);
    if (d->hovered != -2) M3LoadingIndicator::paint(&p, face, d->clock.elapsed(), ink, fill);
    else M3LoadingIndicator::paintDeterminate(&p, face, 0.0f, ink, fill);
}

void PebbleSelectionButton::mouseMoveEvent(QMouseEvent* event) {
    int hovered = -2;
    const QPointF at = event->position();
    if (QRectF(Margin, Margin, Pill, Pill).contains(at)) hovered = -1;
    for (int i = 0; i < 4 && d->open->currentValue().toDouble() > 0.5; i++) {
        if (actionRect(i).contains(at)) hovered = i;
    }
    if (hovered == d->hovered) return;
    d->hovered = hovered;
    if (hovered >= 0) QToolTip::showText(this->mapToGlobal(actionRect(hovered).bottomLeft().toPoint() + QPoint(0, 8)), tr(Actions[hovered].name), this);
    else QToolTip::hideText();
    this->update();
}

void PebbleSelectionButton::mousePressEvent(QMouseEvent* event) {
    Q_UNUSED(event)
    const int chosen = d->hovered;
    this->hide();
    QToolTip::hideText();
    d->frame.stop();
    d->open->snapTo(0.0);
    fit();
    if (chosen >= 0) AssistantPanel::instance()->openAndAsk(tr(Actions[chosen].ask));
    else AssistantPanel::instance()->openWithSelection();
}

void PebbleSelectionButton::enterEvent(QEnterEvent* event) {
    Q_UNUSED(event)
    d->close.stop();
    d->hovered = -1;
    d->open->animateTo(1.0);
    d->frame.start(); // the face thinks while pointed at (frames only then: the springs have their own)
}

void PebbleSelectionButton::leaveEvent(QEvent* event) {
    Q_UNUSED(event)
    d->hovered = -2;
    d->frame.stop();
    QToolTip::hideText();
    this->update();
    d->close.start();
    d->linger.start(1500);
}
