#include "richtip.h"

#include "compositor.h"
#include <QEvent>
#include <QGuiApplication>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPointer>
#include <QScreen>
#include <QTextLayout>
#include <QTimer>
#include <QVariantAnimation>
#include <m3scheme.h>

namespace {
    // RichTooltipTokens / TooltipDefaults
    constexpr int MaxWidth = 320, Side = 16, Top = 12, ActionHeight = 36, Bottom = 8, Radius = 12;

    QFont flex(int px, int wght) {
        QFont font(QStringLiteral("Roboto Flex"));
        font.setPixelSize(px);
        font.setVariableAxis(QFont::Tag("wght"), float(wght));
        return font;
    }
    QFont titleSmall() { return flex(14, 500); }
    QFont bodyMedium() { return flex(14, 400); }
    QFont labelLarge() { return flex(14, 500); }

    // Wraps `text` to `width`; returns the height
    double wrap(QTextLayout& layout, double width) {
        layout.beginLayout();
        double y = 0;
        while (true) {
            QTextLine line = layout.createLine();
            if (!line.isValid()) break;
            line.setLineWidth(width);
            line.setPosition(QPointF(0, y));
            y += line.height() + 2;
        }
        layout.endLayout();
        return y;
    }

    // Hover on one owner: show after a moment, hide after leaving
    class Watcher : public QObject {
        public:
            Watcher(QWidget* owner, std::function<RichTip::Content()> content, std::function<void(QWidget*, const RichTip::Content&)> show, std::function<void()> hide) :
                QObject(owner), owner(owner), content(std::move(content)), show(std::move(show)), hide(std::move(hide)) {
                wait.setSingleShot(true);
                wait.setInterval(600);
                connect(&wait, &QTimer::timeout, this, [this] {
                    if (this->owner->isVisible() && this->owner->underMouse()) this->show(this->owner, this->content());
                });
                owner->installEventFilter(this);
            }

        protected:
            bool eventFilter(QObject* watched, QEvent* event) override {
                Q_UNUSED(watched)
                switch (event->type()) {
                    case QEvent::Enter:
                        wait.start();
                        break;
                    case QEvent::Leave:
                        wait.stop();
                        hide();
                        break;
                    case QEvent::MouseButtonPress:
                    case QEvent::Hide:
                        wait.stop();
                        hide();
                        break;
                    case QEvent::ToolTip:
                        return true; // the rich one instead
                    default:
                        break;
                }
                return false;
            }

        private:
            QWidget* owner;
            std::function<RichTip::Content()> content;
            std::function<void(QWidget*, const RichTip::Content&)> show;
            std::function<void()> hide;
            QTimer wait;
    };
} // namespace

struct RichTip::Private {
        Content content;
        QPointer<QWidget> owner;
        QTimer hide;
        QVariantAnimation fade;
        QRectF actionRect;
        bool actionHovered = false;
};

RichTip::RichTip() :
    QWidget(nullptr, Qt::ToolTip | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint) {
    d = new Private();
    this->setAttribute(Qt::WA_TranslucentBackground);
    this->setAttribute(Qt::WA_ShowWithoutActivating);
    this->setMouseTracking(true);
    d->hide.setSingleShot(true);
    d->hide.setInterval(250);
    connect(&d->hide, &QTimer::timeout, this, &QWidget::hide);
    // an effects spring would be the M3 way; 150 ms of fade reads the same and never overshoots
    d->fade.setDuration(150);
    d->fade.setStartValue(0.0);
    d->fade.setEndValue(1.0);
    connect(&d->fade, &QVariantAnimation::valueChanged, this, [this](const QVariant& value) {
        this->setWindowOpacity(value.toDouble());
    });
}

RichTip* RichTip::instance() {
    static RichTip* tip = new RichTip();
    return tip;
}

void RichTip::attach(QWidget* owner, std::function<Content()> content) {
    new Watcher(
        owner, std::move(content), [](QWidget* owner, const Content& c) { instance()->showFor(owner, c); }, [] { instance()->hideSoon(); });
}

void RichTip::showFor(QWidget* owner, const Content& content) {
    if (content.title.isEmpty() && content.text.isEmpty()) return;
    d->hide.stop();
    d->owner = owner;
    d->content = content;
    d->actionHovered = false;
    layOut();
    // under the owner, centred, kept on its screen
    const QRect anchor(owner->mapToGlobal(QPoint(0, 0)), owner->size());
    // below its window (the bar opens under the pointer, past the chunk)
    const int below = qMax(anchor.bottom(), owner->window()->geometry().bottom());
    QRect geometry(QPoint(anchor.center().x() - this->width() / 2, below + 8), this->size());
    if (QScreen* screen = owner->screen()) {
        const QRect area = screen->geometry().adjusted(8, 8, -8, -8);
        geometry.moveLeft(qBound(area.left(), geometry.left(), area.right() - geometry.width()));
    }
    this->setGeometry(geometry);
    if (!Compositor::isActive()) {
        QPainterPath shape;
        shape.addRoundedRect(QRectF(this->rect()), Radius, Radius);
        this->setMask(QRegion(shape.toFillPolygon().toPolygon()));
    }
    const bool wasVisible = this->isVisible();
    this->show();
    this->raise();
    this->update();
    if (!wasVisible && Compositor::isActive()) {
        this->setWindowOpacity(0);
        d->fade.start();
    }
}

void RichTip::hideSoon() {
    if (this->isVisible() && !this->underMouse()) d->hide.start();
}

void RichTip::layOut() {
    const Content& c = d->content;
    double width = 0;
    if (!c.title.isEmpty()) width = qMax(width, QFontMetricsF(titleSmall()).horizontalAdvance(c.title));
    for (const QString& line : c.text.split('\n')) width = qMax(width, QFontMetricsF(bodyMedium()).horizontalAdvance(line));
    if (!c.action.isEmpty()) width = qMax(width, QFontMetricsF(labelLarge()).horizontalAdvance(c.action) + 24 - 8);
    width = qMin(double(MaxWidth - 2 * Side), std::ceil(width) + 1);

    double height = Top;
    if (!c.title.isEmpty()) height += QFontMetricsF(titleSmall()).height() + 4;
    if (!c.text.isEmpty()) {
        QTextLayout text(QString(c.text).replace('\n', QChar::LineSeparator), bodyMedium());
        height += wrap(text, width);
    }
    height += c.action.isEmpty() ? Top : ActionHeight + Bottom;
    this->setFixedSize(int(width + 2 * Side), int(std::ceil(height)));
}

void RichTip::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event)
    const M3::Scheme s = M3::currentScheme();
    const Content& c = d->content;
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(s.surfaceContainer);
    p.drawRoundedRect(QRectF(this->rect()), Radius, Radius);

    const double width = this->width() - 2 * Side;
    double y = Top;
    if (!c.title.isEmpty()) {
        p.setFont(titleSmall());
        p.setPen(s.onSurfaceVariant);
        const QFontMetricsF metrics(titleSmall());
        p.drawText(QPointF(Side, y + metrics.ascent()), c.title);
        y += metrics.height() + 4;
    }
    if (!c.text.isEmpty()) {
        QTextLayout text(QString(c.text).replace('\n', QChar::LineSeparator), bodyMedium());
        const double top = y;
        y += wrap(text, width);
        p.setPen(s.onSurfaceVariant);
        text.draw(&p, QPointF(Side, top));
    }
    d->actionRect = QRectF();
    if (!c.action.isEmpty()) {
        // a text button, its label lined up with the text above
        const double w = QFontMetricsF(labelLarge()).horizontalAdvance(c.action) + 24;
        d->actionRect = QRectF(Side - 12, y + 2, w, ActionHeight - 4);
        if (d->actionHovered) {
            QColor layer = s.primary;
            layer.setAlphaF(0.08);
            p.setPen(Qt::NoPen);
            p.setBrush(layer);
            p.drawRoundedRect(d->actionRect, d->actionRect.height() / 2, d->actionRect.height() / 2);
        }
        p.setFont(labelLarge());
        p.setPen(s.primary);
        p.drawText(d->actionRect, Qt::AlignCenter, c.action);
    }
}

void RichTip::enterEvent(QEnterEvent* event) {
    Q_UNUSED(event)
    d->hide.stop();
}

void RichTip::leaveEvent(QEvent* event) {
    Q_UNUSED(event)
    d->actionHovered = false;
    this->update();
    if (d->owner && d->owner->underMouse()) return;
    d->hide.start();
}

void RichTip::mouseMoveEvent(QMouseEvent* event) {
    const bool over = d->actionRect.contains(event->position());
    if (over != d->actionHovered) {
        d->actionHovered = over;
        this->setCursor(over ? Qt::PointingHandCursor : Qt::ArrowCursor);
        this->update();
    }
}

void RichTip::mousePressEvent(QMouseEvent* event) {
    if (!d->actionRect.contains(event->position())) return;
    const std::function<void()> act = d->content.act;
    this->hide();
    if (act) act();
}
