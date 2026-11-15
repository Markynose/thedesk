/****************************************
 *
 *   theDesk - the bar's CPU and memory
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 * *************************************/
#include "loadchunk.h"

#include "OverviewPane/m3overview.h"
#include <QDir>
#include <QElapsedTimer>
#include <QEvent>
#include <QFile>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLocale>
#include <QPainter>
#include <QPaintEvent>
#include <QPainterPath>
#include <QPushButton>
#include <QTimer>
#include <QtMath>
#include <Wm/desktopwm.h>
#include <barmanager.h>
#include <functional>
#include <m3motion.h>
#include <m3rollinglabel.h>
#include <m3scheme.h>
#include <map>
#include <powermanager.h>
#include <shapes/materialshapes.h>
#include <shapes/morph.h>
#include <shapes/shapepath.h>
#include <statemanager.h>
#include <statuscentermanager.h>
#include <unistd.h>

namespace {
    constexpr int Closed = 26, Expanded = 38; // the chunk's height

    double clamp01(double t) {
        return qBound(0.0, t, 1.0);
    }

    double lerp(double a, double b, double t) {
        return a + (b - a) * t;
    }

    QColor mix(const QColor& a, const QColor& b, double t) {
        t = clamp01(t);
        return QColor::fromRgbF(float(lerp(a.redF(), b.redF(), t)), float(lerp(a.greenF(), b.greenF(), t)), float(lerp(a.blueF(), b.blueF(), t)), float(lerp(a.alphaF(), b.alphaF(), t)));
    }

    QColor faded(QColor c, double opacity) {
        c.setAlphaF(float(c.alphaF() * clamp01(opacity)));
        return c;
    }

    QFont flex(int px, int weight) {
        QFont font(QStringLiteral("Roboto Flex"));
        font.setPixelSize(px);
        font.setVariableAxis(QFont::Tag("wght"), weight);
        return font;
    }

    // The figure gets heavier and wider with the load (axes in steps, so a font is made per step, not per frame)
    QFont figureFont(int px, double load, int weight, int width) {
        QFont font = flex(px, 500 + qRound(clamp01(load) * (weight - 500) / 20) * 20);
        font.setVariableAxis(QFont::Tag("wdth"), 100 + qRound(clamp01(load) * (width - 100) / 5) * 5);
        font.setFeature(QFont::Tag("tnum"), 1);
        return font;
    }

    // The load as a shape: circle -> Cookie9 -> SoftBurst, made once in steps of 0.05, in a unit square around 0
    const QPainterPath& loadShape(double load) {
        static const QVector<QPainterPath> shapes = [] {
            const m3shapes::Morph calm(MaterialShapes::circle(), MaterialShapes::cookie9Sided()), busy(MaterialShapes::cookie9Sided(), MaterialShapes::softBurst());
            const QRectF box(-0.5, -0.5, 1, 1);
            QVector<QPainterPath> all;
            for (int i = 0; i <= 20; i++) all.append(i < 10 ? m3shapes::toPath(calm, i / 10.f, box) : m3shapes::toPath(busy, (i - 10) / 10.f, box));
            return all;
        }();
        return shapes.at(qRound(clamp01(load) * 20));
    }

    // A spring stepped by the owner's frame timer; it rests (snapped) once it's within `still` and slow
    struct Spring {
            double value = 0, velocity = 0, target = 0;
            bool step(M3Motion::Token token, double dt, double still) {
                const M3Motion::SpringState s = M3Motion::stepSpring(value, velocity, target, M3Motion::spring(token), dt);
                value = s.value;
                velocity = s.velocity;
                if (std::abs(value - target) < still && std::abs(velocity) < still * 10) {
                    value = target;
                    velocity = 0;
                    return false;
                }
                return true;
            }
    };

    // /proc/stat's cpu lines (the whole machine, then each thread), in jiffies
    struct Ticks {
            qint64 busy = 0, total = 0;
    };
    QVector<Ticks> readTicks() {
        QVector<Ticks> all;
        QFile file(QStringLiteral("/proc/stat"));
        if (!file.open(QFile::ReadOnly)) return all;
        while (true) {
            const QByteArray line = file.readLine();
            if (!line.startsWith("cpu")) break; // the rest of the file (interrupts) isn't read
            const QList<QByteArray> f = line.simplified().split(' '); // cpu user nice system idle iowait irq softirq steal
            Ticks t;
            for (int i = 1; i < f.size() && i <= 8; i++) t.total += f.at(i).toLongLong();
            t.busy = t.total - f.value(4).toLongLong() - f.value(5).toLongLong();
            all.append(t);
        }
        return all;
    }

    QPair<qint64, qint64> readMemory() { // used, total (bytes)
        QFile file(QStringLiteral("/proc/meminfo"));
        if (!file.open(QFile::ReadOnly)) return {0, 0};
        qint64 total = 0, available = -1;
        for (int i = 0; i < 6 && (total == 0 || available < 0); i++) {
            const QByteArray line = file.readLine();
            const qint64 bytes = line.simplified().split(' ').value(1).toLongLong() * 1024;
            if (line.startsWith("MemTotal:")) total = bytes;
            if (line.startsWith("MemAvailable:")) available = bytes;
        }
        return {total - qMax<qint64>(0, available), total};
    }

    QString gigabytes(qint64 bytes) {
        return LoadChunk::tr("%1 GB").arg(QLocale().toString(bytes / 1073741824.0, 'f', 1));
    }

    QString amount(qint64 bytes) {
        return bytes < 1073741824 ? LoadChunk::tr("%1 MB").arg(bytes / 1048576) : gigabytes(bytes);
    }

    // A program in the panel: its processes by name, its share of the whole machine
    struct Program {
            QString name;
            double share = 0;
            qint64 memory = 0;
    };

    // The panel: the CPU in big type, a shape per thread, memory as a wavy bar, the programs using the most
    class LoadPanel : public QWidget {
        public:
            std::function<void(bool)> shownChanged;
            QPushButton* seeAll;

            LoadPanel() {
                scheme = M3::currentScheme();
                figure = new M3RollingLabel(this);
                figure->setGeometry(14, 4, 168, 58);
                figure->setFixedSize(figure->size()); // its new figures don't ask the panel to lay out and move again
                seeAll = new QPushButton(LoadChunk::tr("See all in Overview"), this);
                seeAll->setProperty("m3ButtonStyle", QStringLiteral("text"));
                seeAll->setIcon(QIcon::fromTheme(QStringLiteral("go-next")));
                seeAll->setLayoutDirection(Qt::RightToLeft); // the arrow after the words

                frames.setInterval(16);
                connect(&frames, &QTimer::timeout, this, [this] {
                    frame();
                });
                clock.start();
            }

            QSize sizeHint() const override {
                return QSize(376, 364);
            }

            void setReading(const QVector<double>& loads, qint64 used, qint64 total, const QList<Program>& busiest) {
                const bool snap = fresh; // just opened: no springing from the last time
                fresh = false;
                const double cpu = loads.first();
                figure->setText(QStringLiteral("%1%").arg(qRound(cpu * 100)));
                const QFont font = figureFont(48, cpu, 1000, 125);
                if (font != figure->font()) figure->setFont(font);
                QPalette palette = figure->palette();
                palette.setColor(QPalette::WindowText, mix(scheme.onSurface, scheme.primary, (cpu - 0.5) * 2));
                figure->setPalette(palette);

                threads.resize(loads.size() - 1);
                angles.resize(threads.size());
                // The springs only move for a change that shows: a tenth of a GB, two steps of a shape (one step,
                // a thread's usual flicker, just steps)
                bool stepped = false;
                for (int i = 0; i < threads.size(); i++) {
                    Spring& thread = threads[i];
                    thread.target = qRound(loads.at(i + 1) * 20) / 20.0;
                    if (snap || (std::abs(thread.target - thread.value) < 0.06 && thread.value != thread.target)) {
                        thread.value = thread.target;
                        thread.velocity = 0;
                        stepped = true;
                    }
                }
                if (stepped) this->update(threadStrip().toAlignedRect());
                if (gigabytes(used) != gigabytes(this->used) || snap) {
                    ram.target = total > 0 ? double(used) / total : 0;
                    this->update(memoryRect());
                }
                if (snap) ram.value = ram.target;
                this->used = used;
                this->total = total;

                // The rows glide to their rank; newcomers rise in, leavers fade
                for (auto& [name, row] : rows) row.a.target = 0;
                for (int i = 0; i < busiest.size(); i++) {
                    Row& row = rows[busiest.at(i).name];
                    if (row.a.value < 0.02) {
                        row.y.value = snap ? i : i + 0.6;
                        row.y.velocity = 0;
                    }
                    row.y.target = i;
                    row.a.target = 1;
                    if (snap) row.a.value = 1;
                    row.program = busiest.at(i);
                }
                start();
                if (snap) this->update();
                else this->update(rowsRect()); // their figures change every reading; the rest repaints as it moves
            }

        protected:
            void showEvent(QShowEvent* event) override {
                QWidget::showEvent(event);
                if (shownChanged) shownChanged(true);
            }

            void hideEvent(QHideEvent* event) override {
                QWidget::hideEvent(event);
                frames.stop();
                fresh = true;
                rows.clear();
                icons.clear();
                if (shownChanged) shownChanged(false);
            }

            void resizeEvent(QResizeEvent* event) override {
                QWidget::resizeEvent(event);
                const QSize button = seeAll->sizeHint();
                seeAll->setGeometry(this->width() - 8 - button.width(), this->height() - 8 - button.height(), button.width(), button.height());
            }

            void changeEvent(QEvent* event) override {
                if (event->type() == QEvent::PaletteChange) scheme = M3::currentScheme();
                QWidget::changeEvent(event);
            }

            void paintEvent(QPaintEvent* event) override {
                // Only the parts in the dirty rect (the figure's roll and the turning shapes repaint just theirs)
                const QRectF dirty = event->rect();
                const M3::Scheme& s = scheme;
                QPainter p(this);
                p.setRenderHint(QPainter::Antialiasing);

                if (dirty.intersects(QRectF(16, 64, 160, 18))) {
                    p.setPen(s.onSurfaceVariant);
                    p.setFont(captionFont);
                    p.drawText(QRectF(16, 64, 160, 18), Qt::AlignLeft | Qt::AlignVCenter, LoadChunk::tr("CPU, %n threads", nullptr, int(threads.size())));
                }

                // One shape per thread, each spiking and turning with its own load
                const QRectF strip = threadStrip();
                if (dirty.intersects(strip)) {
                    const double step = threads.isEmpty() ? 0 : qMin(46.0, strip.width() / threads.size()), size = qMin(38.0, step - 8);
                    for (int i = 0; i < threads.size(); i++) {
                        const QPointF c(strip.left() + step * (i + 0.5), 44);
                        const double load = threads.at(i).value;
                        p.setPen(Qt::NoPen);
                        p.setBrush(mix(s.secondaryContainer, s.primary, load));
                        p.save();
                        p.translate(c);
                        p.rotate(angles.at(i));
                        p.scale(size, size);
                        p.drawPath(loadShape(load));
                        p.restore();
                        if (step >= 20) {
                            p.setPen(s.onSurfaceVariant);
                            p.setFont(numberFont);
                            p.drawText(QRectF(c.x() - step / 2, c.y() + size / 2 + 2, step, 16), Qt::AlignCenter, QString::number(i + 1));
                        }
                    }
                }

                // Memory: the M3E wavy bar, waving only while it climbs or falls
                const QRectF track(16, 112, this->width() - 32, 10);
                if (dirty.intersects(QRectF(0, 100, this->width(), 84))) {
                    paintWave(&p, track, ram.value, waving(), phase, s.tertiary, s.secondaryContainer);
                    p.setPen(s.onSurfaceVariant);
                    p.setFont(captionFont);
                    p.drawText(QRectF(16, 124, track.width(), 20), Qt::AlignLeft | Qt::AlignVCenter, LoadChunk::tr("Memory · %1 of %2").arg(gigabytes(used), gigabytes(total)));

                    p.setPen(s.primary);
                    p.setFont(headerFont);
                    p.drawText(QRectF(16, 158, 300, 24), Qt::AlignLeft | Qt::AlignVCenter, LoadChunk::tr("Using the most"));
                }

                // The rows: bars as long as their share of a thread
                const QRectF area = rowsRect();
                if (!dirty.intersects(area)) return;
                p.setClipRect(area);
                for (auto& [name, row] : rows) {
                    if (row.a.value < 0.01) continue;
                    p.setOpacity(clamp01(row.a.value));
                    const QRectF r(area.left(), area.top() + row.y.value * RowHeight, area.width(), RowHeight);
                    const QRectF b = r.adjusted(0, 3, 0, -3);
                    const double round = b.height() / 2, share = row.program.share;
                    p.setPen(Qt::NoPen);
                    p.setBrush(s.surfaceContainerHighest);
                    p.drawRoundedRect(b, round, round);
                    p.setBrush(share < 5 ? s.secondaryContainer : s.primaryContainer);
                    p.drawRoundedRect(QRectF(b.left(), b.top(), qMax(b.height(), b.width() * clamp01(share * threads.size() / 100)), b.height()), round, round);
                    p.drawPixmap(QPointF(b.left() + round - 8, b.center().y() - 8), icon(name));

                    const double x = b.left() + 34;
                    p.setPen(s.onSurface);
                    p.setFont(nameFont);
                    p.drawText(QRectF(x, b.top(), 200, b.height()), Qt::AlignLeft | Qt::AlignVCenter, name);
                    p.setPen(s.onSurfaceVariant);
                    p.setFont(memoryFont);
                    p.drawText(QRectF(x + QFontMetricsF(nameFont).horizontalAdvance(name) + 8, b.top(), 120, b.height()), Qt::AlignLeft | Qt::AlignVCenter, amount(row.program.memory));
                    p.setPen(s.onSurface);
                    p.setFont(shareFont);
                    p.drawText(b.adjusted(0, 0, -14, 0), Qt::AlignRight | Qt::AlignVCenter, QStringLiteral("%1%").arg(qRound(share)));
                }
            }

        private:
            static constexpr double RowHeight = 32;
            struct Row {
                    Spring y, a;
                    Program program;
            };

            M3::Scheme scheme;
            M3RollingLabel* figure;
            const QFont captionFont = flex(13, 500), numberFont = flex(12, 600), headerFont = flex(14, 650), nameFont = flex(14, 550), memoryFont = flex(12, 450), shareFont = flex(14, 650);
            QVector<Spring> threads;
            QVector<double> angles;
            Spring ram;
            double phase = 0;
            qint64 used = 0, total = 0;
            std::map<QString, Row> rows;
            QHash<QString, QPixmap> icons;
            bool fresh = true;
            QTimer frames;
            QElapsedTimer clock;
            qint64 lastFrame = 0;

            QRect memoryRect() const {
                return QRect(0, 104, this->width(), 42);
            }

            QRect rowsRect() const {
                return QRect(8, 186, this->width() - 16, 4 * RowHeight);
            }

            QRectF threadStrip() const {
                return QRectF(183, 0, this->width() - 183 - 12, 86);
            }

            double waving() const {
                return M3Motion::expressive() ? clamp01(std::abs(ram.velocity) * 6) : 0;
            }

            const QPixmap& icon(const QString& name) {
                auto found = icons.find(name);
                if (found == icons.end()) found = icons.insert(name, QIcon::fromTheme(name.toLower(), QIcon::fromTheme(QStringLiteral("application-x-executable"))).pixmap(16, 16));
                return *found;
            }

            void start() {
                if (frames.isActive()) return;
                lastFrame = clock.elapsed();
                frames.start(16);
            }

            void frame() {
                const qint64 now = clock.elapsed();
                const double dt = qBound(0.001, (now - lastFrame) / 1000.0, 0.05);
                lastFrame = now;
                // Each part repaints only while it moves; the shapes alone turning tick at 20 fps
                const bool waves = ram.step(M3Motion::SpatialSlow, dt, 0.0005); // a tenth of a pixel of the track
                if (waving() > 0) phase += dt; // the wave flows back towards the start
                bool shapes = false, turning = false, ranks = false;
                for (int i = 0; i < threads.size(); i++) {
                    shapes = threads[i].step(M3Motion::SpatialSlow, dt, 0.004) || shapes;
                    if (M3Motion::expressive() && threads.at(i).value > 0.2) {
                        angles[i] = std::fmod(angles.at(i) + (threads.at(i).value - 0.2) / 0.8 * 120 * dt, 360.0);
                        turning = true;
                    }
                }
                for (auto it = rows.begin(); it != rows.end();) {
                    const bool y = it->second.y.step(M3Motion::SpatialDefault, dt, 0.003), a = it->second.a.step(M3Motion::EffectsDefault, dt, 0.005);
                    ranks = y || a || ranks;
                    if (!a && it->second.a.target == 0) it = rows.erase(it);
                    else ++it;
                }
                if (shapes || turning) this->update(threadStrip().toAlignedRect());
                if (waves) this->update(memoryRect());
                if (ranks) this->update(rowsRect());
                const bool moving = waves || shapes || ranks;
                if (!moving && !turning) frames.stop();
                else if (frames.interval() != (moving ? 16 : 50)) frames.setInterval(moving ? 16 : 50);
            }
    };
} // namespace

struct LoadChunk::Private {
        M3::Scheme scheme;
        double open = 0; // the bar: 0 closed, 1 open
        QLabel* cpuWord;
        M3RollingLabel* cpu;
        QLabel* ramWord;
        M3RollingLabel* ram;
        const QFont closedWord = flex(13, 500), openWord = flex(14, 500);
        LoadPanel* panel;
        bool panelShown = false;

        Spring level, memory, kick;
        double angle = 0;
        QTimer frames;
        QElapsedTimer clock;
        qint64 lastFrame = 0;

        QTimer poll;
        QVector<Ticks> ticks;
        QHash<int, qint64> processTicks; // only while the panel is open
        int percent = -1;

        bool locked = false, screenOff = false, covered = false; // nobody sees the bar: no reading
        DesktopWmWindowPtr active;

        QList<Program> busiest(qint64 elapsed) {
            static const qint64 page = sysconf(_SC_PAGESIZE);
            QHash<int, qint64> now;
            QHash<QString, Program> byName;
            for (const QString& entry : QDir(QStringLiteral("/proc")).entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Unsorted)) {
                bool number;
                const int pid = entry.toInt(&number);
                if (!number) continue;
                QFile file(QStringLiteral("/proc/%1/stat").arg(pid));
                if (!file.open(QFile::ReadOnly)) continue;
                const QByteArray all = file.readAll();
                const int from = all.indexOf('('), to = all.lastIndexOf(')');
                if (from < 0 || to < from) continue;
                const QList<QByteArray> f = all.mid(to + 2).split(' '); // after "pid (name) ": utime 11, stime 12, rss 21
                if (f.size() < 22) continue;
                const qint64 ticks = f.at(11).toLongLong() + f.at(12).toLongLong();
                now.insert(pid, ticks);
                const QString name = QString::fromUtf8(all.mid(from + 1, to - from - 1));
                Program& program = byName[name];
                program.name = name;
                program.memory += f.at(21).toLongLong() * page;
                if (elapsed > 0 && processTicks.contains(pid)) program.share += 100.0 * qMax<qint64>(0, ticks - processTicks.value(pid)) / elapsed;
            }
            processTicks = now;
            QList<Program> programs = byName.values();
            std::sort(programs.begin(), programs.end(), [](const Program& a, const Program& b) {
                return qRound(a.share) != qRound(b.share) ? a.share > b.share : a.memory > b.memory;
            });
            return programs.mid(0, 4);
        }
};

LoadChunk::LoadChunk() :
    Chunk() {
    d = new Private();
    d->scheme = M3::currentScheme();

    d->cpuWord = new QLabel(tr("CPU") + QLatin1Char(' '), this);
    d->cpu = new M3RollingLabel(this);
    d->ramWord = new QLabel(QStringLiteral(" · ") + tr("RAM") + QLatin1Char(' '), this);
    d->ram = new M3RollingLabel(this);
    auto* layout = new QHBoxLayout(this);
    layout->setSpacing(0);
    for (QLabel* label : {d->cpuWord, static_cast<QLabel*>(d->cpu), d->ramWord, static_cast<QLabel*>(d->ram)}) {
        label->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
        layout->addWidget(label);
    }

    d->panel = new LoadPanel();
    d->panel->shownChanged = [this](bool shown) {
        d->panelShown = shown;
        if (!shown) d->processTicks.clear();
        updatePolling();
    };
    connect(d->panel->seeAll, &QPushButton::clicked, this, [this] {
        this->hideQuickWidget();
        StateManager::statusCenterManager()->showWithPane(QStringLiteral("OverviewPane"));
    });
    connect(this, &Chunk::clicked, this, &Chunk::showQuickWidget);

    connect(&d->frames, &QTimer::timeout, this, &LoadChunk::frame);
    d->clock.start();
    d->poll.setTimerType(Qt::CoarseTimer);
    connect(&d->poll, &QTimer::timeout, this, &LoadChunk::sample);

    connect(StateManager::powerManager(), &PowerManager::lockedChanged, this, [this](bool locked) {
        d->locked = locked;
        updatePolling();
    });
    connect(StateManager::powerManager(), &PowerManager::screenOffChanged, this, [this](bool off) {
        d->screenOff = off;
        updatePolling();
    });
    connect(DesktopWm::instance(), &DesktopWm::activeWindowChanged, this, &LoadChunk::updateCovered);
    connect(DesktopWm::instance(), &DesktopWm::currentDesktopChanged, this, &LoadChunk::updateCovered);

    connect(StateManager::barManager(), &BarManager::barHeightTransitioning, this, [this](qreal percentage) {
        d->open = percentage;
        fit();
        this->update();
    });

    fit();
    updateCovered();
}

LoadChunk::~LoadChunk() {
    delete d->panel; // in the chunk's panel once it has opened, on its own before
    delete d;
}

void LoadChunk::updateCovered() {
    // A fullscreen window in front (the bar steps behind it); its state can change while it stays active
    DesktopWmWindowPtr active = DesktopWm::activeWindow();
    if (active != d->active) {
        if (d->active) d->active->disconnect(this);
        d->active = active;
        if (active) connect(active, &DesktopWmWindow::windowStateChanged, this, &LoadChunk::updateCovered);
    }
    d->covered = active && active->isFullScreen() && !active->isMinimized() && active->isOnCurrentDesktop();
    updatePolling();
}

void LoadChunk::updatePolling() {
    // Read while the bar can be seen: every 2 s, every second with the panel open
    if ((d->locked || d->screenOff || d->covered) && !d->panelShown) {
        d->poll.stop();
        d->frames.stop();
        return;
    }
    const int interval = d->panelShown ? 1000 : 2000;
    if (d->poll.isActive() && d->poll.interval() == interval) return;
    d->poll.start(interval);
    sample();
    startFrames();
}

void LoadChunk::sample() {
    const QVector<Ticks> last = d->ticks, now = readTicks();
    d->ticks = now;
    if (now.isEmpty() || last.size() != now.size() || now.first().total <= last.first().total) {
        // The load is a difference between readings: the first one looks again in a moment
        if (last.isEmpty()) QTimer::singleShot(500, this, [this] {
            if (d->poll.isActive()) sample();
        });
        return;
    }
    QVector<double> loads;
    for (int i = 0; i < now.size(); i++) {
        const double total = now.at(i).total - last.at(i).total;
        loads.append(total > 0 ? clamp01((now.at(i).busy - last.at(i).busy) / total) : 0);
    }
    const auto [used, total] = readMemory();

    // Only a change in what's shown moves anything: a whole percent, a tenth of a GB
    bool changed = false;
    const int percent = qRound(loads.first() * 100);
    if (percent != d->percent) {
        d->cpu->setText(QStringLiteral("%1%").arg(percent));
        changed = true;
    }
    const double level = qRound(loads.first() * 20) / 20.0; // the shape's steps
    if (level != d->level.target) {
        if (d->percent >= 0 && M3Motion::expressive()) d->kick.velocity += 9; // a small kick as it turns spikier or calmer
        d->level.target = level;
        changed = true;
    }
    d->percent = percent;
    const QString memory = gigabytes(used);
    if (memory != d->ram->text()) {
        d->memory.target = total > 0 ? double(used) / total : 0;
        d->ram->setText(memory);
        changed = true;
    }
    if (changed) {
        fit();
        startFrames();
    }
    if (!d->panelShown) return;
    // The programs' shares are differences too: just opened, the rows come with a second look in a moment
    const bool first = d->processTicks.isEmpty();
    const QList<Program> busiest = d->busiest(now.first().total - last.first().total);
    d->panel->setReading(loads, used, total, first ? QList<Program>() : busiest);
    if (first) QTimer::singleShot(400, this, [this] {
        if (d->panelShown) sample();
    });
}

void LoadChunk::startFrames() {
    if (!d->poll.isActive()) return; // nobody sees it
    if (d->frames.isActive()) {
        if (d->frames.interval() != 16) d->frames.setInterval(16);
        return;
    }
    d->lastFrame = d->clock.elapsed();
    d->frames.start(16);
}

void LoadChunk::frame() {
    // The springs settle and rest; busier than a fifth the shape also turns, faster the busier (20 fps is enough for that alone)
    const qint64 now = d->clock.elapsed();
    const double dt = qBound(0.001, (now - d->lastFrame) / 1000.0, 0.05);
    d->lastFrame = now;
    bool moving = d->level.step(M3Motion::SpatialSlow, dt, 0.004); // a tenth of a pixel of the shape
    moving = d->memory.step(M3Motion::SpatialSlow, dt, 0.004) || moving;
    moving = d->kick.step(M3Motion::SpatialFast, dt, 0.01) || moving;
    const double spin = M3Motion::expressive() ? qMax(0.0, d->level.value - 0.2) / 0.8 * 120 : 0; // degrees a second
    d->angle = std::fmod(d->angle + spin * dt, 360.0);
    if (!moving && spin <= 0) d->frames.stop();
    else if (d->frames.interval() != (moving ? 16 : 50)) d->frames.setInterval(moving ? 16 : 50);
    this->update(shapeRect());
}

void LoadChunk::fit() {
    // The words and RAM grow in as the bar opens; the fonts swap half way
    const double e = d->open;
    const bool full = e >= 0.5;
    for (QLabel* label : {d->cpuWord, d->ramWord, static_cast<QLabel*>(d->ram)}) {
        label->setFont(full ? d->openWord : d->closedWord);
        const int width = qCeil(QFontMetricsF(label->font()).horizontalAdvance(label->text()) * e);
        label->setFixedWidth(width);
        label->setVisible(width > 0);
    }
    d->cpu->setFont(figureFont(full ? 15 : 13, d->level.target, 900, 115));
    this->layout()->setContentsMargins(qRound(4 + lerp(20, 28, e) + 6), 0, 4, 0);
    recolour();
}

void LoadChunk::recolour() {
    const M3::Scheme& s = d->scheme;
    const auto colour = [](QLabel* label, const QColor& ink) {
        QPalette palette = label->palette();
        palette.setColor(QPalette::WindowText, ink);
        label->setPalette(palette);
    };
    colour(d->cpuWord, faded(s.onSurfaceVariant, d->open));
    colour(d->ramWord, faded(s.onSurfaceVariant, d->open));
    colour(d->ram, faded(s.onSurface, d->open));
    colour(d->cpu, mix(s.onSurface, s.primary, (d->level.target - 0.5) * 2)); // primary when it's hot
}

QRect LoadChunk::shapeRect() const {
    const double size = lerp(20, 28, d->open), r = size * 0.6 + 2;
    return QRectF(4 + size / 2 - r, this->height() / 2.0 - r, 2 * r, 2 * r).toAlignedRect();
}

void LoadChunk::changeEvent(QEvent* event) {
    // The scheme is fetched here, not per frame
    if (event->type() == QEvent::PaletteChange) {
        d->scheme = M3::currentScheme();
        recolour();
    }
    Chunk::changeEvent(event);
}

void LoadChunk::paintEvent(QPaintEvent* event) {
    if (!event->rect().intersects(shapeRect())) return; // its labels rolling
    const M3::Scheme& s = d->scheme;
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);

    // The shape, its colour warming with the load, filled from the bottom with the memory in use
    const double size = lerp(20, 28, d->open), load = d->level.value, scale = size * (1 + 0.08 * d->kick.value);
    const QPointF c(4 + size / 2, this->height() / 2.0);
    QTransform transform;
    transform.translate(c.x(), c.y());
    transform.rotate(d->angle);
    transform.scale(scale, scale);
    const QPainterPath shape = transform.map(loadShape(load));
    p.setBrush(mix(s.secondaryContainer, s.primaryContainer, load));
    p.drawPath(shape);
    QPainterPath filled;
    filled.addRect(QRectF(c.x() - size, c.y() + size / 2 - size * clamp01(d->memory.value), 2 * size, 2 * size));
    p.setBrush(s.tertiary);
    p.drawPath(shape.intersected(filled)); // antialiased, unlike a clip
}

QWidget* LoadChunk::quickWidget() {
    return d->panel;
}

QString LoadChunk::name() {
    return QStringLiteral("SystemLoad");
}

int LoadChunk::expandedHeight() {
    return Expanded;
}

int LoadChunk::statusBarHeight() {
    return Closed;
}
