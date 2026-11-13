#include "assistantchunk.h"

#include <QElapsedTimer>
#include <QPainter>
#include <QTimer>
#include <m3motion.h>
#include <m3scheme.h>
#include "assistantface.h"
#include <shapes/m3loadingindicator.h>

namespace {
    constexpr int Height = 26, Lead = 4, Face = 20, Gap = 7, Trail = 12;

    QFont label() {
        QFont font(QStringLiteral("Roboto Flex"));
        font.setPixelSize(13);
        font.setVariableAxis(QFont::Tag("wght"), 600);
        return font;
    }
} // namespace

struct AssistantChunk::Private {
        bool done = false;
        QString text;
        QElapsedTimer clock;
        QTimer frame;
        M3SpringAnimation* pop;
};

AssistantChunk::AssistantChunk() :
    Chunk() {
    d = new Private();
    this->setCursor(Qt::PointingHandCursor);
    d->clock.start();
    d->frame.setInterval(16);
    connect(&d->frame, &QTimer::timeout, this, qOverload<>(&QWidget::update));
    d->pop = new M3SpringAnimation(this);
    d->pop->setToken(M3Motion::SpatialFast);
    d->pop->snapTo(1.0);
    connect(d->pop, &M3SpringAnimation::valueChanged, this, qOverload<>(&QWidget::update));
}

AssistantChunk::~AssistantChunk() {
    delete d;
}

void AssistantChunk::setState(bool done, QString text) {
    if (done && !d->done) {
        d->pop->snapTo(0.6);
        d->pop->animateTo(1.0);
    }
    d->done = done;
    d->text = text;
    // the indicator turns only while it works
    if (done) d->frame.stop();
    else d->frame.start();
    this->updateGeometry();
    this->update();
}

QSize AssistantChunk::sizeHint() const {
    return QSize(Lead + Face + Gap + QFontMetrics(label()).horizontalAdvance(d->text) + Trail, Height);
}

void AssistantChunk::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event)
    const M3::Scheme s = M3::currentScheme();
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QRectF pill(0, (this->height() - Height) / 2.0, this->width(), Height);
    p.setPen(Qt::NoPen);
    p.setBrush(d->done ? s.primaryContainer : s.surfaceContainerHigh);
    p.drawRoundedRect(pill, Height / 2.0, Height / 2.0);
    const QRectF face(pill.left() + Lead, pill.center().y() - Face / 2.0, Face, Face);
    if (d->done) {
        const double k = d->pop->currentValue().toDouble();
        p.save();
        p.translate(face.center());
        p.scale(k, k);
        p.setBrush(s.primary);
        p.drawEllipse(QPointF(0, 0), Face / 2.0, Face / 2.0);
        QFont symbols(QStringLiteral("Material Symbols Rounded"));
        symbols.setPixelSize(15);
        p.setFont(symbols);
        p.setPen(s.onPrimary);
        p.drawText(QRectF(-Face / 2.0, -Face / 2.0, Face, Face), Qt::AlignCenter, QStringLiteral("check"));
        p.restore();
    } else {
        const auto [ink, fill] = pebbleFaceColours(s, false);
        M3LoadingIndicator::paint(&p, face, d->clock.elapsed(), ink, fill);
    }
    p.setFont(label());
    p.setPen(d->done ? s.onPrimaryContainer : s.onSurface);
    p.drawText(QRectF(face.right() + Gap, pill.top(), pill.width(), pill.height()), Qt::AlignLeft | Qt::AlignVCenter, d->text);
}

QString AssistantChunk::name() {
    return QStringLiteral("Assistant");
}

int AssistantChunk::expandedHeight() {
    return Height + 12;
}

int AssistantChunk::statusBarHeight() {
    return Height;
}
