#include "pebblelive.h"

#include "assistantface.h"
#include <QDateTime>
#include <QElapsedTimer>
#include <QFontMetrics>
#include <QPainter>
#include <QPainterPath>
#include <QTimer>
#include <barmanager.h>
#include <chunk.h>
#include <cmath>
#include <m3motion.h>
#include <m3scheme.h>
#include <shapes/m3loadingindicator.h>
#include <statemanager.h>

namespace {
    struct Live {
            int id;
            QString text;
            qint64 due, since;
    };

    constexpr int Height = 26, Lead = 4, Face = 20, Gap = 7, Trail = 12;

    QFont label() {
        QFont font(QStringLiteral("Roboto Flex"));
        font.setPixelSize(13);
        font.setVariableAxis(QFont::Tag("wght"), 600);
        font.setFeature(QFont::Tag("tnum"), 1);
        return font;
    }

    QString countdown(qint64 ms) {
        const qint64 secs = (qMax<qint64>(0, ms) + 999) / 1000;
        if (secs >= 3600) return QStringLiteral("%1:%2:%3").arg(secs / 3600).arg(secs / 60 % 60, 2, 10, QChar('0')).arg(secs % 60, 2, 10, QChar('0'));
        return QStringLiteral("%1:%2").arg(secs / 60).arg(secs % 60, 2, 10, QChar('0'));
    }

    // The chip: the soonest countdown (a ring that drains), or what's being watched
    class LiveChunk : public Chunk {
        public:
            QList<Live> live;
            std::function<void()> open;
            QTimer frame;
            QElapsedTimer clock;
            M3SpringAnimation* textWidth;

            LiveChunk() {
                this->setCursor(Qt::PointingHandCursor);
                clock.start();
                frame.setInterval(16);
                connect(&frame, &QTimer::timeout, this, [this] {
                    this->updateGeometry();
                    this->update();
                });
                textWidth = new M3SpringAnimation(this);
                textWidth->setToken(M3Motion::SpatialDefault);
                textWidth->snapTo(0.0);
                connect(textWidth, &M3SpringAnimation::valueChanged, this, [this] {
                    this->updateGeometry();
                });
                connect(this, &Chunk::clicked, this, [this] {
                    if (open) open();
                });
            }

            const Live* shown() const {
                const Live* best = nullptr;
                for (const Live& item : live)
                    if (!best || (item.due && (!best->due || item.due < best->due))) best = &item;
                return best;
            }

            QString textNow() const {
                const Live* item = shown();
                if (!item) return {};
                QString text = item->due ? countdown(item->due - QDateTime::currentMSecsSinceEpoch()) + QStringLiteral(" · ") + item->text : item->text;
                if (live.size() > 1) text += QStringLiteral("  +%1").arg(live.size() - 1);
                return text;
            }

            void changed() {
                textWidth->animateTo(double(QFontMetrics(label()).horizontalAdvance(textNow())));
                // a countdown ticks each second; a watch keeps its face moving
                if (live.isEmpty()) frame.stop();
                else frame.start(shown()->due ? 1000 : 16);
                this->update();
            }

            QSize sizeHint() const override {
                return QSize(Lead + Face + Gap + int(std::ceil(textWidth->currentValue().toDouble())) + Trail, Height);
            }
            QString name() override { return QStringLiteral("PebbleLive"); }
            int expandedHeight() override { return Height + 12; }
            int statusBarHeight() override { return Height; }

        protected:
            void paintEvent(QPaintEvent*) override {
                const Live* item = shown();
                if (!item) return;
                const M3::Scheme s = M3::currentScheme();
                QPainter p(this);
                p.setRenderHint(QPainter::Antialiasing);
                const QRectF pill(0, (this->height() - Height) / 2.0, this->width(), Height);
                p.setPen(Qt::NoPen);
                p.setBrush(s.secondaryContainer);
                p.drawRoundedRect(pill, Height / 2.0, Height / 2.0);
                const QRectF face(pill.left() + Lead, pill.center().y() - Face / 2.0, Face, Face);
                if (item->due) {
                    // what's left of the time as a ring
                    const qint64 now = QDateTime::currentMSecsSinceEpoch();
                    const double left = item->due > item->since ? qBound(0.0, double(item->due - now) / (item->due - item->since), 1.0) : 0;
                    p.setBrush(Qt::NoBrush);
                    p.setPen(QPen(s.surfaceContainerHighest, 3));
                    p.drawEllipse(face.adjusted(2, 2, -2, -2));
                    p.setPen(QPen(s.primary, 3, Qt::SolidLine, Qt::RoundCap));
                    p.drawArc(face.adjusted(2, 2, -2, -2), 90 * 16, int(-360 * 16 * left));
                } else {
                    const auto [ink, fill] = pebbleFaceColours(s, false);
                    M3LoadingIndicator::paint(&p, face, clock.elapsed(), ink, fill);
                }
                p.setFont(label());
                p.setPen(s.onSecondaryContainer);
                p.drawText(QRectF(face.right() + Gap, pill.top(), pill.width() - face.right() - Gap, pill.height()), Qt::AlignLeft | Qt::AlignVCenter, QFontMetrics(label()).elidedText(textNow(), Qt::ElideRight, int(pill.width() - face.right() - Gap - Trail / 2)));
            }
    };

    LiveChunk* chunk = nullptr;
    int nextId = 1;

    void refresh() {
        if (!chunk || !StateManager::barManager()) return;
        if (chunk->live.isEmpty() && chunk->chunkRegistered()) StateManager::barManager()->removeChunk(chunk);
        else if (!chunk->live.isEmpty() && !chunk->chunkRegistered()) StateManager::barManager()->addChunk(chunk);
        chunk->changed();
    }
} // namespace

void PebbleLive::setUp(std::function<void()> open) {
    chunk = new LiveChunk();
    chunk->open = std::move(open);
}

int PebbleLive::add(const QString& text, qint64 due, qint64 since) {
    if (!chunk) return 0;
    const int id = nextId++;
    chunk->live.append({id, text, due, since ? since : QDateTime::currentMSecsSinceEpoch()});
    refresh();
    return id;
}

void PebbleLive::remove(int id) {
    if (!chunk) return;
    chunk->live.removeIf([id](const Live& item) { return item.id == id; });
    refresh();
}
