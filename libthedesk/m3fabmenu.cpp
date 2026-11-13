#include "m3fabmenu.h"

#include <QEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QTimer>
#include <m3motion.h>
#include <m3scheme.h>

namespace {
    // FabMenu / FabPrimaryContainer tokens
    constexpr double Fab = 56, FabCorner = 16, Margin = 16, ItemHeight = 56, ItemGap = 4, FabGap = 8;
    constexpr int StaggerMs = 30;

    QFont flex(int px, int wght) {
        QFont font(QStringLiteral("Roboto Flex"));
        font.setPixelSize(px);
        font.setVariableAxis(QFont::Tag("wght"), float(wght));
        return font;
    }
    void symbol(QPainter& p, const QRectF& r, const QString& name, const QColor& colour) {
        QFont font(QStringLiteral("Material Symbols Rounded"));
        font.setPixelSize(24);
        p.setFont(font);
        p.setPen(colour);
        p.drawText(r, Qt::AlignCenter, name);
    }
    double c01(double v) { return qBound(0.0, v, 1.0); }
} // namespace

struct M3FabMenu::Private {
        QWidget* host;
        QString symbol, label;
        struct Item {
                QString symbol, text;
                std::function<void()> act;
                M3SpringAnimation* shown;
        };
        QList<Item> items;
        M3SpringAnimation* open;
        bool isOpen = false;
        int hovered = -2; // -1 the FAB, 0.. an item
        bool keyboard = false; // the focus ring shows for keys, not clicks
};

M3FabMenu::M3FabMenu(QWidget* host, const QString& symbol, const QString& label) :
    QWidget(host) {
    d = new Private();
    d->host = host;
    d->symbol = symbol;
    d->label = label;
    this->setMouseTracking(true);
    this->setFocusPolicy(Qt::StrongFocus);
    this->setToolTip(label);
    d->open = new M3SpringAnimation(this);
    d->open->setToken(M3Motion::SpatialFast);
    d->open->snapTo(0.0);
    connect(d->open, &M3SpringAnimation::valueChanged, this, qOverload<>(&QWidget::update));
    host->installEventFilter(this);
    place();
    this->raise();
    this->show();
}

M3FabMenu::~M3FabMenu() {
    delete d;
}

void M3FabMenu::addItem(const QString& symbol, const QString& text, std::function<void()> act) {
    auto* shown = new M3SpringAnimation(this);
    shown->setToken(M3Motion::SpatialFast);
    shown->snapTo(0.0);
    connect(shown, &M3SpringAnimation::valueChanged, this, qOverload<>(&QWidget::update));
    d->items.append({symbol, text, std::move(act), shown});
}

void M3FabMenu::setOpen(bool open) {
    if (open == d->isOpen) return;
    d->isOpen = open;
    this->setToolTip(open ? QString() : d->label);
    d->open->animateTo(open ? 1.0 : 0.0);
    // the items rise one after another (from the FAB up), and go the other way round
    const int n = int(d->items.size());
    for (int i = 0; i < n; i++) {
        M3SpringAnimation* shown = d->items.at(i).shown;
        const int delay = (open ? i : n - 1 - i) * StaggerMs;
        QTimer::singleShot(delay, shown, [shown, open] {
            shown->animateTo(open ? 1.0 : 0.0);
        });
    }
    place();
    if (open) this->setFocus();
}

void M3FabMenu::place() {
    // over the whole host while open (a click outside closes it); just the FAB otherwise
    this->setGeometry(d->host->rect());
    if (d->isOpen) {
        this->clearMask();
    } else {
        this->setMask(fabRect().adjusted(-2, -2, 2, 2).toAlignedRect());
    }
}

QRectF M3FabMenu::fabRect() const {
    return QRectF(this->width() - Margin - Fab, this->height() - Margin - Fab, Fab, Fab);
}

QRectF M3FabMenu::itemRect(int i, double k) const {
    // stacked above the FAB, lined up with its end; each grows out of that end
    const Private::Item& item = d->items.at(i);
    const double w = 16 + 24 + 8 + QFontMetricsF(flex(16, 500)).horizontalAdvance(item.text) + 24;
    const QRectF fab = fabRect();
    const double bottom = fab.top() - FabGap - i * (ItemHeight + ItemGap);
    const double shownW = qMax(ItemHeight, w * c01(k));
    return QRectF(fab.right() - shownW, bottom - ItemHeight + 12 * (1 - c01(k)), shownW, ItemHeight);
}

void M3FabMenu::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event)
    const M3::Scheme s = M3::currentScheme();
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    for (int i = 0; i < d->items.size(); i++) {
        const double k = d->items.at(i).shown->currentValue().toDouble();
        if (k <= 0.01) continue;
        const QRectF r = itemRect(i, k);
        p.save();
        p.setOpacity(c01(k * 1.6));
        p.setPen(Qt::NoPen);
        QColor fill = s.primaryContainer;
        p.setBrush(fill);
        p.drawRoundedRect(r, ItemHeight / 2, ItemHeight / 2);
        if (d->hovered == i) {
            QColor layer = s.onPrimaryContainer;
            layer.setAlphaF(0.08);
            p.setBrush(layer);
            p.drawRoundedRect(r, ItemHeight / 2, ItemHeight / 2);
        }
        p.setClipRect(r);
        symbol(p, QRectF(r.left() + 16, r.top(), 24, ItemHeight), d->items.at(i).symbol, s.onPrimaryContainer);
        p.setFont(flex(16, 500));
        p.setPen(s.onPrimaryContainer);
        p.drawText(QRectF(r.left() + 16 + 24 + 8, r.top(), r.width(), ItemHeight), Qt::AlignVCenter | Qt::AlignLeft, d->items.at(i).text);
        p.restore();
    }

    // The FAB: PrimaryContainer with its corner, opening into a round Primary close button
    const double k = d->open->currentValue().toDouble();
    const QRectF fab = fabRect();
    const double corner = FabCorner + (Fab / 2 - FabCorner) * c01(k);
    const QColor container = M3::layer(s.primaryContainer, s.primary, c01(k));
    p.setPen(Qt::NoPen);
    p.setBrush(container);
    p.drawRoundedRect(fab, corner, corner);
    if (d->hovered == -1) {
        QColor layer = k > 0.5 ? s.onPrimary : s.onPrimaryContainer;
        layer.setAlphaF(0.08);
        p.setBrush(layer);
        p.drawRoundedRect(fab, corner, corner);
    }
    p.save();
    p.translate(fab.center());
    p.rotate(90 * c01(k)); // the symbol turns into the close
    p.setOpacity(1 - c01(k * 2));
    symbol(p, QRectF(-12, -12, 24, 24), d->symbol, s.onPrimaryContainer);
    p.setOpacity(c01(k * 2 - 1));
    symbol(p, QRectF(-12, -12, 24, 24), QStringLiteral("close"), s.onPrimary);
    p.restore();
    if (this->hasFocus() && d->keyboard) {
        p.setPen(QPen(s.secondary, 3));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(fab.adjusted(-4, -4, 4, 4), corner + 4, corner + 4);
    }
}

void M3FabMenu::mousePressEvent(QMouseEvent* event) {
    d->keyboard = false;
    const QPointF at = event->position();
    if (fabRect().contains(at)) {
        setOpen(!d->isOpen);
        return;
    }
    for (int i = 0; i < d->items.size(); i++) {
        if (!d->isOpen || !itemRect(i, 1).contains(at)) continue;
        const std::function<void()> act = d->items.at(i).act;
        setOpen(false);
        if (act) QTimer::singleShot(120, this, act);
        return;
    }
    setOpen(false); // outside: close
}

void M3FabMenu::mouseMoveEvent(QMouseEvent* event) {
    int hovered = fabRect().contains(event->position()) ? -1 : -2;
    for (int i = 0; d->isOpen && i < d->items.size(); i++) {
        if (itemRect(i, 1).contains(event->position())) hovered = i;
    }
    if (hovered != d->hovered) {
        d->hovered = hovered;
        this->setCursor(hovered == -2 ? Qt::ArrowCursor : Qt::PointingHandCursor);
        this->update();
    }
}

void M3FabMenu::leaveEvent(QEvent* event) {
    Q_UNUSED(event)
    d->hovered = -2;
    this->update();
}

void M3FabMenu::keyPressEvent(QKeyEvent* event) {
    d->keyboard = true;
    this->update();
    if (event->key() == Qt::Key_Escape && d->isOpen) {
        setOpen(false);
        return;
    }
    if ((event->key() == Qt::Key_Return || event->key() == Qt::Key_Space) && !d->isOpen) {
        setOpen(true);
        return;
    }
    QWidget::keyPressEvent(event);
}

bool M3FabMenu::eventFilter(QObject* watched, QEvent* event) {
    if (watched == d->host && event->type() == QEvent::Resize) place();
    if (watched == d->host && event->type() == QEvent::Hide) {
        // the page went away: closed, without the show
        d->isOpen = false;
        d->open->snapTo(0.0);
        for (const Private::Item& item : std::as_const(d->items)) item.shown->snapTo(0.0);
        place();
    }
    return false;
}
