#include "recorder.h"

#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QElapsedTimer>
#include <QGuiApplication>
#include <QPainter>
#include <QPainterPath>
#include <QProcess>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>
#include <barmanager.h>
#include <cmath>
#include <hudmanager.h>
#include <keygrab.h>
#include <m3motion.h>
#include <m3scheme.h>
#include <shapes/materialshapes.h>
#include <shapes/morph.h>
#include <shapes/shapepath.h>
#include <statemanager.h>

namespace {
    constexpr int Height = 26;
    constexpr int CountdownSize = 168;

    QFont flex(double px, int wght) {
        QFont font(QStringLiteral("Roboto Flex"));
        font.setPixelSize(qRound(px));
        font.setVariableAxis(QFont::Tag("wght"), float(wght));
        font.setFeature(QFont::Tag("tnum"), 1);
        return font;
    }

    // 3, 2, 1 in the middle of the region: each second the shape morphs to
    // the next and its number springs in
    class Countdown : public QWidget {
        public:
            explicit Countdown(QRect region) :
                QWidget(nullptr) {
                this->setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::X11BypassWindowManagerHint | Qt::WindowDoesNotAcceptFocus | Qt::Tool);
                this->setAttribute(Qt::WA_TranslucentBackground);
                this->setAttribute(Qt::WA_ShowWithoutActivating);
                this->setAttribute(Qt::WA_DeleteOnClose);
                this->setGeometry(QRect(region.center() - QPoint(CountdownSize / 2, CountdownSize / 2), QSize(CountdownSize, CountdownSize)));
                morphs.reserve(3);
                morphs.emplace_back(MaterialShapes::circle(), MaterialShapes::cookie9Sided());
                morphs.emplace_back(MaterialShapes::cookie9Sided(), MaterialShapes::sunny());
                morphs.emplace_back(MaterialShapes::sunny(), MaterialShapes::clover4Leaf());
                frame.setInterval(16);
                QObject::connect(&frame, &QTimer::timeout, this, [this] {
                    this->update();
                });
                clock.start();
                frame.start();
            }

        protected:
            void paintEvent(QPaintEvent* event) override {
                Q_UNUSED(event)
                const M3::Scheme s = M3::currentScheme();
                const double t = clock.elapsed() / 1000.0;
                const int second = qBound(0, int(t), 2);
                const double within = t - second;
                // A spring's step from rest, so each beat overshoots a little
                const M3Motion::Spring spring = M3Motion::spring(M3Motion::SpatialDefault);
                const double w = std::sqrt(spring.stiffness), z = spring.dampingRatio, wd = w * std::sqrt(qMax(1e-6, 1 - z * z));
                const double step = 1 - std::exp(-z * w * within) * (std::cos(wd * within) + z * w / wd * std::sin(wd * within));

                QPainter p(this);
                p.setRenderHint(QPainter::Antialiasing);
                p.setCompositionMode(QPainter::CompositionMode_Source);
                p.fillRect(this->rect(), Qt::transparent);
                p.setCompositionMode(QPainter::CompositionMode_SourceOver);
                const double size = CountdownSize * (0.82 + 0.12 * step);
                const QRectF box(this->width() / 2.0 - size / 2, this->height() / 2.0 - size / 2, size, size);
                p.translate(box.center());
                p.rotate(second * 40 + 40 * step);
                p.translate(-box.center());
                p.setPen(Qt::NoPen);
                p.setBrush(s.errorContainer);
                p.drawPath(m3shapes::toPath(morphs[second], float(qBound(0.0, step, 1.0)), box));
                p.resetTransform();
                QColor ink = s.onErrorContainer;
                ink.setAlphaF(float(qBound(0.0, within * 4, 1.0)));
                p.setPen(ink);
                p.setFont(flex(56 + 10 * step, 750));
                p.drawText(this->rect(), Qt::AlignCenter, QString::number(3 - second));
            }

        private:
            std::vector<m3shapes::Morph> morphs;
            QTimer frame;
            QElapsedTimer clock;
    };
} // namespace

struct Recorder::Private {
        QProcess* ffmpeg = nullptr;
        RecordingChunk* chunk;
        QString file;
        bool counting = false;
};

Recorder::Recorder(QObject* parent) :
    QObject(parent) {
    d = new Private();
    d->chunk = new RecordingChunk();
    connect(d->chunk, &Chunk::clicked, this, &Recorder::stop);
    connect(new KeyGrab(QKeySequence(Qt::MetaModifier | Qt::ShiftModifier | Qt::Key_R), QStringLiteral("recordStop"), this), &KeyGrab::activated, this, &Recorder::stop);
    connect(StateManager::hudManager(), &HudManager::actionTriggered, this, [this](QString id) {
        if (id == QStringLiteral("recording-show")) QDesktopServices::openUrl(QUrl::fromLocalFile(d->file));
    });
}

Recorder::~Recorder() {
    stop();
    delete d;
}

bool Recorder::isRecording() const {
    return d->ffmpeg || d->counting;
}

void Recorder::start(QRect region) {
    if (isRecording()) return;
    // x264 wants even sides
    region.setWidth(region.width() & ~1);
    region.setHeight(region.height() & ~1);
    if (region.width() < 16 || region.height() < 16) return;

    QDir dir(QStandardPaths::writableLocation(QStandardPaths::MoviesLocation));
    if (!dir.mkpath(QStringLiteral("Recordings")) || !dir.cd(QStringLiteral("Recordings"))) return;
    d->file = dir.absoluteFilePath(QStringLiteral("Recording_%1.mp4").arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd_HH-mm-ss"))));

    d->counting = true;
    auto* countdown = new Countdown(region);
    countdown->show();
    QTimer::singleShot(3000, this, [this, countdown, region] {
        countdown->close();
        // Once the countdown is off the screen, so it isn't in the first frame
        QTimer::singleShot(120, this, [this, region] {
            d->counting = false;
            d->ffmpeg = new QProcess(this);
            d->ffmpeg->setProcessChannelMode(QProcess::ForwardedErrorChannel);
            const QString display = qEnvironmentVariable("DISPLAY", QStringLiteral(":0"));
            d->ffmpeg->start(QStringLiteral("ffmpeg"), {
                QStringLiteral("-loglevel"), QStringLiteral("error"), QStringLiteral("-y"),
                QStringLiteral("-f"), QStringLiteral("x11grab"), QStringLiteral("-draw_mouse"), QStringLiteral("1"), QStringLiteral("-framerate"), QStringLiteral("30"),
                QStringLiteral("-video_size"), QStringLiteral("%1x%2").arg(region.width()).arg(region.height()),
                QStringLiteral("-i"), QStringLiteral("%1+%2,%3").arg(display).arg(region.x()).arg(region.y()),
                QStringLiteral("-c:v"), QStringLiteral("libx264"), QStringLiteral("-preset"), QStringLiteral("ultrafast"), QStringLiteral("-crf"), QStringLiteral("26"),
                QStringLiteral("-pix_fmt"), QStringLiteral("yuv420p"), d->file
            });
            connect(d->ffmpeg, &QProcess::finished, this, [this](int code) {
                d->ffmpeg->deleteLater();
                d->ffmpeg = nullptr;
                StateManager::barManager()->removeChunk(d->chunk);
                if (code == 0 && QFile::exists(d->file)) {
                    StateManager::hudManager()->showHud({
                        {QStringLiteral("icon"), QStringLiteral("media-record")},
                        {QStringLiteral("title"), tr("Recording saved")},
                        {QStringLiteral("text"), tr("In Videos, Recordings")},
                        {QStringLiteral("action"), tr("Show")},
                        {QStringLiteral("actionId"), QStringLiteral("recording-show")}
                    });
                } else {
                    StateManager::hudManager()->showHud({
                        {QStringLiteral("icon"), QStringLiteral("dialog-error")},
                        {QStringLiteral("title"), tr("Recording failed")},
                        {QStringLiteral("text"), tr("ffmpeg couldn't record the screen")}
                    });
                }
            });
            if (!d->ffmpeg->waitForStarted(3000)) {
                d->ffmpeg->deleteLater();
                d->ffmpeg = nullptr;
                StateManager::hudManager()->showHud({
                    {QStringLiteral("icon"), QStringLiteral("dialog-error")},
                    {QStringLiteral("title"), tr("Recording failed")},
                    {QStringLiteral("text"), tr("Recording needs ffmpeg")}
                });
                return;
            }
            StateManager::barManager()->addChunk(d->chunk);
            d->chunk->begin();
        });
    });
}

void Recorder::stop() {
    if (!d->ffmpeg) return;
    // "q" lets ffmpeg finish the file; a stuck one is ended after a while
    d->ffmpeg->write("q");
    QProcess* process = d->ffmpeg;
    QTimer::singleShot(5000, process, [process] {
        process->kill();
    });
}

struct RecordingChunk::Private {
        M3SpringAnimation* grow;
        QTimer frame;
        QElapsedTimer clock;
};

RecordingChunk::RecordingChunk() :
    Chunk() {
    d = new Private();
    this->setCursor(Qt::PointingHandCursor);
    this->setToolTip(tr("Recording · click to stop (Super+Shift+R)"));
    d->grow = new M3SpringAnimation(this);
    d->grow->setToken(M3Motion::SpatialDefault);
    d->grow->snapTo(0.0);
    connect(d->grow, &M3SpringAnimation::valueChanged, this, [this] {
        this->updateGeometry();
        this->update();
    });
    // The dot breathes with expressive motion; the time ticks either way
    connect(&d->frame, &QTimer::timeout, this, qOverload<>(&QWidget::update));
}

RecordingChunk::~RecordingChunk() {
    delete d;
}

void RecordingChunk::begin() {
    d->clock.start();
    d->grow->snapTo(0.0);
    d->grow->animateTo(1.0);
    d->frame.start(M3Motion::expressive() ? 33 : 500);
}

QSize RecordingChunk::sizeHint() const {
    const int full = 10 + 10 + 8 + QFontMetrics(flex(14, 600)).horizontalAdvance(QStringLiteral("00:00")) + 8 + 18 + 8;
    return QSize(qMax(Height, int(full * qMax(0.0, d->grow->currentValue().toDouble()))), Height);
}

QString RecordingChunk::name() {
    return QStringLiteral("recording");
}

int RecordingChunk::expandedHeight() {
    return Height + 12;
}

int RecordingChunk::statusBarHeight() {
    return Height;
}

void RecordingChunk::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event)
    const M3::Scheme s = M3::currentScheme();
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QRectF pill(0, (this->height() - Height) / 2.0, this->width(), Height);
    QPainterPath shape;
    shape.addRoundedRect(pill, Height / 2.0, Height / 2.0);
    p.setPen(Qt::NoPen);
    p.setBrush(s.errorContainer);
    p.drawPath(shape);
    p.setClipPath(shape);

    const double seconds = d->clock.isValid() ? d->clock.elapsed() / 1000.0 : 0;
    const double breathe = M3Motion::expressive() ? 0.5 + 0.5 * std::sin(seconds * M_PI * 1.6) : 1;
    const double r = 4 + 1.5 * breathe;
    p.setBrush(s.error);
    p.drawEllipse(QPointF(10 + 5, pill.center().y()), r, r);

    const int total = int(seconds);
    p.setFont(flex(14, 600));
    p.setPen(s.onErrorContainer);
    p.drawText(QRectF(10 + 10 + 8, pill.top(), this->width(), Height), Qt::AlignVCenter | Qt::AlignLeft, QStringLiteral("%1:%2").arg(total / 60).arg(total % 60, 2, 10, QChar('0')));

    QFont symbol(QStringLiteral("Material Symbols Rounded"));
    symbol.setPixelSize(18);
    symbol.setVariableAxis(QFont::Tag("FILL"), 1);
    p.setFont(symbol);
    p.drawText(QRectF(this->width() - 8 - 18, pill.top(), 18, Height), Qt::AlignCenter, QStringLiteral("stop"));
}
