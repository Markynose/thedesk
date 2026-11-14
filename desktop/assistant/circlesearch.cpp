#include "circlesearch.h"

#include "assistantface.h"
#include "assistantpanel.h"
#include <QClipboard>
#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QProcess>
#include <QStandardPaths>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QLineEdit>
#include <QPainter>
#include <QPainterPath>
#include <QScreen>
#include <QTimer>
#include <Wm/desktopwm.h>
#include <cmath>
#include <functional>
#include <keygrab.h>
#include <m3motion.h>
#include <m3scheme.h>
#include <shapes/m3loadingindicator.h>

namespace {
    double c01(double v) { return qBound(0.0, v, 1.0); }
    double lerp(double a, double b, double t) { return a + (b - a) * t; }
    QRectF lerpRect(const QRectF& a, const QRectF& b, double t) {
        return QRectF(lerp(a.left(), b.left(), t), lerp(a.top(), b.top(), t), lerp(a.width(), b.width(), t), lerp(a.height(), b.height(), t));
    }
    QColor alpha(QColor c, double a) {
        c.setAlphaF(float(c01(a) * c.alphaF()));
        return c;
    }
    QColor mix(const QColor& a, const QColor& b, double t) {
        return QColor::fromRgbF(lerp(a.redF(), b.redF(), t), lerp(a.greenF(), b.greenF(), t), lerp(a.blueF(), b.blueF(), t), lerp(a.alphaF(), b.alphaF(), t));
    }
    // the spring's value from rest after [seconds]
    double springAt(M3Motion::Token token, double seconds) {
        static QHash<int, QVector<float>> tables;
        if (seconds <= 0) return 0;
        auto table = tables.find(int(token));
        if (table == tables.end()) {
            QVector<float> values{0};
            double value = 0, velocity = 0;
            for (int i = 1; i <= 3000; i++) {
                const M3Motion::SpringState state = M3Motion::stepSpring(value, velocity, 1, M3Motion::spring(token), 0.001);
                value = state.value;
                velocity = state.velocity;
                values.append(float(value));
            }
            table = tables.insert(int(token), values);
        }
        const int i = int(seconds * 1000);
        return i >= 3000 ? 1.0 : table->at(i);
    }
    QFont flex(double px, double wght) {
        QFont font(QStringLiteral("Roboto Flex"));
        font.setPixelSize(int(px));
        font.setVariableAxis(QFont::Tag("wght"), float(wght));
        return font;
    }
    void icon(QPainter& p, const QRectF& r, const QString& name, const QColor& c, double px) {
        QFont font(QStringLiteral("Material Symbols Rounded"));
        font.setPixelSize(int(px));
        font.setVariableAxis(QFont::Tag("FILL"), 1);
        p.setFont(font);
        p.setPen(c);
        p.drawText(r, Qt::AlignCenter, name);
    }

    // a soft glow: draw small, blur, scale up
    void blur(QImage& img, int radius) {
        const int w = img.width(), h = img.height();
        for (int pass = 0; pass < 4; pass++) {
            const bool across = pass % 2 == 0;
            const int n = across ? w : h, m = across ? h : w;
            QVector<QRgb> line(n);
            for (int j = 0; j < m; j++) {
                auto px = [&](int i) -> QRgb& { return across ? reinterpret_cast<QRgb*>(img.scanLine(j))[i] : reinterpret_cast<QRgb*>(img.scanLine(i))[j]; };
                for (int i = 0; i < n; i++) line[i] = px(i);
                int a = 0, r = 0, g = 0, b = 0;
                auto add = [&](int i, int sign) {
                    if (i < 0 || i >= n) return;
                    a += sign * qAlpha(line[i]);
                    r += sign * qRed(line[i]);
                    g += sign * qGreen(line[i]);
                    b += sign * qBlue(line[i]);
                };
                for (int i = -radius; i <= radius; i++) add(i, 1);
                const int span = 2 * radius + 1;
                for (int i = 0; i < n; i++) {
                    px(i) = qRgba(r / span, g / span, b / span, a / span);
                    add(i - radius, -1);
                    add(i + radius + 1, 1);
                }
            }
        }
    }
    void soft(QPainter& p, QImage& layer, const QSizeF& size, double opacity, int radius = 2) {
        blur(layer, radius);
        p.save();
        p.setOpacity(c01(opacity));
        p.setRenderHint(QPainter::SmoothPixmapTransform);
        p.drawImage(QRectF(QPointF(0, 0), size), layer);
        p.restore();
    }

    // the corner handles: short thick L's following the rounded corners
    QPainterPath handlesOf(const QRectF& r, double rad) {
        const double L = 18;
        QPainterPath h;
        h.moveTo(r.left(), r.top() + rad + L);
        h.lineTo(r.left(), r.top() + rad);
        h.arcTo(QRectF(r.left(), r.top(), 2 * rad, 2 * rad), 180, -90);
        h.lineTo(r.left() + rad + L, r.top());
        h.moveTo(r.right() - rad - L, r.top());
        h.lineTo(r.right() - rad, r.top());
        h.arcTo(QRectF(r.right() - 2 * rad, r.top(), 2 * rad, 2 * rad), 90, -90);
        h.lineTo(r.right(), r.top() + rad + L);
        h.moveTo(r.right(), r.bottom() - rad - L);
        h.lineTo(r.right(), r.bottom() - rad);
        h.arcTo(QRectF(r.right() - 2 * rad, r.bottom() - 2 * rad, 2 * rad, 2 * rad), 0, -90);
        h.lineTo(r.right() - rad - L, r.bottom());
        h.moveTo(r.left() + rad + L, r.bottom());
        h.lineTo(r.left() + rad, r.bottom());
        h.arcTo(QRectF(r.left(), r.bottom() - 2 * rad, 2 * rad, 2 * rad), 270, -90);
        h.lineTo(r.left(), r.bottom() - rad - L);
        return h;
    }
} // namespace

struct CircleSearch::Private {
        QImage shot;
        QElapsedTimer clock;
        QTimer frame;
        qint64 openedAt = 0, selectedAt = -1;
        QPolygonF stroke;
        bool drawing = false;
        qint64 strokeDoneAt = -1;
        QRectF from, selection; // the stroke's bounds, and what it became
        int dragging = -1;      // a corner being dragged: 0 top left, 1 top right, 2 bottom right, 3 bottom left
        bool answering = false; // Pebble's sheet is up
        QLineEdit* field;
        QList<QPair<QRectF, std::function<void()>>> hits;
        QPointF mouse;

        // The text on the frozen screen (Tesseract, read as it opens): words, and the lines they make
        struct Word {
                QRectF box;
                QString text;
                int line;
        };
        struct Line {
                QRectF box;
                QString text, translated;
                QColor background, ink;
        };
        QList<Word> words;
        QList<Line> lines;
        QProcess* reader = nullptr;
        bool read = false;
        enum Mode { Circling, Picking, Translated } mode = Circling;
        int pickFrom = -1, pickTo = -1; // words picked, in reading order
        bool picking = false;
        qint64 copiedAt = -1, modeAt = -1;
        QProcess* translator = nullptr;
        bool translating = false, translateFailed = false;
        qint64 translatedAt = -1;

        qint64 now() const { return clock.elapsed(); }
        double since(qint64 t) const { return t < 0 ? -1 : (now() - t) / 1000.0; }
};

CircleSearch* CircleSearch::instance() {
    static auto* search = new CircleSearch();
    return search;
}

void CircleSearch::setUp() {
    connect(new KeyGrab(QKeySequence(Qt::MetaModifier | Qt::ShiftModifier | Qt::Key_Space), QStringLiteral("circlesearch")), &KeyGrab::activated, instance(), [] {
        if (instance()->isVisible()) instance()->finish();
        else instance()->start();
    });
    // Pebble's sheet went away: back to circling
    connect(AssistantPanel::instance(), &AssistantPanel::sheetClosed, instance(), [] {
        CircleSearch* search = instance();
        search->d->answering = false;
        if (!search->isVisible()) return;
        search->activateWindow();
        search->d->field->setFocus();
        search->update();
    });
}

CircleSearch::CircleSearch() :
    QDialog(nullptr) {
    d = new Private();
    this->setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    this->setMouseTracking(true);
    this->setWindowTitle(tr("Circle to search"));
    this->setCursor(Qt::CrossCursor);
    d->clock.start();
    d->frame.setInterval(33);
    connect(&d->frame, &QTimer::timeout, this, qOverload<>(&QWidget::update));

    d->field = new QLineEdit(this);
    d->field->setFrame(false);
    d->field->setStyleSheet(QStringLiteral("QLineEdit { background: transparent; border: none; }"));
    d->field->setFont(flex(17, 450));
    d->field->setPlaceholderText(tr("Circle anything to ask Pebble"));
    d->field->setCursor(Qt::IBeamCursor);
    d->field->installEventFilter(this);
}

void CircleSearch::start() {
    // freeze what's on screen
    QScreen* screen = QGuiApplication::primaryScreen();
    if (!screen) return;
    d->shot = screen->grabWindow(0).toImage();
    d->openedAt = d->now();
    d->selectedAt = -1;
    d->strokeDoneAt = -1;
    d->stroke.clear();
    d->drawing = false;
    d->dragging = -1;
    d->answering = false;
    d->field->clear();
    d->mode = Private::Circling;
    d->pickFrom = d->pickTo = -1;
    d->copiedAt = d->translatedAt = -1;
    d->translating = d->translateFailed = false;
    const M3::Scheme s = M3::currentScheme();
    QPalette palette = d->field->palette();
    palette.setColor(QPalette::Text, s.onSurface);
    palette.setColor(QPalette::PlaceholderText, s.onSurfaceVariant);
    d->field->setPalette(palette);
    this->setGeometry(screen->geometry());
    readText(); // once it has its size: the words are placed by it
    this->show();
    this->raise();
    this->activateWindow();
    DesktopWm::setSystemWindow(this, DesktopWm::SystemWindowTypeMenu);
    d->field->setFocus();
    d->frame.start();
}

void CircleSearch::finish() {
    d->frame.stop();
    if (d->answering) AssistantPanel::instance()->close();
    d->answering = false;
    this->hide();
    d->shot = QImage();
}

void CircleSearch::readText() {
    // Tesseract reads the frozen screen at twice the size (small screen text reads better), in the background
    d->words.clear();
    d->lines.clear();
    d->read = false;
    if (d->reader) {
        d->reader->disconnect(this);
        d->reader->kill();
        d->reader->deleteLater();
    }
    const QString path = QDir::temp().filePath(QStringLiteral("thedesk-circle-%1.bmp").arg(QCoreApplication::applicationPid()));
    d->shot.scaled(d->shot.size() * 2, Qt::IgnoreAspectRatio, Qt::SmoothTransformation).save(path, "BMP");
    d->reader = new QProcess(this);
    const double k = this->width() > 0 ? this->width() / (2.0 * d->shot.width()) : 0.5; // read pixels -> window
    connect(d->reader, &QProcess::finished, this, [this, path, k] {
        QFile::remove(path);
        QHash<QString, int> lineOf;
        const QList<QByteArray> rows = d->reader->readAllStandardOutput().split('\n');
        for (const QByteArray& row : rows) {
            const QList<QByteArray> f = row.split('\t');
            // level 5 = a word: level page block par line word left top width height conf text
            if (f.size() < 12 || f.at(0) != "5") continue;
            const QString text = QString::fromUtf8(f.at(11)).trimmed();
            if (text.isEmpty() || f.at(10).toDouble() < 10) continue; // only the near-certain misreads go
            const QRectF box(f.at(6).toDouble() * k, f.at(7).toDouble() * k, f.at(8).toDouble() * k, f.at(9).toDouble() * k);
            const QString key = QString::fromUtf8(f.at(2) + '.' + f.at(3) + '.' + f.at(4));
            if (!lineOf.contains(key)) {
                lineOf.insert(key, int(d->lines.size()));
                d->lines.append({box, text, {}, {}, {}});
            } else {
                Private::Line& line = d->lines[lineOf.value(key)];
                line.box = line.box.united(box);
                line.text += ' ' + text;
            }
            d->words.append({box, text, lineOf.value(key)});
        }
        d->read = true;
        d->reader->deleteLater();
        d->reader = nullptr;
        if (d->mode == Private::Translated && d->translatedAt < 0 && !d->translating) translate();
        this->update();
    });
    connect(d->reader, &QProcess::errorOccurred, this, [this, path](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart) return;
        QFile::remove(path);
        d->read = true; // nothing to read with: no text
        this->update();
    });
    d->reader->start(QStringLiteral("tesseract"), {path, QStringLiteral("stdout"), QStringLiteral("-l"), QStringLiteral("eng+ukr"), QStringLiteral("tsv")});
}

void CircleSearch::translate() {
    // the lines (words only, no picture) to Pebble's quick model; each comes back numbered
    QStringList numbered;
    for (int i = 0; i < d->lines.size() && i < 160; i++) numbered.append(QStringLiteral("%1\t%2").arg(i + 1).arg(d->lines.at(i).text));
    if (numbered.isEmpty()) {
        d->translatedAt = d->now();
        return;
    }
    QString claude = QStandardPaths::findExecutable(QStringLiteral("claude"));
    if (claude.isEmpty()) claude = QStandardPaths::findExecutable(QStringLiteral("claude"), {QDir::homePath() + "/.local/bin", QDir::homePath() + "/.claude/local"});
    d->translating = true;
    d->translateFailed = false;
    d->translator = new QProcess(this);
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.remove(QStringLiteral("CLAUDECODE"));
    env.remove(QStringLiteral("CLAUDE_CODE_ENTRYPOINT"));
    d->translator->setProcessEnvironment(env);
    d->translator->setWorkingDirectory(QDir::tempPath());
    connect(d->translator, &QProcess::finished, this, [this](int code) {
        const QString out = QString::fromUtf8(d->translator->readAllStandardOutput());
        d->translator->deleteLater();
        d->translator = nullptr;
        d->translating = false;
        int got = 0;
        for (const QString& row : out.split('\n')) {
            const int tab = int(row.indexOf('\t'));
            const int n = row.left(tab).trimmed().toInt();
            if (tab < 0 || n < 1 || n > d->lines.size()) continue;
            Private::Line& line = d->lines[n - 1];
            line.translated = row.mid(tab + 1).trimmed();
            // what's behind the line, from its edges, and an ink that reads on it
            double r = 0, g = 0, b = 0;
            int count = 0;
            const double k = d->shot.width() / double(qMax(1, this->width()));
            const QRect edge = QRectF(line.box.topLeft() * k, line.box.size() * k).toAlignedRect().adjusted(-3, -3, 3, 3).intersected(d->shot.rect());
            for (int x = edge.left(); x <= edge.right(); x += 2) {
                for (int y : {edge.top(), edge.bottom()}) {
                    const QColor c = d->shot.pixelColor(x, y);
                    r += c.redF();
                    g += c.greenF();
                    b += c.blueF();
                    count++;
                }
            }
            line.background = count ? QColor::fromRgbF(r / count, g / count, b / count) : Qt::white;
            line.ink = line.background.lightnessF() < 0.5 ? QColor(245, 245, 245) : QColor(20, 20, 20);
            got++;
        }
        if (code != 0 || got == 0) d->translateFailed = true;
        else d->translatedAt = d->now();
        this->update();
    });
    d->translator->start(claude, {QStringLiteral("-p"), QStringLiteral("--model"), QStringLiteral("haiku"), QStringLiteral("--setting-sources"), QString(), QStringLiteral("--strict-mcp-config"), QStringLiteral("--disable-slash-commands"), QStringLiteral("--no-session-persistence"), QStringLiteral("--tools"), QString(),
        QStringLiteral("--system-prompt"), QStringLiteral("You translate the text of a screenshot, line by line. You answer with nothing but the translated lines.")});
    d->translator->write(QStringLiteral("Translate each numbered line into English. If a line is already English, or is a name, a number, a path or code, repeat it unchanged. Keep the numbers. Answer with only the lines, each as NUMBER<TAB>TEXT.\n\n%1\n").arg(numbered.join('\n')).toUtf8());
    d->translator->closeWriteChannel();
}

int CircleSearch::wordAt(const QPointF& at) const {
    // the word under the pointer, else the nearest one close by
    int best = -1;
    double nearest = 40;
    for (int i = 0; i < d->words.size(); i++) {
        const QRectF box = d->words.at(i).box;
        if (box.adjusted(-3, -3, 3, 3).contains(at)) return i;
        const double dx = qMax(0.0, qMax(box.left() - at.x(), at.x() - box.right())), dy = qMax(0.0, qMax(box.top() - at.y(), at.y() - box.bottom()));
        if (std::hypot(dx, dy) < nearest) {
            nearest = std::hypot(dx, dy);
            best = i;
        }
    }
    return best;
}

void CircleSearch::copyPicked(bool all) {
    // the words in reading order; a new line where the line changes
    const int from = all ? 0 : qMin(d->pickFrom, d->pickTo), to = all ? int(d->words.size()) - 1 : qMax(d->pickFrom, d->pickTo);
    if (from < 0 || to < from) return;
    QString text;
    for (int i = from; i <= to; i++) {
        if (i > from) text += d->words.at(i).line != d->words.at(i - 1).line ? '\n' : ' ';
        text += d->words.at(i).text;
    }
    QGuiApplication::clipboard()->setText(text);
    d->copiedAt = d->now();
    this->update();
}

QRectF CircleSearch::pillRect() const {
    const double w = qMin(640.0, this->width() - 80.0);
    return QRectF((this->width() - w) / 2, this->height() - 64 - 40, w, 64);
}

void CircleSearch::ask(const QString& question) {
    // the part circled (or the whole screen), and the question, to Pebble's sheet
    QImage crop = d->shot;
    if (d->selectedAt >= 0) {
        const double scale = d->shot.width() / double(qMax(1, this->width()));
        crop = d->shot.copy(QRectF(d->selection.topLeft() * scale, d->selection.size() * scale).toAlignedRect());
    }
    d->answering = true;
    AssistantPanel::instance()->openForCircle(crop, question.trimmed().isEmpty() ? tr("What's this?") : question.trimmed());
    d->field->clear();
    this->update();
}

void CircleSearch::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event)
    const M3::Scheme s = M3::currentScheme();
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    d->hits.clear();
    const QSizeF size = this->size();
    p.drawImage(this->rect(), d->shot);
    // Translated (under the scrim, so it sits in the page like the words it covers): each line replaced in place by its translation, on its own background, squeezed to fit
    if (d->mode == Private::Translated && d->translatedAt >= 0) {
        for (int i = 0; i < d->lines.size(); i++) {
            const Private::Line& line = d->lines.at(i);
            if (line.translated.isEmpty() || line.translated == line.text) continue;
            const double k = c01(springAt(M3Motion::EffectsDefault, d->since(d->translatedAt) - i * 0.012));
            if (k <= 0) continue;
            const QRectF box = line.box.adjusted(-3, -2, 3, 2);
            p.setPen(Qt::NoPen);
            p.setBrush(alpha(line.background, k));
            p.drawRoundedRect(box, 3, 3);
            double px = qMax(8.0, line.box.height() * 0.86), width = 100;
            QFont font = flex(px, 500);
            // narrower first (Roboto Flex's width axis), then smaller
            while (QFontMetricsF(font).horizontalAdvance(line.translated) > box.width() && (width > 70 || px > 8)) {
                if (width > 70) width -= 5;
                else px -= 0.5;
                font = flex(px, 500);
                font.setVariableAxis(QFont::Tag("wdth"), float(width));
            }
            p.setFont(font);
            p.setPen(alpha(line.ink, k));
            p.drawText(box.adjusted(3, 0, 0, 0), Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine, line.translated);
        }
    }


    const double open = d->since(d->openedAt);
    const double appear = c01(springAt(M3Motion::EffectsDefault, open));
    const double picked = d->selectedAt >= 0 ? c01(springAt(M3Motion::EffectsDefault, d->since(d->selectedAt))) : 0;
    const double grow = d->selectedAt >= 0 ? springAt(M3Motion::SpatialDefault, d->since(d->selectedAt)) : 0;
    const QRectF sel = lerpRect(d->from, d->selection, grow);
    const double radius = qMax(8.0, lerp(qMin(sel.width(), sel.height()) / 2, 24, c01(grow)));

    // frozen: a light scrim; once something's picked, the rest darker and the pick clear
    QPainterPath all;
    all.addRect(this->rect());
    QPainterPath inside;
    inside.addRoundedRect(sel, radius, radius);
    if (d->selectedAt >= 0) {
        p.fillPath(all.subtracted(inside), alpha(s.scrim, 0.25 + 0.2 * picked));
        p.fillPath(inside, alpha(s.scrim, 0.25 * (1 - picked)));
    } else {
        // lighter while picking text or reading a translation
        const double modeK = c01(springAt(M3Motion::EffectsDefault, d->since(d->modeAt)));
        const double clear = d->mode == Private::Circling ? 1 - modeK : modeK;
        p.fillRect(this->rect(), alpha(s.scrim, 0.25 * appear * (1 - 0.7 * (d->modeAt < 0 ? 0 : clear))));
    }

    // Picking text: every word it found faintly marked, the picked ones in Pebble's colour
    if (d->mode == Private::Picking) {
        const double k = c01(springAt(M3Motion::EffectsDefault, d->since(d->modeAt)));
        p.setPen(Qt::NoPen);
        const int from = qMin(d->pickFrom, d->pickTo), to = qMax(d->pickFrom, d->pickTo);
        for (int i = 0; i < d->words.size(); i++) {
            const bool picked = from >= 0 && i >= from && i <= to;
            p.setBrush(alpha(picked ? s.primary : s.primaryContainer, (picked ? 0.45 : 0.18) * k));
            p.drawRoundedRect(d->words.at(i).box.adjusted(-2, -1, 2, 1), 4, 4);
        }
    }

    // the shimmer: a soft band in Pebble's colours sweeping bottom to top as it opens
    if (open < 2.2) {
        const double k = springAt(M3Motion::SpatialSlow, open * 0.3);
        const double y = lerp(size.height() + 260, -320, k);
        QImage layer(int(size.width() / 6), int(size.height() / 6), QImage::Format_ARGB32_Premultiplied);
        layer.fill(Qt::transparent);
        QPainter q(&layer);
        q.setRenderHint(QPainter::Antialiasing);
        q.scale(1 / 6.0, 1 / 6.0);
        q.translate(size.width() / 2, y);
        q.rotate(-7);
        QLinearGradient band(-size.width() * 0.6, 0, size.width() * 0.6, 0);
        band.setColorAt(0, s.secondary);
        band.setColorAt(0.3, s.primary);
        band.setColorAt(0.6, s.tertiary);
        band.setColorAt(1, s.secondary);
        q.setPen(Qt::NoPen);
        for (int i = 0; i < 7; i++) { // stacked: brightest in the middle
            const double h = 60 + i * 45;
            q.setOpacity(0.22);
            q.setBrush(band);
            q.drawRoundedRect(QRectF(-size.width() * 0.7, -h / 2, size.width() * 1.4, h), h / 2, h / 2);
        }
        q.end();
        soft(p, layer, size, 0.7 * c01(springAt(M3Motion::EffectsDefault, open)) * (1 - c01(springAt(M3Motion::EffectsSlow, open - 1.3))));
    }

    // what was picked: a pulsing glow ring in Pebble's colours, and the corner handles
    if (d->selectedAt >= 0) {
        QImage layer(int(size.width() / 4), int(size.height() / 4), QImage::Format_ARGB32_Premultiplied);
        layer.fill(Qt::transparent);
        QPainter q(&layer);
        q.setRenderHint(QPainter::Antialiasing);
        q.scale(0.25, 0.25);
        const double t = d->since(d->selectedAt);
        QConicalGradient ring(sel.center(), t * 70);
        ring.setColorAt(0, s.primary);
        ring.setColorAt(0.33, s.tertiary);
        ring.setColorAt(0.66, s.secondary);
        ring.setColorAt(1, s.primary);
        q.setBrush(Qt::NoBrush);
        q.setPen(QPen(QBrush(ring), 16));
        q.drawRoundedRect(sel.adjusted(-8, -8, 8, 8), radius + 8, radius + 8);
        q.end();
        soft(p, layer, size, picked * (0.65 + 0.35 * std::sin(t * 2 * M_PI / 1.6)) * 0.8);
        const QPainterPath handles = handlesOf(sel, radius);
        p.save();
        p.setOpacity(c01(springAt(M3Motion::EffectsFast, t - 0.05)));
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(alpha(s.shadow, 0.4), 8, Qt::SolidLine, Qt::RoundCap));
        p.drawPath(handles);
        p.setPen(QPen(s.onSurface, 5, Qt::SolidLine, Qt::RoundCap));
        p.drawPath(handles);
        p.restore();
    }

    // the stroke being drawn: a white line with a soft halo in Pebble's colours; it fades once picked
    const double strokeLeft = d->strokeDoneAt >= 0 ? 1 - c01(springAt(M3Motion::EffectsDefault, d->since(d->strokeDoneAt))) : 1;
    if (d->stroke.size() > 1 && strokeLeft > 0.01) {
        QImage layer(int(size.width() / 3), int(size.height() / 3), QImage::Format_ARGB32_Premultiplied);
        layer.fill(Qt::transparent);
        QPainter q(&layer);
        q.setRenderHint(QPainter::Antialiasing);
        q.scale(1 / 3.0, 1 / 3.0);
        for (int i = 1; i < d->stroke.size(); i++) {
            q.setPen(QPen(mix(s.primary, s.tertiary, double(i) / d->stroke.size()), 26, Qt::SolidLine, Qt::RoundCap));
            q.drawLine(d->stroke.at(i - 1), d->stroke.at(i));
        }
        q.end();
        soft(p, layer, size, 0.45 * strokeLeft);
        QPainterPath line;
        line.addPolygon(d->stroke);
        p.save();
        p.setOpacity(strokeLeft);
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(alpha(mix(s.onSurface, s.primary, 0.3), 0.5), 9, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.drawPath(line);
        p.setPen(QPen(s.onSurface, 4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.drawPath(line);
        p.restore();
    }

    // close, top left
    const double chrome = c01(springAt(M3Motion::EffectsDefault, open - 0.3));
    const QRectF close(18, 46 - 10 * (1 - c01(springAt(M3Motion::SpatialFast, open - 0.3))), 44, 44);
    p.setOpacity(chrome);
    p.setPen(Qt::NoPen);
    p.setBrush(close.contains(d->mouse) ? mix(s.surfaceContainerHigh, s.onSurface, 0.08) : s.surfaceContainerHigh);
    p.drawEllipse(close);
    icon(p, close, QStringLiteral("close"), s.onSurface, 22);
    d->hits.append({close, [this] { finish(); }});

    // the hint, until they start
    const double hint = c01(springAt(M3Motion::EffectsDefault, open - 0.5)) * (d->stroke.isEmpty() && d->selectedAt < 0 && d->mode == Private::Circling ? 1 : 0);
    if (hint > 0.01) {
        const QString label = tr("Circle, scribble or tap");
        const double w = QFontMetricsF(flex(14, 500)).horizontalAdvance(label) + 64;
        const QRectF chip(size.width() / 2 - w / 2, 50 - 12 * (1 - c01(springAt(M3Motion::SpatialDefault, open - 0.5))), w, 36);
        p.setOpacity(hint);
        p.setBrush(s.surfaceContainerHighest);
        p.drawRoundedRect(chip, 18, 18);
        icon(p, QRectF(chip.left() + 14, chip.top(), 24, 36), QStringLiteral("gesture"), s.primary, 18);
        p.setFont(flex(14, 500));
        p.setPen(s.onSurface);
        p.drawText(chip.adjusted(44, 0, 0, 0), Qt::AlignVCenter, label);
    }
    p.setOpacity(1);

    // the pill: Pebble's face, the question, translate and read out the text (Pebble's own pill covers it while it answers)
    const QRectF pill = pillRect();
    const double rise = springAt(M3Motion::SpatialDefault, open - 0.2);
    if (!d->answering) {
        const double w = lerp(64, pill.width(), c01(rise));
        const QRectF shown(pill.center().x() - w / 2, pill.top() + 24 * (1 - c01(rise)), w, pill.height());
        p.setOpacity(c01(open / 0.2));
        p.setBrush(s.surfaceContainerHigh);
        p.drawRoundedRect(shown, shown.height() / 2, shown.height() / 2);
        const auto [ink, fill] = pebbleFaceColours(s, false);
        const bool working = (d->mode != Private::Circling && !d->read) || d->translating;
        const QRectF face(shown.left() + 12, shown.center().y() - 22, 44, 44);
        if (working) M3LoadingIndicator::paint(&p, face, d->now(), ink, fill);
        else M3LoadingIndicator::paintDeterminate(&p, face, float(lerp(0.1, 1, c01(d->field->text().size() / 40.0))), ink, fill);
        const bool settled = rise > 0.95;
        d->field->setVisible(settled && d->mode == Private::Circling);
        if (settled) {
            const QRectF words(pill.left() + 70, pill.top(), pill.width() - 70 - 124, pill.height());
            if (d->mode == Private::Circling) d->field->setGeometry(QRectF(pill.left() + 68, pill.center().y() - 20, pill.width() - 68 - 120, 40).toRect());
            else {
                // what the mode is doing, and its own buttons
                QString status;
                QList<QPair<QString, std::function<void()>>> actions;
                if (!d->read) status = tr("Reading the text…");
                else if (d->words.isEmpty()) status = tr("No text here");
                else if (d->mode == Private::Translated) status = d->translating ? tr("Translating…") : d->translateFailed ? tr("Couldn't translate it") : tr("Translated to English · tap again for the original");
                else if (d->since(d->copiedAt) >= 0 && d->since(d->copiedAt) < 1.6) status = tr("Copied");
                else if (d->pickFrom >= 0) {
                    const int count = qAbs(d->pickTo - d->pickFrom) + 1;
                    status = count == 1 ? tr("1 word") : tr("%1 words").arg(count);
                    actions.append({tr("Copy"), [this] { copyPicked(false); }});
                } else {
                    status = tr("Drag over text to pick it");
                    actions.append({tr("Copy all"), [this] { copyPicked(true); }});
                }
                p.setFont(flex(16, 500));
                p.setPen(s.onSurface);
                double right = words.right();
                for (int i = int(actions.size()) - 1; i >= 0; i--) {
                    const double w = QFontMetricsF(flex(14, 600)).horizontalAdvance(actions.at(i).first) + 32;
                    const QRectF b(right - w, pill.center().y() - 18, w, 36);
                    p.setBrush(b.contains(d->mouse) ? mix(s.primary, s.onPrimary, 0.08) : s.primary);
                    p.setPen(Qt::NoPen);
                    p.drawRoundedRect(b, 18, 18);
                    p.setFont(flex(14, 600));
                    p.setPen(s.onPrimary);
                    p.drawText(b, Qt::AlignCenter, actions.at(i).first);
                    d->hits.append({b, actions.at(i).second});
                    right = b.left() - 8;
                }
                p.setFont(flex(16, 500));
                p.setPen(s.onSurface);
                p.drawText(QRectF(words.left(), words.top(), right - words.left(), words.height()), Qt::AlignVCenter | Qt::AlignLeft, QFontMetricsF(flex(16, 500)).elidedText(status, Qt::ElideRight, right - words.left()));
                if (working) d->frame.start();
            }
            // translate the screen in place; pick text on it
            const QList<QPair<QString, Private::Mode>> buttons{{QStringLiteral("translate"), Private::Translated}, {QStringLiteral("text_fields"), Private::Picking}};
            for (int i = 0; i < buttons.size(); i++) {
                const QRectF b(pill.right() - 52 - (buttons.size() - 1 - i) * 48, pill.center().y() - 20, 40, 40);
                const bool on = d->mode == buttons.at(i).second;
                const QColor bg = on ? s.primary : s.secondaryContainer, fg = on ? s.onPrimary : s.onSecondaryContainer;
                p.setPen(Qt::NoPen);
                p.setBrush(b.contains(d->mouse) ? mix(bg, fg, 0.08) : bg);
                p.drawEllipse(b);
                icon(p, b, buttons.at(i).first, fg, 20);
                const Private::Mode mode = buttons.at(i).second;
                d->hits.append({b, [this, mode] {
                    d->mode = d->mode == mode ? Private::Circling : mode; // again: back to circling
                    d->modeAt = d->now();
                    d->pickFrom = d->pickTo = -1;
                    if (d->mode == Private::Translated && d->read && d->translatedAt < 0 && !d->translating) translate();
                    if (d->mode == Private::Circling) d->field->setFocus();
                    else this->setFocus(); // keys (Ctrl+C, Esc) to the overlay, not the hidden field
                    this->update();
                }});
            }
        }
        d->hits.append({shown, [] {}}); // the pill itself isn't for drawing on
        p.setOpacity(1);
    } else {
        d->field->hide();
    }
}

void CircleSearch::mousePressEvent(QMouseEvent* event) {
    const QPointF at = event->position();
    if (d->answering) return;
    for (const auto& hit : std::as_const(d->hits)) {
        if (!hit.first.contains(at)) continue;
        hit.second();
        return;
    }
    // picking text: from the word pressed on
    if (d->mode == Private::Picking) {
        d->pickFrom = d->pickTo = wordAt(at);
        d->picking = d->pickFrom >= 0;
        this->update();
        return;
    }
    if (d->mode == Private::Translated) return; // reading: nothing to draw
    // a corner of what's picked: drag it to pick a different part
    if (d->selectedAt >= 0) {
        const QList<QPointF> corners{d->selection.topLeft(), d->selection.topRight(), d->selection.bottomRight(), d->selection.bottomLeft()};
        for (int i = 0; i < corners.size(); i++) {
            if (QLineF(corners.at(i), at).length() < 28) {
                d->dragging = i;
                return;
            }
        }
    }
    d->drawing = true;
    d->strokeDoneAt = -1;
    d->stroke = QPolygonF{at};
}

void CircleSearch::mouseMoveEvent(QMouseEvent* event) {
    d->mouse = event->position();
    if (d->picking) {
        const int word = wordAt(d->mouse);
        if (word >= 0) d->pickTo = word;
    } else if (d->drawing) {
        d->stroke.append(d->mouse);
    } else if (d->dragging >= 0) {
        QRectF r = d->selection;
        const QPointF at = d->mouse;
        if (d->dragging == 0) r.setTopLeft(at);
        else if (d->dragging == 1) r.setTopRight(at);
        else if (d->dragging == 2) r.setBottomRight(at);
        else r.setBottomLeft(at);
        r = r.normalized();
        if (r.width() >= 40 && r.height() >= 40) d->from = d->selection = r;
    }
    this->update();
}

void CircleSearch::mouseDoubleClickEvent(QMouseEvent* event) {
    // a double click picks the word under it
    if (d->mode != Private::Picking) return QDialog::mouseDoubleClickEvent(event);
    d->pickFrom = d->pickTo = wordAt(event->position());
    this->update();
}

void CircleSearch::mouseReleaseEvent(QMouseEvent* event) {
    Q_UNUSED(event)
    d->picking = false;
    if (d->dragging >= 0) {
        d->dragging = -1;
        ask(tr("What's in this part?"));
        return;
    }
    if (!d->drawing) return;
    d->drawing = false;
    // a tap picks the area around it; a circle or scribble, what it went around
    const QRectF bounds = d->stroke.boundingRect();
    QRectF picked = bounds.width() < 12 && bounds.height() < 12 ? QRectF(bounds.center() - QPointF(130, 90), QSizeF(260, 180)) : bounds.adjusted(-10, -10, 10, 10);
    picked = picked.intersected(QRectF(this->rect()).adjusted(8, 8, -8, -8));
    if (picked.width() < 24 || picked.height() < 24) return;
    d->from = bounds.width() < 12 ? QRectF(bounds.center(), QSizeF(1, 1)) : bounds;
    d->selection = picked;
    d->selectedAt = d->now();
    d->strokeDoneAt = d->now();
    ask(d->field->text());
}

void CircleSearch::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Escape && d->mode != Private::Circling) {
        d->mode = Private::Circling;
        d->modeAt = d->now();
        d->field->setFocus();
        this->update();
    } else if (event->key() == Qt::Key_Escape) finish();
    else if (event->matches(QKeySequence::Copy) && d->mode == Private::Picking) copyPicked(d->pickFrom < 0);
    // not QDialog's: its Escape would only hide it, past finish()
}

bool CircleSearch::eventFilter(QObject* watched, QEvent* event) {
    if (watched == d->field && event->type() == QEvent::KeyPress) {
        auto* key = static_cast<QKeyEvent*>(event);
        if (key->key() == Qt::Key_Escape) {
            finish();
            return true;
        }
        // a question typed without a circle: about the whole screen
        if (key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) {
            if (!d->field->text().trimmed().isEmpty()) ask(d->field->text());
            return true;
        }
    }
    return QDialog::eventFilter(watched, event);
}
