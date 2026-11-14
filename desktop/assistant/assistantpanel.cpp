#include "assistantpanel.h"

#include "../clipboard/clipboardhistory.h"
#include "assistantchunk.h"
#include "assistantface.h"
#include "selectionbutton.h"
#include "pebblenudges.h"
#include "pebblelive.h"
#include "circlesearch.h"
#include "claudebrain.h"
#include "demobrain.h"
#include "pebblesettings.h"
#include <statuscentermanager.h>

#include <QClipboard>
#include <QDesktopServices>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMimeData>
#include <QJsonObject>
#include <assistanttools.h>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPainter>
#include <QPainterPath>
#include <QProcess>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QScreen>
#include <QTimer>
#include <QUrl>
#include <QVariantAnimation>
#include <Wm/desktopwm.h>
#include <barmanager.h>
#include <cmath>
#include <compositor.h>
#include <functional>
#include <keygrab.h>
#include <m3motion.h>
#include <m3scheme.h>
#include <shapes/m3loadingindicator.h>
#include <statemanager.h>
#include <hudmanager.h>
#include <desksound.h>
#include <QDBusConnection>
#include <QDBusInterface>
#include <tsettings.h>
#include <shapes/materialshapes.h>
#include <shapes/shapepath.h>
#include <mpris/mprisengine.h>
#include <mpris/mprisplayer.h>
#include <QPointer>
#include <csignal>
#include <unistd.h>

namespace {
    constexpr double PillWidth = 640, PillHeight = 64, StageWidth = 760;
    constexpr double ScrimOpacity = 0.45;
    constexpr qint64 LineGoesAfterMs = 1500; // a long answer's first-sentence line, after the last word lands
    constexpr qint64 FreshThreadAfterMs = 10 * 60 * 1000; // a new thread after 10 idle minutes
    const QString Name = QStringLiteral("Pebble");

    // The spring's value from rest after [seconds], sampled once per token
    double springAt(M3Motion::Token token, double seconds) {
        static QHash<int, QVector<float>> tables;
        if (seconds <= 0) return 0;
        auto table = tables.find(int(token));
        if (table == tables.end()) {
            QVector<float> values{0};
            double value = 0, velocity = 0;
            const M3Motion::Spring spring = M3Motion::spring(token);
            for (int i = 1; i <= 3000; i++) {
                const M3Motion::SpringState state = M3Motion::stepSpring(value, velocity, 1, spring, 0.001);
                value = state.value;
                velocity = state.velocity;
                values.append(float(value));
            }
            table = tables.insert(int(token), values);
        }
        const int i = int(seconds * 1000);
        return i >= 3000 ? 1.0 : table->at(i);
    }
    double c01(double v) { return qBound(0.0, v, 1.0); }
    double lerp(double a, double b, double t) { return a + (b - a) * t; }
    QRectF lerpRect(const QRectF& a, const QRectF& b, double t) {
        return QRectF(lerp(a.left(), b.left(), t), lerp(a.top(), b.top(), t), lerp(a.width(), b.width(), t), lerp(a.height(), b.height(), t));
    }
    QColor mix(const QColor& a, const QColor& b, double t) {
        t = c01(t);
        return QColor::fromRgbF(lerp(a.redF(), b.redF(), t), lerp(a.greenF(), b.greenF(), t), lerp(a.blueF(), b.blueF(), t), lerp(a.alphaF(), b.alphaF(), t));
    }
    QColor alpha(QColor c, double a) {
        c.setAlphaF(float(c.alphaF() * c01(a)));
        return c;
    }
    QFont flex(double px, double wght, double wdth = 100) {
        QFont font(QStringLiteral("Roboto Flex"));
        font.setPixelSize(qMax(1, qRound(px)));
        // in steps: every new axis value is a new font instance, and springs make a fresh one each frame
        font.setVariableAxis(QFont::Tag("wght"), float(qBound(100.0, std::round(wght / 20) * 20, 1000.0)));
        font.setVariableAxis(QFont::Tag("wdth"), float(qBound(25.0, std::round(wdth / 5) * 5, 151.0)));
        return font;
    }
    QFont mono(double px, double wght = 450) {
        QFont font(QStringLiteral("Google Sans Code"));
        font.setPixelSize(qRound(px));
        font.setVariableAxis(QFont::Tag("wght"), float(wght));
        return font;
    }
    void text(QPainter& p, const QRectF& r, const QString& s, const QFont& f, const QColor& c, int align = Qt::AlignLeft | Qt::AlignVCenter) {
        p.setFont(f);
        p.setPen(c);
        p.drawText(r, align, s);
    }
    void icon(QPainter& p, const QRectF& r, const QString& name, const QColor& c, double px = 20, bool fill = false) {
        QFont font(QStringLiteral("Material Symbols Rounded"));
        font.setPixelSize(qRound(px));
        font.setVariableAxis(QFont::Tag("FILL"), fill ? 1 : 0);
        text(p, r, name, font, c, Qt::AlignCenter);
    }
    double textWidth(const QString& s, const QFont& f) { return QFontMetricsF(f).horizontalAdvance(s); }
    double chipWidth(const QString& ic, const QString& label) { return 24 + (ic.isEmpty() ? 0 : 26) + textWidth(label, flex(14, 560)); }

    // An answer as the brain writes it: a tone tag first ({done} {unsure}
    // {oops}), **key words**, [[file:/path]] and [[app:Name]] chips.
    // Markup still arriving hides the rest until it closes.
    struct AnswerToken {
            QString text;
            int kind = 0; // 0 word, 1 key word, 2 file chip, 3 app chip, 4 line break, 5 link chip
            QString target;
            bool glued = false; // no space before it ("**391**." keeps its full stop)
            int style = -1;     // a <t …> span it's in, and its place in it
            int inStyle = 0;
    };
    // <t wght=900 wdth=150 slnt=-10 size=1.6 color=tertiary fx=wave>words</t>:
    // Pebble styling its own words. wght, wdth and size take a ramp across the
    // words ("200-900"); fx plays once after the words land
    struct WordStyle {
            double wght[2] = {-1, -1}, wdth[2] = {-1, -1}, size[2] = {1, 1}, slnt = 0;
            QString colour, fx;
            int words = 0;
    };
    struct ParsedAnswer {
            int tone = 0; // 0 plain, 1 done, 2 unsure, 3 oops
            QString face; // a face tag: {hi} {heart} {lol} {night} {bye}
            QList<AnswerToken> tokens;
            QList<WordStyle> styles;
    };
    WordStyle parseStyle(const QString& attributes) {
        WordStyle style;
        static const QRegularExpression pair(QStringLiteral("(\\w+)\\s*=\\s*[\"']?([^\"'\\s>]+)"));
        auto range = [](const QString& value, double* out, double lo, double hi) {
            // "700" or a ramp "200-900" (a leading minus is a negative number, not a ramp)
            const int dash = int(value.indexOf('-', 1));
            bool ok1 = false, ok2 = true;
            out[0] = qBound(lo, (dash < 0 ? value : value.left(dash)).toDouble(&ok1), hi);
            out[1] = dash < 0 ? out[0] : qBound(lo, value.mid(dash + 1).toDouble(&ok2), hi);
            if (!ok1 || !ok2) out[0] = out[1] = -1;
        };
        for (auto it = pair.globalMatch(attributes); it.hasNext();) {
            const QRegularExpressionMatch m = it.next();
            const QString key = m.captured(1).toLower(), value = m.captured(2);
            if (key == "wght" || key == "weight") range(value, style.wght, 100, 1000);
            else if (key == "wdth" || key == "width") range(value, style.wdth, 25, 151);
            else if (key == "size") {
                range(value, style.size, 0.6, 2.2);
                if (style.size[0] < 0) style.size[0] = style.size[1] = 1;
            } else if (key == "slnt" || key == "slant") style.slnt = qBound(-10.0, value.toDouble(), 0.0);
            else if (key == "color" || key == "colour") style.colour = value.toLower();
            else if (key == "fx") style.fx = value.toLower();
        }
        return style;
    }
    ParsedAnswer parseAnswer(const QString& raw) {
        ParsedAnswer out;
        QString s = raw;
        while (!s.isEmpty() && s.at(0).isSpace()) s.remove(0, 1);
        static const QHash<QString, QString> faces{{"hi", "hi"}, {"heart", "love"}, {"lol", "lol"}, {"night", "gn"}, {"bye", "bye"}};
        while (s.startsWith('{')) { // a tone tag and a face tag, either order
            const int close = int(s.indexOf('}'));
            if (close < 0) return out;
            const QString tag = s.mid(1, close - 1).trimmed().toLower();
            if (faces.contains(tag)) out.face = faces.value(tag);
            else if (!out.tone) out.tone = tag == "done" ? 1 : tag == "unsure" ? 2 : tag == "oops" ? 3 : 0;
            s = s.mid(close + 1);
            while (!s.isEmpty() && s.at(0).isSpace()) s.remove(0, 1);
        }
        bool key = false, glued = false;
        int closedAt = -2; // where the last ** or ]] ended: a word right after it has no space before it
        int style = -1;
        QString word;
        auto flush = [&] {
            if (word.isEmpty()) return;
            const bool lineStart = out.tokens.isEmpty() || out.tokens.last().kind == 4;
            if (lineStart && (word == "-" || word == "*")) word = QStringLiteral("•");
            out.tokens.append({word, key ? 1 : 0, {}, glued, style, style >= 0 ? out.styles[style].words++ : 0});
            word.clear();
        };
        for (int k = 0; k < s.size(); k++) {
            const QChar c = s.at(k);
            if (s.mid(k, 3) == QStringLiteral("<t ") || s.mid(k, 3) == QStringLiteral("<t>") || s.mid(k, 4) == QStringLiteral("</t>")) {
                flush();
                const int close = int(s.indexOf('>', k));
                if (close < 0) break; // the tag is still arriving
                if (s.at(k + 1) == '/') {
                    style = -1;
                    closedAt = close;
                } else {
                    out.styles.append(parseStyle(s.mid(k + 2, close - k - 2)));
                    style = int(out.styles.size()) - 1;
                    if (k > 0 && !s.at(k - 1).isSpace()) closedAt = close; // "<t>" glued to the word before
                }
                k = close;
                continue;
            }
            if (s.mid(k, 2) == QStringLiteral("**")) {
                flush();
                if (!key && s.indexOf(QStringLiteral("**"), k + 2) < 0) break; // the rest is still arriving
                if (key) closedAt = k + 1;
                key = !key;
                k++;
                continue;
            }
            if (s.mid(k, 2) == QStringLiteral("[[")) {
                flush();
                const int close = int(s.indexOf(QStringLiteral("]]"), k));
                if (close < 0) break;
                const QString inner = s.mid(k + 2, close - k - 2);
                if (inner.startsWith(QStringLiteral("file:"))) {
                    const QString path = inner.mid(5).trimmed();
                    const QString name = QFileInfo(path).fileName();
                    out.tokens.append({name.isEmpty() ? path : name, 2, path});
                } else if (inner.startsWith(QStringLiteral("app:"))) {
                    out.tokens.append({inner.mid(4).trimmed(), 3, inner.mid(4).trimmed()});
                }
                k = close + 1;
                closedAt = k;
                continue;
            }
            if (c == '[') {
                // a markdown link, [words](url), becomes a link chip
                const int bracket = int(s.indexOf(']', k));
                if (bracket < 0) break; // still arriving
                if (s.mid(bracket, 2) == QStringLiteral("](")) {
                    // the closing bracket that matches (URLs can hold brackets of their own)
                    int paren = -1, depth = 0;
                    for (int j = bracket + 2; j < s.size() && paren < 0; j++) {
                        if (s.at(j) == '(') depth++;
                        else if (s.at(j) == ')' && depth-- == 0) paren = j;
                    }
                    if (paren < 0) break;
                    flush();
                    out.tokens.append({s.mid(k + 1, bracket - k - 1).simplified(), 5, s.mid(bracket + 2, paren - bracket - 2).trimmed()});
                    k = paren;
                    closedAt = k;
                    continue;
                }
            }
            if (c == '\n') {
                flush();
                if (!out.tokens.isEmpty() && out.tokens.last().kind != 4) out.tokens.append({{}, 4, {}});
                continue;
            }
            if (c == '`') continue;
            if (c.isSpace()) {
                flush();
                continue;
            }
            if (word.isEmpty()) glued = k - 1 == closedAt && !out.tokens.isEmpty() && out.tokens.last().kind != 4;
            word += c;
        }
        flush();
        return out;
    }

    QPair<QColor, QColor> faceColours(const M3::Scheme& s, bool deep) {
        return pebbleFaceColours(s, deep);
    }

    QString firstName() {
        const QString full = qEnvironmentVariableIsSet("TD_DISPLAY_NAME") ? qEnvironmentVariable("TD_DISPLAY_NAME") : DesktopWm::userDisplayName();
        return full.section(' ', 0, 0).trimmed();
    }

    // Something to say on opening, by the time of day (and when they're back soon)
    QString greeting(bool back) {
        const QString name = firstName().isEmpty() ? QStringLiteral("there") : QStringLiteral("**%1**").arg(firstName());
        const int hour = QTime::currentTime().hour();
        QStringList lines;
        if (back) lines = {AssistantPanel::tr("Back already, %1?"), AssistantPanel::tr("Hey again, %1."), AssistantPanel::tr("Yo %1, what else")};
        else if (hour < 5) lines = {AssistantPanel::tr("Still up, %1?"), AssistantPanel::tr("Late one, %1. What's up"), AssistantPanel::tr("Hey %1. Can't sleep?")};
        else if (hour < 12) lines = {AssistantPanel::tr("Morning, %1."), AssistantPanel::tr("Morning %1. What are we doing today"), AssistantPanel::tr("Hey %1, early start?")};
        else if (hour < 18) lines = {AssistantPanel::tr("Hey %1."), AssistantPanel::tr("Afternoon, %1. What's up"), AssistantPanel::tr("Yo %1, what do you need")};
        else lines = {AssistantPanel::tr("Evening, %1."), AssistantPanel::tr("Hey %1. What's the plan tonight"), AssistantPanel::tr("Evening %1, still at it?")};
        return lines.at(QRandomGenerator::global()->bounded(int(lines.size()))).arg(name);
    }

    // Rises into place with a spring (save the painter first)
    // a rectangle with its left and right corners rounded apart (connected button groups)
    QPainterPath sidedRect(const QRectF& r, double left, double right) {
        QPainterPath path;
        path.moveTo(r.left() + left, r.top());
        path.lineTo(r.right() - right, r.top());
        path.arcTo(QRectF(r.right() - 2 * right, r.top(), 2 * right, 2 * right), 90, -90);
        path.lineTo(r.right(), r.bottom() - right);
        path.arcTo(QRectF(r.right() - 2 * right, r.bottom() - 2 * right, 2 * right, 2 * right), 0, -90);
        path.lineTo(r.left() + left, r.bottom());
        path.arcTo(QRectF(r.left(), r.bottom() - 2 * left, 2 * left, 2 * left), 270, -90);
        path.lineTo(r.left(), r.top() + left);
        path.arcTo(QRectF(r.left(), r.top(), 2 * left, 2 * left), 180, -90);
        path.closeSubpath();
        return path;
    }

    QString plainOf(const QList<AnswerToken>& tokens, const QString& lineBreak) {
        QString plain;
        for (const AnswerToken& token : tokens) {
            if (token.kind == 4) {
                plain = plain.trimmed() + lineBreak;
                continue;
            }
            if (token.glued && plain.endsWith(' ')) plain.chop(1);
            plain += token.text + ' ';
        }
        return plain.trimmed();
    }

    // the sky drawn in Material shapes, centred on the origin
    void paintSky(QPainter& p, const QString& kind, double size, const M3::Scheme& s) {
        p.setPen(Qt::NoPen);
        auto shape = [&](const m3shapes::RoundedPolygon& polygon, const QRectF& r, const QColor& c) {
            p.setBrush(c);
            p.drawPath(m3shapes::toPath(polygon, r));
        };
        auto at = [size](double x, double y, double w) { return QRectF((x - w / 2) * size, (y - w / 2) * size, w * size, w * size); };
        const bool wet = kind == "rain" || kind == "drizzle" || kind == "storm";
        if (kind == "clear") shape(MaterialShapes::sunny(), at(0, 0, 1), s.tertiary);
        else if (kind == "night" || kind == "partlynight") {
            // a crescent: a circle with a bite out of it
            const bool cloud = kind == "partlynight";
            QPainterPath moon, bite;
            moon.addEllipse(cloud ? at(0.18, -0.18, 0.6) : at(0, 0, 0.8));
            bite.addEllipse(cloud ? at(0.36, -0.32, 0.52) : at(0.24, -0.18, 0.7));
            p.setBrush(s.primary);
            p.drawPath(moon.subtracted(bite));
            if (cloud) shape(MaterialShapes::puffy(), at(-0.1, 0.12, 0.78), s.secondary);
        } else if (kind == "partly") {
            shape(MaterialShapes::sunny(), at(0.18, -0.18, 0.7), s.tertiary);
            shape(MaterialShapes::puffy(), at(-0.1, 0.12, 0.78), s.secondary);
        } else if (kind == "fog") {
            for (int i = 0; i < 3; i++) shape(MaterialShapes::pill(), QRectF(-0.45 * size + i * 0.06 * size, (-0.32 + i * 0.26) * size, 0.84 * size, 0.18 * size), s.secondary);
        } else if (kind == "snow") shape(MaterialShapes::flower(), at(0, 0, 0.9), s.onSurfaceVariant);
        else shape(MaterialShapes::puffy(), wet ? at(0, -0.14, 0.86) : at(0, 0, 0.9), s.secondary);
        if (wet) {
            for (int i = 0; i < 3; i++) {
                const QRectF drop(at(-0.24 + i * 0.24, 0.36, 0.12).adjusted(0, 0, 0, kind == "drizzle" ? 0 : 0.1 * size));
                shape(MaterialShapes::pill(), drop, s.primary);
            }
        }
        if (kind == "storm") shape(MaterialShapes::boom(), at(0.22, 0.26, 0.42), s.tertiary);
    }

    // Live numbers for the "what's busy" tiles, measured between paints (a second or so apart)
    struct Usage {
            qint64 busy = 0, total = 0, at = 0;
            double value = -1;
    };
    double cpuOfProcess(int pid) {
        static QHash<int, Usage> last;
        QFile stat(QStringLiteral("/proc/%1/stat").arg(pid));
        if (!stat.open(QFile::ReadOnly)) return -1;
        const QByteArray all = stat.readAll();
        const QList<QByteArray> fields = all.mid(all.lastIndexOf(')') + 2).split(' '); // after "pid (name) "
        if (fields.size() < 13) return -1;
        const qint64 ticks = fields.at(11).toLongLong() + fields.at(12).toLongLong(), now = QDateTime::currentMSecsSinceEpoch();
        Usage& u = last[pid];
        if (u.at > 0 && now - u.at >= 400) {
            u.value = 100.0 * (ticks - u.busy) / sysconf(_SC_CLK_TCK) / ((now - u.at) / 1000.0);
            u.busy = ticks;
            u.at = now;
        } else if (u.at == 0) {
            u.busy = ticks;
            u.at = now;
        }
        return u.value;
    }
    qint64 memoryOfProcess(int pid) {
        QFile statm(QStringLiteral("/proc/%1/statm").arg(pid));
        if (!statm.open(QFile::ReadOnly)) return -1;
        return statm.readAll().split(' ').value(1).toLongLong() * sysconf(_SC_PAGESIZE);
    }
    double cpuOfSystem() {
        static Usage last;
        QFile stat(QStringLiteral("/proc/stat"));
        if (!stat.open(QFile::ReadOnly)) return -1;
        const QList<QByteArray> f = stat.readLine().simplified().split(' '); // cpu user nice system idle iowait irq softirq steal
        qint64 total = 0;
        for (int i = 1; i < f.size() && i <= 8; i++) total += f.at(i).toLongLong();
        const qint64 idle = f.value(4).toLongLong() + f.value(5).toLongLong(), now = QDateTime::currentMSecsSinceEpoch();
        if (last.at > 0 && now - last.at >= 400 && total > last.total) {
            last.value = 100.0 * (1 - double(idle - last.busy) / (total - last.total));
            last.busy = idle;
            last.total = total;
            last.at = now;
        } else if (last.at == 0) {
            last.busy = idle;
            last.total = total;
            last.at = now;
        }
        return last.value;
    }
    QPair<qint64, qint64> memoryOfSystem() { // used, total
        QFile info(QStringLiteral("/proc/meminfo"));
        if (!info.open(QFile::ReadOnly)) return {0, 0};
        qint64 total = 0, available = 0;
        for (const QByteArray& line : info.readAll().split('\n')) {
            if (line.startsWith("MemTotal:")) total = line.simplified().split(' ').value(1).toLongLong() * 1024;
            if (line.startsWith("MemAvailable:")) available = line.simplified().split(' ').value(1).toLongLong() * 1024;
        }
        return {total - available, total};
    }

    void rise(QPainter& p, const QRectF& r, double since) {
        const double s = springAt(M3Motion::SpatialDefault, since);
        p.translate(r.center());
        p.scale(lerp(0.86, 1, s), lerp(0.86, 1, s));
        p.translate(-r.center() + QPointF(0, 24 * (1 - s)));
        p.setOpacity(p.opacity() * c01(since / 0.18));
    }
} // namespace

struct AssistantPanel::Private {
        struct Step {
                QString id, text;
                bool done = false, card = false;
                qint64 at = 0; // when it started: steps roll by in the pill
        };
        struct Item {
                enum Kind { User, Steps, Answer, Card, Chips } kind;
                qint64 born = 0;
                QString text;
                QList<qint64> wordBorn;              // Answer: when each word arrived
                QList<Step> steps;                   // Steps
                qint64 foldAt = -1;                  // Steps: folded away once the answer lands
                bool unfolded = false;               // Steps: opened again by hand
                AssistantCard card;                  // Card
                QString was;                         // Card: the subtitle before a change
                qint64 changedAt = -1;               // Card
                QList<AssistantChip> chips;          // Chips
                QString pressed;                     // Chips
                QHash<QString, double> countFrom;    // Answer: key numbers count from these (by suffix: "%" or "")
                bool greeting = false;               // Answer: said on opening, not part of the talk yet
                bool reacted = false;                // Answer: its face tag played
                bool opens = false;                  // Answer: a greeting, which starts a turn of its own
                qint64 doneAt = -1;                  // Answer: when its turn finished
                qint64 pressedAt = -1;
        };
        struct Sees {
                QString icon, label, context;
                bool dismissed = false;
                qint64 dismissedAt = -1;
        };
        struct Hit {
                QRectF rect;
                std::function<void(QPointF)> act; // with the pointer, in window coordinates
                bool drags = false;
                std::function<void(int)> nudge; // arrow keys: -1 / +1
        };

        AssistantBrain* brain;
        AssistantChunk* chunk;
        QLineEdit* field;
        QList<Item> items;
        QList<Sees> sees;
        QList<Hit> hits;
        int dragging = -1;
        int focus = -1; // keyboard focus among the hits; -1: the field
        QPointF mouse{-1, -1};
        DesktopWmWindowPtr previous;
        QImage screen;                 // what was on screen as it opened (for "this")
        QHash<QString, QImage> images; // image cards' pictures, by path or URL
        QNetworkAccessManager* network = nullptr;

        QElapsedTimer clock;
        QTimer frame;
        qint64 animateUntil = 0;
        qint64 openedAt = -1, toBarAt = -1, fromBarAt = -1, closedAt = -1;
        bool open = false, inBar = false, busy = false, harder = false, deep = false;
        qint64 busySince = 0;
        QString busyText, lastDone;
        int turnSteps = -1, turnAnswer = -1, turnChips = -1;
        M3SpringAnimation* seesShown; // the "Sees" row, hidden while it works
        // small talk: the face morphs into a shape, pops and turns on springs
        const m3shapes::RoundedPolygon* reactShape = nullptr;
        M3SpringAnimation *reactMorph, *reactScale, *reactSpin, *reactDim;
        int reactGeneration = 0;
        M3SpringAnimation* carousel;   // which card is the large one
        int viewBack = 0;              // turns flipped back from the latest
        qint64 flipAt = -1;
        int flipDir = 0;
        bool listOpen = false;        // the earlier turns, as a list
        bool sheet = false;           // circle to search: a sheet at the bottom instead of the pill at the top
        // layout on springs: where things go glides there instead of jumping
        struct Glide {
                double value = 0, velocity = 0, target = 0;
                M3Motion::Token token = M3Motion::SpatialDefault;
                qint64 used = 0;
        };
        QHash<QString, Glide> glides;
        int leavingStart = -1;        // the turn on its way out as a new one starts
        qint64 leftAt = -1;
        double leavingPos = 0;
        double echoFrom = 0;          // how far above the stage the pill's words are
        const m3shapes::RoundedPolygon* burstShape = nullptr; // small talk: little shapes pop off the face
        qint64 burstAt = -1;
        qint64 listAt = -1;
        qint64 sendPressedAt = -1;
        QList<QRectF> solid;           // what's drawn; a click anywhere else closes
        bool secondTick = false;       // a countdown asked for the next second's frame
        int armedPid = -1;             // "what's busy": a Close pressed once, waiting for the second press
        qint64 armedAt = -1;
        QSet<int> closedPids;
        QVariantAnimation* leave;

        double scroll = 0, scrollVelocity = 0, scrollTarget = 0;
        double contentHeight = 0, viewHeight = 0;
        bool stickToEnd = false;
        QHash<QString, double> lastNumbers; // the last key number per suffix, so the next one rolls from it
        bool batteryNudged = false;

        // The first run: a short tour in theDesk's onboarding style
        int introStep = -1;           // -1: not touring; 0 hello, 1 sees, 2 asks, 3 colour, 4 try
        qint64 introAt = 0;           // when this step started
        qint64 introToPanelAt = -1;   // the card turning into the panel
        qint64 introSeesGoneAt = -1;
        int introAsk = 0;             // the demo ask: 0 asking, 1 allowed, -1 not
        qint64 introAskAt = -1;
        int introPick = 0;            // primary, secondary (Calm), tertiary (Bright), ink
        qint64 introPickAt = -1;

        qint64 now() const { return clock.elapsed(); }
        double since(qint64 t) const { return t < 0 ? -1 : (now() - t) / 1000.0; }
};

AssistantPanel* AssistantPanel::instance() {
    static auto* panel = new AssistantPanel();
    return panel;
}

void AssistantPanel::setUp() {
    connect(StateManager::hudManager(), &HudManager::actionTriggered, instance(), [](QString id) {
        if (id == "pebble-show") instance()->open();
        else if (id.startsWith(QStringLiteral("pebble-ask:"))) {
            // a nudge's button: Pebble opens with the job asked
            instance()->open();
            instance()->sendText(id.mid(11));
        }
        else if (id == "pebble-dim" && AssistantTools::instance()->has(QStringLiteral("set_brightness"))) AssistantTools::instance()->tool(QStringLiteral("set_brightness")).run(QJsonObject{{"percent", 40}});
    });
    // a nudge at low battery: Pebble offers to dim the screen
    QDBusConnection::systemBus().connect(QStringLiteral("org.freedesktop.UPower"), QStringLiteral("/org/freedesktop/UPower/devices/DisplayDevice"),
        QStringLiteral("org.freedesktop.DBus.Properties"), QStringLiteral("PropertiesChanged"), instance(), SLOT(batteryChanged()));
    PebbleSelectionButton::setUp();
    PebbleNudges::setUp();
    PebbleLive::setUp([] {
        instance()->open();
    });
    CircleSearch::setUp();
    // the screenshot editor's Ask Pebble
    connect(AssistantTools::instance(), &AssistantTools::askRequested, instance(), &AssistantPanel::openWithScreenshot);
    StateManager::statusCenterManager()->addPane(new PebbleSettings(), StatusCenterManager::SystemSettings);
    connect(new KeyGrab(QKeySequence(Qt::MetaModifier | Qt::Key_Space), QStringLiteral("assistant")), &KeyGrab::activated, instance(), [] {
        AssistantPanel* panel = instance();
        if (panel->d->introStep == 4) panel->finishIntro(); // "now you try" — they did
        else if (panel->d->open) panel->close();
        else panel->open();
    });
}

AssistantPanel::AssistantPanel() :
    QDialog(nullptr) {
    d = new Private();
    this->setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    this->setAttribute(Qt::WA_TranslucentBackground);
    this->setMouseTracking(true);
    this->setWindowTitle(Name);
    d->clock.start();

    d->field = new QLineEdit(this);
    d->field->setFrame(false);
    d->field->setStyleSheet(QStringLiteral("QLineEdit { background: transparent; border: none; }"));
    d->field->setFont(flex(17, 450));
    d->field->setPlaceholderText(tr("Ask %1").arg(Name));
    d->field->installEventFilter(this);
    connect(d->field, &QLineEdit::textChanged, this, [this] {
        animateFor(400);
    });

    d->frame.setInterval(16);
    connect(&d->frame, &QTimer::timeout, this, &AssistantPanel::tick);
    d->seesShown = new M3SpringAnimation(this);
    d->seesShown->setToken(M3Motion::SpatialDefault);
    d->seesShown->snapTo(1.0);
    d->carousel = new M3SpringAnimation(this);
    d->carousel->setToken(M3Motion::SpatialDefault);
    d->carousel->snapTo(0.0);
    connect(d->carousel, &M3SpringAnimation::valueChanged, this, [this] {
        animateFor(100);
    });
    connect(d->seesShown, &M3SpringAnimation::valueChanged, this, [this] {
        animateFor(100);
    });
    const std::tuple<M3SpringAnimation**, M3Motion::Token, double> reactSprings[]{{&d->reactMorph, M3Motion::SpatialDefault, 0.0}, {&d->reactScale, M3Motion::SpatialFast, 1.0}, {&d->reactSpin, M3Motion::SpatialDefault, 0.0}, {&d->reactDim, M3Motion::EffectsDefault, 0.0}};
    for (auto [spring, token, rest] : reactSprings) {
        *spring = new M3SpringAnimation(this);
        (*spring)->setToken(token);
        (*spring)->snapTo(rest);
        connect(*spring, &M3SpringAnimation::valueChanged, this, [this] {
            animateFor(100);
        });
    }

    d->leave = new QVariantAnimation(this);
    d->leave->setStartValue(1.0);
    d->leave->setEndValue(0.0);
    d->leave->setDuration(120);
    connect(d->leave, &QVariantAnimation::valueChanged, this, [this](const QVariant& value) {
        this->setWindowOpacity(value.toDouble());
    });
    connect(d->leave, &QVariantAnimation::finished, this, [this] {
        if (d->open) return;
        QDialog::hide();
        this->setWindowOpacity(1);
        sheetGone();
    });

    d->chunk = new AssistantChunk();
    connect(d->chunk, &Chunk::clicked, this, &AssistantPanel::open);

    // TD_ASSISTANT_DEMO=1: the scripted stand-in (for tests; it changes nothing)
    if (qEnvironmentVariableIsSet("TD_ASSISTANT_DEMO")) d->brain = new DemoBrain(this);
    else d->brain = new ClaudeBrain(this);
    connect(d->brain, &AssistantBrain::stepStarted, this, [this](QString id, QString text) {
        if (d->turnSteps < 0) {
            d->items.append({Private::Item::Steps, d->now()});
            d->turnSteps = int(d->items.size()) - 1;
        }
        Private::Step step{id, text};
        step.at = d->now();
        d->items[d->turnSteps].steps.append(step);
        d->busyText = QString(text).remove(QStringLiteral("…"));
        if (d->inBar) d->chunk->setState(false, d->busyText);
        scrollToEnd();
    });
    connect(d->brain, &AssistantBrain::stepFinished, this, [this](QString id, QString text, bool card) {
        if (d->turnSteps < 0) return;
        for (Private::Step& step : d->items[d->turnSteps].steps) {
            if (step.id != id) continue;
            step.done = true;
            step.card = card;
            step.text = text;
        }
        d->lastDone = text;
        animateFor(800);
    });
    connect(d->brain, &AssistantBrain::textArrived, this, [this](QString part) {
        // The answer lands: the steps fold away (the cards show what they did)
        if (d->turnSteps >= 0 && d->items[d->turnSteps].foldAt < 0) d->items[d->turnSteps].foldAt = d->now();
        if (d->turnAnswer < 0) {
            d->items.append({Private::Item::Answer, d->now()});
            d->turnAnswer = int(d->items.size()) - 1;
        }
        if (d->items[d->turnAnswer].countFrom.isEmpty()) d->items[d->turnAnswer].countFrom = d->lastNumbers;
        Private::Item& answer = d->items[d->turnAnswer];
        answer.text += part;
        const ParsedAnswer parsed = parseAnswer(answer.text);
        if (!parsed.face.isEmpty() && !answer.reacted) { // the brain's own face tag: the face reacts as the words land
            answer.reacted = true;
            react(parsed.face);
        }
        const int words = int(parsed.tokens.size());
        while (answer.wordBorn.size() < words) {
            const qint64 last = answer.wordBorn.isEmpty() ? 0 : answer.wordBorn.last();
            // a steady beat, quicker when words pile up so the end doesn't lag
            answer.wordBorn.append(qMax(d->now(), last + (last > d->now() + 250 ? 22 : 65)));
        }
        scrollToEnd();
        animateFor(int(answer.wordBorn.isEmpty() ? 0 : answer.wordBorn.last() - d->now()) + 1500); // words queued up land later
    });
    connect(d->brain, &AssistantBrain::cardAdded, this, [this](AssistantCard card) {
        qint64 born = d->now();
        if (!d->items.isEmpty() && d->items.last().kind == Private::Item::Card) born = qMax(born, d->items.last().born + 100);
        Private::Item item{Private::Item::Card, born};
        item.card = card;
        d->items.append(item);
        if (card.kind == AssistantCard::Ask) d->carousel->animateTo(0.0); // an ask comes first
        scrollToEnd();
    });
    connect(d->brain, &AssistantBrain::cardChanged, this, [this](AssistantCard card) {
        for (Private::Item& item : d->items) {
            if (item.kind != Private::Item::Card || item.card.id != card.id) continue;
            item.was = item.card.subtitle;
            item.card = card;
            item.changedAt = d->now();
        }
        animateFor(1200);
    });
    connect(d->brain, &AssistantBrain::chipsOffered, this, [this](QList<AssistantChip> chips) {
        if (d->turnChips >= 0) {
            d->items[d->turnChips].chips = chips;
        } else {
            Private::Item item{Private::Item::Chips, d->now() + 150};
            item.chips = chips;
            d->items.append(item);
            d->turnChips = int(d->items.size()) - 1;
        }
        scrollToEnd();
    });
    connect(d->brain, &AssistantBrain::deepChanged, this, [this](bool deep) {
        d->deep = deep;
        animateFor(300);
    });
    connect(d->brain, &AssistantBrain::turnFinished, this, [this] {
        d->busy = false;
        if (d->turnSteps >= 0 && d->items[d->turnSteps].foldAt < 0) d->items[d->turnSteps].foldAt = d->now();
        if (d->turnAnswer >= 0) {
            Private::Item& answer = d->items[d->turnAnswer];
            answer.doneAt = d->now();
            // a long answer's line folds away once it's all there: keep drawing till then
            if (!answer.wordBorn.isEmpty()) animateFor(int(qMax<qint64>(0, answer.wordBorn.last() - d->now())) + LineGoesAfterMs + 1500);
        }
        if (d->turnAnswer >= 0) {
            static const QRegularExpression keyNumber(QStringLiteral("\\*\\*(\\d+)(%?)\\*\\*"));
            auto it = keyNumber.globalMatch(d->items[d->turnAnswer].text);
            while (it.hasNext()) {
                const QRegularExpressionMatch m = it.next();
                d->lastNumbers.insert(m.captured(2), m.captured(1).toDouble());
            }
        }
        if (d->inBar || d->toBarAt >= 0) { // or still on its way there
            d->chunk->setState(true, d->lastDone.isEmpty() ? tr("Answer ready") : tr("Done · %1").arg(d->lastDone));
            // a nudge: it finished while they were elsewhere
            if (tSettings().value(QStringLiteral("Assistant/nudges")).toBool()) {
                DeskSound::play(QStringLiteral("complete"));
                StateManager::hudManager()->showHud({
                    {QStringLiteral("icon"), QStringLiteral("dialog-information")},
                    {QStringLiteral("title"), tr("Pebble's done")},
                    {QStringLiteral("text"), d->lastDone.isEmpty() ? tr("Your answer is ready") : d->lastDone},
                    {QStringLiteral("action"), tr("Show")},
                    {QStringLiteral("actionId"), QStringLiteral("pebble-show")}
                });
            }
        }
        animateFor(1000);
    });
}

QRectF AssistantPanel::sheetRect() const {
    // all Pebble draws on: the pill and the stage under it (as a sheet: the bottom part of the screen)
    if (d->sheet) return QRectF((this->width() - stageWidth()) / 2 - 24, this->height() * 0.4 - 60, stageWidth() + 48, this->height() * 0.6 + 60);
    const QRectF pill = pillRect();
    const double w = qMax(pill.width(), stageWidth());
    return QRectF((this->width() - w) / 2 - 24, pill.top() - 24, w + 48, this->height() - pill.top());
}

void AssistantPanel::open() {
    const bool fromBar = d->inBar;
    if (!fromBar) {
        d->previous = DesktopWm::activeWindow();
        // a new thread after a while away
        if (!d->busy && d->closedAt >= 0 && d->now() - d->closedAt > FreshThreadAfterMs) {
            d->items.clear();
            d->turnSteps = d->turnAnswer = d->turnChips = -1;
            d->brain->forget();
            d->field->setPlaceholderText(tr("Ask %1").arg(Name));
        }
        // What it may see, as chips under the field (each can be dismissed)
        d->sees.clear();
        d->screen = QImage();
        if (QScreen* screen = QGuiApplication::primaryScreen()) d->screen = screen->grabWindow(0).toImage();
        if (!d->screen.isNull()) d->sees.append({QStringLiteral("screenshot_monitor"), tr("Screen"), {}});
        if (d->previous) {
            QString app;
            bool terminal = false;
            if (ApplicationPointer application = d->previous->application()) {
                app = application->getProperty(QStringLiteral("Name")).toString();
                terminal = application->getStringList(QStringLiteral("Categories")).contains(QStringLiteral("TerminalEmulator"));
            }
            const QString title = d->previous->title();
            QString label = app.isEmpty() ? title : (title.isEmpty() || title == app ? app : app + QStringLiteral(" · ") + title);
            if (label.size() > 34) label = label.left(33) + QStringLiteral("…");
            if (!label.isEmpty()) d->sees.append({terminal ? QStringLiteral("terminal") : QStringLiteral("web_asset"), label, QStringLiteral("Active window: %1 — %2").arg(app, title)});
        }
        QClipboard* clipboard = QGuiApplication::clipboard();
        const QMimeData* mime = clipboard->mimeData();
        const bool secret = mime && mime->data(QStringLiteral("x-kde-passwordManagerHint")) == "secret";
        const QString copied = clipboard->text();
        if (mime && mime->hasImage()) d->sees.append({QStringLiteral("image"), tr("Copied image"), QStringLiteral("Clipboard: an image")});
        else if (!copied.trimmed().isEmpty() && !secret && !ClipboardHistory::looksLikePassword(copied)) d->sees.append({QStringLiteral("content_paste"), tr("Copied text"), QStringLiteral("Clipboard:\n") + copied.left(4000)});
        const QString selected = clipboard->text(QClipboard::Selection);
        if (!selected.trimmed().isEmpty() && selected != copied && !ClipboardHistory::looksLikePassword(selected)) d->sees.append({QStringLiteral("select"), tr("Selection"), QStringLiteral("Selected text:\n") + selected.left(4000)});
    }

    d->open = true;
    d->inBar = false;
    d->focus = -1;
    d->leave->stop();
    this->setWindowOpacity(1);
    if (QScreen* screen = QGuiApplication::primaryScreen()) this->setGeometry(screen->geometry());
    if (!Compositor::isActive()) {
        QPainterPath round;
        round.addRoundedRect(sheetRect(), 28, 28);
        this->setMask(QRegion(round.toFillPolygon().toPolygon()));
    }
    else this->clearMask();
    if (StateManager::barManager() && d->chunk->chunkRegistered()) StateManager::barManager()->removeChunk(d->chunk);
    d->toBarAt = -1;
    if (fromBar) d->fromBarAt = d->now();
    else d->openedAt = d->now();
    d->seesShown->snapTo(d->busy ? 0.0 : 1.0);

    // the field's colours follow the scheme
    const M3::Scheme s = M3::currentScheme();
    QPalette palette = d->field->palette();
    palette.setColor(QPalette::Text, s.onSurface);
    palette.setColor(QPalette::PlaceholderText, s.onSurfaceVariant);
    palette.setColor(QPalette::Highlight, s.primaryContainer);
    palette.setColor(QPalette::HighlightedText, s.onPrimaryContainer);
    d->field->setPalette(palette);
    d->field->show();

    QDialog::show();
    this->raise();
    QDialog::activateWindow();
    d->field->setFocus();
    DesktopWm::setSystemWindow(this, DesktopWm::SystemWindowTypeMenu);
    d->brain->warm(); // starting takes a moment: do it while they type
    if (!tSettings().value(QStringLiteral("Assistant/introduced")).toBool()) startIntro();
    else if (!fromBar && !d->sheet) greet();
    animateFor(1000);
}

void AssistantPanel::batteryChanged() {
    QDBusInterface device(QStringLiteral("org.freedesktop.UPower"), QStringLiteral("/org/freedesktop/UPower/devices/DisplayDevice"), QStringLiteral("org.freedesktop.UPower.Device"), QDBusConnection::systemBus());
    const double percent = device.property("Percentage").toDouble();
    const bool draining = device.property("State").toUInt() == 2;
    if (!draining || percent > 20) {
        d->batteryNudged = false; // charging again: it may nudge next time
        return;
    }
    if (d->batteryNudged || percent > 15 || !tSettings().value(QStringLiteral("Assistant/nudges")).toBool()) return;
    d->batteryNudged = true;
    StateManager::hudManager()->showHud({
        {QStringLiteral("icon"), QStringLiteral("battery-caution")},
        {QStringLiteral("title"), tr("Battery's at %1%").arg(qRound(percent))},
        {QStringLiteral("text"), tr("Pebble can dim the screen to stretch it")},
        {QStringLiteral("action"), tr("Dim it")},
        {QStringLiteral("actionId"), QStringLiteral("pebble-dim")}
    });
}

void AssistantPanel::openWithScreenshot(const QImage& image) {
    // what it sees is the screenshot they just took, in place of the screen
    open();
    d->screen = image;
    d->sees.removeIf([](const Private::Sees& seen) {
        return seen.context.isEmpty();
    });
    d->sees.prepend({QStringLiteral("screenshot_region"), tr("Screenshot"), {}});
    d->field->setPlaceholderText(tr("Ask about this screenshot"));
    this->update();
}

void AssistantPanel::openAndAsk(const QString& text) {
    open();
    sendText(text);
}

void AssistantPanel::openWithSelection() {
    // from the face next to selected text: open, with a few things to do with it
    open();
    if (!d->items.isEmpty() && d->items.last().greeting) d->items.removeLast();
    Private::Item item{Private::Item::Chips, d->now() + 200};
    item.chips = {{"explain", tr("Explain this"), "lightbulb"}, {"summarise", tr("Summarise it"), "short_text"}, {"rewrite", tr("Rewrite it nicer"), "edit"}, {"translate", tr("Translate to English"), "translate"}};
    d->items.append(item);
    d->turnChips = -1;
    scrollToEnd();
}

void AssistantPanel::say(const QString& raw, bool isGreeting) {
    // Pebble saying something by itself (not the brain): same living type
    Private::Item item{Private::Item::Answer, d->now()};
    item.text = raw;
    item.greeting = isGreeting;
    item.opens = isGreeting;
    const int words = int(parseAnswer(raw).tokens.size());
    for (int i = 0; i < words; i++) item.wordBorn.append(d->now() + 150 + i * 65);
    d->items.append(item);
    scrollToEnd();
}

void AssistantPanel::greet() {
    // a greeting waiting for an answer is swapped, not stacked
    if (!d->items.isEmpty() && d->items.last().greeting) d->items.removeLast();
    const bool talking = !d->items.isEmpty();
    // mid-talk, only after a little while away
    if (talking && d->closedAt >= 0 && d->now() - d->closedAt < 2 * 60 * 1000) return;
    say(greeting(talking), true);
}

void AssistantPanel::startIntro() {
    d->introStep = 0;
    d->introAt = d->now();
    d->introToPanelAt = -1;
    d->introSeesGoneAt = d->introAskAt = d->introPickAt = -1;
    d->introAsk = 0;
    d->field->hide();
    this->setFocus();
    animateFor(6000);
}

void AssistantPanel::introNext() {
    if (d->introStep < 0 || d->introStep >= 4) return;
    d->introStep++;
    d->introAt = d->now();
    animateFor(3000);
}

void AssistantPanel::introBack() {
    if (d->introStep <= 0) return;
    d->introStep--;
    d->introAt = d->now();
    animateFor(3000);
}

void AssistantPanel::finishIntro() {
    if (d->introStep < 0) return;
    tSettings().setValue(QStringLiteral("Assistant/introduced"), true);
    d->introStep = -1;
    d->introToPanelAt = d->now();
    d->openedAt = -1;
    // its first answer, without asking the brain anything
    const QString name = firstName().isEmpty() ? QString() : QStringLiteral(", **%1**").arg(firstName());
    say(tr("{done} All set%1. I'm here whenever you press Super+Space. Just ask, like you'd text a friend.").arg(name), false);
    for (qint64& born : d->items.last().wordBorn) born += 500; // once the card has become the sheet
    QTimer::singleShot(450, this, [this] {
        d->field->show();
        d->field->setFocus();
    });
    animateFor(2500);
}

void AssistantPanel::close() {
    if (!d->open) return;
    if (d->introStep >= 0) {
        d->introStep = -1;
        d->field->show();
    }
    if (d->busy) {
        toBar();
        return;
    }
    d->open = false;
    d->closedAt = d->now();
    if (Compositor::isActive() && M3Motion::expressive()) d->leave->start();
    else {
        QDialog::hide();
        sheetGone();
    }
}

void AssistantPanel::sheetGone() {
    // circle to search goes back to circling
    if (!d->sheet) return;
    d->sheet = false;
    emit sheetClosed();
}

void AssistantPanel::openForCircle(const QImage& image, const QString& question) {
    d->sheet = true;
    openWithScreenshot(image);
    d->field->setPlaceholderText(tr("Ask a follow-up"));
    if (!d->busy) sendText(question);
}

void AssistantPanel::toBar() {
    if (d->toBarAt >= 0) return;
    if (StateManager::barManager() && !d->chunk->chunkRegistered()) StateManager::barManager()->addChunk(d->chunk);
    d->chunk->setState(false, d->busyText.isEmpty() ? tr("Working") : d->busyText);
    d->toBarAt = d->now();
    d->field->hide();
    animateFor(700);
    QTimer::singleShot(Compositor::isActive() && M3Motion::expressive() ? 480 : 0, this, [this] {
        if (d->toBarAt < 0) return; // opened again meanwhile
        d->open = false;
        d->inBar = true;
        d->toBarAt = -1;
        d->closedAt = d->now();
        QDialog::hide();
        sheetGone();
    });
}

void AssistantPanel::send() {
    d->sendPressedAt = d->now();
    sendText(d->field->text());
}

void AssistantPanel::sendText(QString text) {
    text = text.trimmed();
    if (text.isEmpty() || d->busy) return;
    if (smallTalk(text)) return;
    d->field->clear();
    d->field->setPlaceholderText(tr("Ask %1 more").arg(Name));
    QStringList context;
    bool screen = false;
    for (const Private::Sees& seen : std::as_const(d->sees)) {
        if (seen.dismissed) continue;
        if (seen.context.isEmpty()) screen = true; // the screen goes as a picture, when it asks
        else context.append(seen.context);
        // a screenshot they took to ask about: the question is about it
        if (seen.icon == QStringLiteral("screenshot_region")) context.append(QStringLiteral("They just took a screenshot to ask about; \"this\" and \"it\" mean the screenshot. Look at it with look_at_screen first."));
    }
    d->brain->setScreen(screen ? d->screen : QImage());
    if (!d->items.isEmpty()) d->items.last().greeting = false;
    markLeaving();
    d->items.append({Private::Item::User, d->now(), text});
    d->turnSteps = d->turnAnswer = d->turnChips = -1;
    d->viewBack = 0;
    d->flipAt = -1;
    d->listOpen = false;
    d->carousel->snapTo(0.0);
    d->scroll = d->scrollTarget = 0;
    d->busy = true;
    d->busySince = d->now();
    d->busyText.clear();
    d->lastDone.clear();
    d->seesShown->animateTo(0.0);
    scrollToEnd();
    d->brain->send(text, context, d->harder);
    d->harder = false;
}

bool AssistantPanel::smallTalk(const QString& text) {
    // Pebble asked something: whatever comes back is the answer, for the brain
    if (!d->items.isEmpty() && d->items.last().kind == Private::Item::Answer && !d->items.last().greeting && d->items.last().text.trimmed().endsWith('?')) return false;
    // "Heyyy pebble!!" -> "heyyy"
    QString said = text.toLower();
    said.remove(QRegularExpression(QStringLiteral("[^a-z<3 ]")));
    said.remove(QRegularExpression(QStringLiteral("\\b%1\\b").arg(Name.toLower())));
    said = said.simplified();
    static const QList<std::tuple<QRegularExpression, QString, const char*>> talk{
        // each reply a few ways, picked at random so it doesn't sound canned
        {QRegularExpression(QStringLiteral("^(h+i+|he+y+|hel+o+|yo+|su+p+|hiya|howdy|hey there|hi there|wassup|whats up)$")), QStringLiteral("hi"), QT_TR_NOOP("hey :)|hiii ^^|yo o/|hey hey :D")},
        {QRegularExpression(QStringLiteral("^(thanks?( you| a lot| so much)?|thank u|thx|ty+|tysm|tyvm)$")), QStringLiteral("thanks"), QT_TR_NOOP("np <3|anytime :)|np ^^|ofc :D")},
        {QRegularExpression(QStringLiteral("^(i )?(love (you|u)|ily|ilysm|<3+)$")), QStringLiteral("love"), QT_TR_NOOP("<3|<333|aww <3|^_^ <3")},
        {QRegularExpression(QStringLiteral("^(gn+|good ?night|night|nighty? ?night|gnight)$")), QStringLiteral("gn"), QT_TR_NOOP("night :)|gn ^^|sleep well <3|night night o/")},
        {QRegularExpression(QStringLiteral("^(l+o+l+|lmf?a+o+|(ha){2,}h?|ha+h+a+|he(he)+|xd+|rofl)$")), QStringLiteral("lol"), QT_TR_NOOP("hehe :D|xD|haha :P|hehe ^^")},
        {QRegularExpression(QStringLiteral("^(bye+( bye)?|cya|see (ya|you)|ttyl|goodbye)$")), QStringLiteral("bye"), QT_TR_NOOP("bye o/|cya :)|later ^^|bye bye <3")},
    };
    for (const auto& [words, kind, reply] : talk) {
        if (!words.match(said).hasMatch()) continue;
        d->field->clear();
        if (!d->items.isEmpty()) d->items.last().greeting = false;
        markLeaving();
        d->items.append({Private::Item::User, d->now(), text});
        d->turnSteps = d->turnAnswer = d->turnChips = -1;
        d->viewBack = 0;
        d->flipAt = -1;
        d->listOpen = false;
        d->carousel->snapTo(0.0);
        react(kind);
        const QStringList ways = tr(reply).split('|');
        const QString said = ways.at(QRandomGenerator::global()->bounded(int(ways.size())));
        QTimer::singleShot(M3Motion::expressive() ? 260 : 120, this, [this, said] {
            say(said, false);
        });
        scrollToEnd();
        return true;
    }
    return false;
}

void AssistantPanel::react(const QString& kind) {
    // M3E: the shape morphs and pops on spatial springs, colour eases on effects ones
    static const QHash<QString, std::pair<const m3shapes::RoundedPolygon* (*)(), double>> shapes{
        {QStringLiteral("hi"), {[] { return &MaterialShapes::flower(); }, 90}},
        {QStringLiteral("thanks"), {[] { return &MaterialShapes::heart(); }, 0}},
        {QStringLiteral("love"), {[] { return &MaterialShapes::heart(); }, 0}},
        {QStringLiteral("gn"), {[] { return &MaterialShapes::pill(); }, 0}},
        {QStringLiteral("lol"), {[] { return &MaterialShapes::softBurst(); }, 45}},
        {QStringLiteral("bye"), {[] { return &MaterialShapes::clover4Leaf(); }, -90}},
    };
    const auto [shape, turn] = shapes.value(kind);
    const int generation = ++d->reactGeneration;
    d->reactShape = shape();
    d->burstShape = kind == "gn" ? nullptr : kind == "hi" ? &MaterialShapes::sunny() : d->reactShape;
    d->burstAt = d->now();
    animateFor(1900);
    d->reactMorph->animateTo(1.0);
    d->reactSpin->snapTo(0.0);
    d->reactSpin->animateTo(turn);
    auto pop = [this](int at, double to) {
        QTimer::singleShot(at, this, [this, to] {
            d->reactScale->animateTo(to);
        });
    };
    pop(0, 1.22);
    pop(140, 1.0);
    if (kind == "thanks" || kind == "love") { // a heartbeat
        pop(520, 1.16);
        pop(640, 1.0);
    }
    if (kind == "lol") // laughing: the burst kicks round a few more times
        for (int i = 1; i <= 3; i++) QTimer::singleShot(i * 170, this, [this, i] {
            d->reactSpin->animateTo(45.0 * (i + 1));
        });
    d->reactDim->animateTo(kind == "gn" ? 1.0 : 0.0); // night: the face dims, eye shut
    // then the face settles back into itself
    QTimer::singleShot(kind == "gn" ? 2600 : 1700, this, [this, generation] {
        if (generation != d->reactGeneration) return;
        d->reactMorph->animateTo(0.0);
        d->reactDim->animateTo(0.0);
    });
    animateFor(400);
}

void AssistantPanel::animateFor(int ms) {
    d->animateUntil = qMax(d->animateUntil, d->now() + ms);
    if (!d->frame.isActive()) d->frame.start();
    redraw();
}

void AssistantPanel::redraw() {
    // the whole screen only while the scrim or the sheet's place moves; else just the sheet (picom recomposites what changes)
    auto recent = [this](qint64 at) { return at >= 0 && d->since(at) < 0.9; };
    if (d->introStep >= 0 || d->toBarAt >= 0 || recent(d->openedAt) || recent(d->fromBarAt) || recent(d->introToPanelAt)) this->update();
    else this->update(sheetRect().toAlignedRect().adjusted(-16, -16, 16, 16));
}

void AssistantPanel::scrollToEnd() {
    animateFor(1200);
}

void AssistantPanel::tick() {
    // the scroll springs toward its target
    const double max = qMax(0.0, d->contentHeight - d->viewHeight);
    if (d->stickToEnd) d->scrollTarget = max;
    d->scrollTarget = qBound(0.0, d->scrollTarget, max);
    const M3Motion::SpringState state = M3Motion::stepSpring(d->scroll, d->scrollVelocity, d->scrollTarget, M3Motion::spring(M3Motion::SpatialFast), 0.016);
    d->scroll = state.value;
    d->scrollVelocity = state.velocity;
    bool settled = std::abs(d->scroll - d->scrollTarget) < 0.5 && std::abs(d->scrollVelocity) < 1;
    if (settled) d->scroll = d->scrollTarget;
    for (auto glide = d->glides.begin(); glide != d->glides.end();) {
        if (d->now() - glide->used > 5000) { // not drawn for a while
            glide = d->glides.erase(glide);
            continue;
        }
        const M3Motion::SpringState next = M3Motion::stepSpring(glide->value, glide->velocity, glide->target, M3Motion::spring(glide->token), 0.016);
        glide->value = next.value;
        glide->velocity = next.velocity;
        if (std::abs(glide->value - glide->target) < 0.3 && std::abs(glide->velocity) < 1) {
            glide->value = glide->target;
            glide->velocity = 0;
        } else settled = false;
        ++glide;
    }
    redraw();
    if ((d->now() > d->animateUntil && !d->busy && settled) || this->isHidden()) d->frame.stop();
}

void AssistantPanel::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event)
    const M3::Scheme s = M3::currentScheme();
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    d->hits.clear();

    // the tour, and the moment its card turns into the sheet
    if (d->introStep >= 0 || (d->introToPanelAt >= 0 && d->since(d->introToPanelAt) < 0.45)) {
        if (Compositor::isActive()) {
            p.setCompositionMode(QPainter::CompositionMode_Source);
            p.fillRect(this->rect(), alpha(s.scrim, ScrimOpacity));
            p.setCompositionMode(QPainter::CompositionMode_SourceOver);
        }
        paintIntro(p);
        paintFocus(p);
        return;
    }

    // One line: a pill at the top; under it the answer as one big line,
    // cards in a carousel and the follow-ups as a connected button group
    const QRectF base = pillRect();
    QRectF bar(this->width() / 2.0 - 110, 14, 220, 34);
    if (d->chunk->isVisible() && d->chunk->width() > 0) bar = QRectF(QPointF(d->chunk->mapToGlobal(QPoint(0, 0)) - this->mapToGlobal(QPoint(0, 0))), QSizeF(d->chunk->size()));
    double toward = 0; // 0 = the pill, 1 = the chip in the bar
    if (d->toBarAt >= 0) toward = springAt(M3Motion::SpatialDefault, d->since(d->toBarAt));
    else if (d->fromBarAt >= 0) toward = 1 - springAt(M3Motion::SpatialDefault, d->since(d->fromBarAt));
    const double presence = (1 - c01(toward)) * (d->openedAt >= 0 ? c01(d->since(d->openedAt) / 0.2) : 1);
    d->solid.clear();

    if (Compositor::isActive()) {
        p.setCompositionMode(QPainter::CompositionMode_Source);
        p.fillRect(this->rect(), alpha(s.scrim, (d->sheet ? 0 : ScrimOpacity) * presence)); // as a sheet, circle to search's screen shows through
        p.setCompositionMode(QPainter::CompositionMode_SourceOver);
    } else {
        // no compositor: no see-through, so the stage gets a plain backdrop
        p.setPen(Qt::NoPen);
        p.setBrush(s.surfaceContainer);
        p.drawRoundedRect(sheetRect(), 28, 28);
    }

    // the pill grows out of the face as it opens
    QRectF pill = base;
    if (d->openedAt >= 0 && toward == 0) {
        const double k = springAt(M3Motion::SpatialDefault, d->since(d->openedAt));
        const double w = lerp(PillHeight, base.width(), k);
        pill = QRectF(base.center().x() - w / 2, base.top() - 14 * (1 - c01(k)), w, PillHeight);
        p.setOpacity(c01(d->since(d->openedAt) / 0.15));
    }
    pill = lerpRect(pill, bar, toward);
    // as a sheet: the answer area grows up out of the pill as the words and cards come
    double sheetInner = 0;
    if (d->sheet && toward < 0.5) {
        sheetInner = glide(QStringLiteral("sheetHeight"), qMin(d->contentHeight, this->height() * 0.55));
        const QRectF sheet(this->width() / 2.0 - stageWidth() / 2 - 20, pill.top() - sheetInner - 28, stageWidth() + 40, sheetInner + 28 + pill.height() + 16);
        p.setPen(Qt::NoPen);
        p.setBrush(s.surfaceContainer);
        p.drawRoundedRect(sheet, 28, 28);
        d->solid.append(sheet);
    }
    double content = 1 - c01(toward * 4);
    if (d->introToPanelAt >= 0) content *= c01((d->since(d->introToPanelAt) - 0.45) / 0.25); // after the tour, the insides fade in
    p.setPen(Qt::NoPen);
    p.setBrush(s.surfaceContainerHigh);
    p.drawRoundedRect(pill, pill.height() / 2, pill.height() / 2);
    d->solid.append(pill);
    if (content <= 0) {
        if (toward == 0) d->field->setGeometry(-2000, -2000, 10, 10);
        return;
    }
    p.setOpacity(p.opacity() * content);

    // The face, the words (a real line edit; steps roll by in its place while it works), send / stop
    const double cy = pill.center().y();
    const QRectF face(pill.left() + 12, cy - 22, 44, 44);
    const auto [faceInk, faceFill] = faceColours(s, d->deep); // thinking harder: the face turns tertiary
    const double reaction = d->reactMorph->currentValue().toDouble();
    if (d->busy) M3LoadingIndicator::paint(&p, face, d->now() - d->busySince, faceInk, faceFill);
    else if (d->reactShape && reaction > 0.002) {
        // small talk: the face's shape morphs into the reaction, pops and turns
        static QHash<const m3shapes::RoundedPolygon*, std::shared_ptr<m3shapes::Morph>> morphs;
        auto& morph = morphs[d->reactShape];
        if (!morph) morph = std::make_shared<m3shapes::Morph>(MaterialShapes::circle(), *d->reactShape);
        const double dim = d->reactDim->currentValue().toDouble();
        p.save();
        p.translate(face.center());
        p.scale(d->reactScale->currentValue().toDouble(), d->reactScale->currentValue().toDouble());
        p.setBrush(mix(faceFill, s.surfaceContainerHigh, 0.7 * dim));
        p.drawEllipse(QPointF(), face.width() / 2, face.height() / 2);
        p.rotate(d->reactSpin->currentValue().toDouble());
        p.setBrush(mix(faceInk, s.onSurfaceVariant, 0.6 * dim));
        const double inner = face.width() * 0.62;
        p.drawPath(m3shapes::toPath(*morph, float(qBound(-0.1, reaction, 1.1)), QRectF(-inner / 2, -inner / 2, inner, inner)));
        p.restore();
    } else M3LoadingIndicator::paintDeterminate(&p, face, float(lerp(0.1, 1, c01(d->field->text().size() / 40.0))), faceInk, faceFill);
    if (d->burstShape && d->burstAt >= 0) {
        const double t = d->since(d->burstAt) - 0.12;
        if (t > 0 && t < 1.6) {
            const double out = springAt(M3Motion::SpatialDefault, t), fade = 1 - springAt(M3Motion::EffectsSlow, t - 0.35);
            p.save();
            p.setPen(Qt::NoPen);
            for (int i = 0; i < 8; i++) {
                const double angle = i * M_PI / 4 + 0.3, size = (i % 2 ? 7 : 10) * c01(out);
                const QPointF at = face.center() + QPointF(std::cos(angle), std::sin(angle)) * (26 + out * 24);
                p.setBrush(alpha(i % 2 ? s.tertiary : s.primary, c01(fade)));
                p.drawPath(m3shapes::toPath(*d->burstShape, QRectF(at.x() - size / 2, at.y() - size / 2, size, size)));
            }
            p.restore();
        }
    }
    const double tx = pill.left() + 70;
    if (toward == 0 && !d->busy) d->field->setGeometry(QRectF(tx - 2, cy - 20, pill.width() - 70 - 64, 40).toRect());
    else if (toward == 0) d->field->setGeometry(-2000, -2000, 10, 10); // what they type meanwhile waits there
    if (d->busy) {
        p.save();
        p.setClipRect(QRectF(tx, pill.top() + 6, pill.width() - 70 - 70, pill.height() - 12), Qt::IntersectClip);
        // the steps roll up through the pill
        QList<Private::Step> steps;
        if (d->turnSteps >= 0 && d->turnSteps < d->items.size()) steps = d->items.at(d->turnSteps).steps;
        if (steps.isEmpty()) text(p, QRectF(tx, cy - 22, pill.width() - 150, 26), d->busyText.isEmpty() ? tr("Thinking") : d->busyText, flex(15, 500), s.onSurface);
        for (int i = qMax(0, int(steps.size()) - 2); i < steps.size(); i++) {
            const double in = springAt(M3Motion::SpatialDefault, d->since(steps.at(i).at));
            const double out = i + 1 < steps.size() ? springAt(M3Motion::SpatialDefault, d->since(steps.at(i + 1).at)) : 0;
            p.save();
            p.setOpacity(p.opacity() * c01(d->since(steps.at(i).at) / 0.2) * (1 - c01(out)));
            text(p, QRectF(tx, cy - 22 + (1 - in) * 22 - out * 22, pill.width() - 150, 26), steps.at(i).text, flex(15, 500), s.onSurface);
            p.restore();
        }
        // M3E wavy progress under it: amplitude 3, wavelength 40, stroke 4, a stop dot at the end
        const double w = qMin(300.0, pill.width() - 150), working = d->since(d->busySince);
        const double end = tx + w * (0.12 + 0.75 * (1 - std::exp(-working / 6)));
        QPainterPath wave;
        for (double x = tx; x <= end; x += 2) {
            const QPointF pt(x, cy + 14 + 3 * std::sin((x - tx) / 40.0 * 2 * M_PI - working * 6));
            if (x == tx) wave.moveTo(pt);
            else wave.lineTo(pt);
        }
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(s.primary, 4, Qt::SolidLine, Qt::RoundCap));
        p.drawPath(wave);
        p.setPen(QPen(s.secondaryContainer, 4, Qt::SolidLine, Qt::RoundCap));
        if (end + 8 < tx + w) p.drawLine(QPointF(end + 8, cy + 14), QPointF(tx + w, cy + 14));
        p.setPen(Qt::NoPen);
        p.setBrush(s.primary);
        p.drawEllipse(QPointF(tx + w, cy + 14), 2, 2);
        p.restore();
    }
    // send / stop: a circle that squashes when pressed
    const QPointF button(pill.right() - 32, cy);
    const QRectF buttonRect(button.x() - 24, button.y() - 24, 48, 48);
    const double squash = d->sendPressedAt >= 0 ? std::sin(M_PI * c01(d->since(d->sendPressedAt) / 0.22)) : 0;
    const double corner = lerp(24, 12, squash);
    if (d->busy) {
        p.setBrush(buttonRect.contains(d->mouse) ? mix(s.secondaryContainer, s.onSecondaryContainer, 0.08) : s.secondaryContainer);
        p.drawRoundedRect(buttonRect, corner, corner);
        icon(p, buttonRect, QStringLiteral("stop"), s.onSecondaryContainer, 22, true);
        d->hits.append({buttonRect, [this](QPointF) { d->brain->stop(); }});
    } else if (!d->field->text().trimmed().isEmpty() || squash > 0) {
        p.setBrush(buttonRect.contains(d->mouse) ? mix(s.primary, s.onPrimary, 0.08) : s.primary);
        p.drawRoundedRect(buttonRect, corner, corner);
        icon(p, buttonRect, QStringLiteral("arrow_upward"), s.onPrimary, 22);
        d->hits.append({buttonRect, [this](QPointF) { send(); }});
    }

    // What it sees, under the empty pill: chips that can be dismissed, and Think harder
    const QList<int> starts = turnStarts();
    bool talked = false;
    for (const Private::Item& item : std::as_const(d->items)) talked = talked || item.kind == Private::Item::User;
    const bool wantSees = !d->sheet && !d->busy && (!talked || !d->field->text().isEmpty());
    if (d->seesShown->endValue().toDouble() != (wantSees ? 1.0 : 0.0)) d->seesShown->animateTo(wantSees ? 1.0 : 0.0);
    const double seesK = c01(d->seesShown->currentValue().toDouble());
    const double top = pill.bottom() + 14;
    if (seesK > 0.01) {
        p.save();
        p.setOpacity(p.opacity() * seesK);
        p.translate(0, -8 * (1 - seesK));
        const double left = pill.left() + 8;
        const QString harderLabel = tr("Think harder");
        const double harderWidth = 24 + 26 + textWidth(harderLabel, flex(13, 600));
        const QRectF harder(pill.right() - 8 - harderWidth, top, harderWidth, 32);
        p.setPen(d->harder ? Qt::NoPen : QPen(s.outlineVariant, 1));
        QColor harderBg = d->harder ? s.tertiaryContainer : s.surfaceContainer;
        if (harder.contains(d->mouse)) harderBg = mix(harderBg, d->harder ? s.onTertiaryContainer : s.onSurface, 0.08);
        p.setBrush(harderBg);
        p.drawRoundedRect(harder.adjusted(0.5, 0.5, -0.5, -0.5), 8, 8);
        p.setPen(Qt::NoPen);
        const QColor harderInk = d->harder ? s.onTertiaryContainer : s.onSurfaceVariant;
        icon(p, QRectF(harder.left() + 8, harder.top(), 20, 32), QStringLiteral("psychology"), harderInk, 17, d->harder);
        text(p, harder.adjusted(32, 0, 0, 0), harderLabel, flex(13, 600), d->harder ? s.onTertiaryContainer : s.onSurface);
        d->hits.append({harder, [this](QPointF) {
            d->harder = !d->harder;
            animateFor(300);
        }});
        d->solid.append(harder);
        double x = left;
        // the chips share what's left before Think harder: the widest ones shrink first
        auto seenK = [this](const Private::Sees& seen) { return seen.dismissed ? 1 - c01(springAt(M3Motion::SpatialFast, d->since(seen.dismissedAt))) : 1.0; };
        auto rowWidth = [&](double cap) {
            double sum = 0;
            for (const Private::Sees& seen : std::as_const(d->sees)) sum += (qMin(chipWidth(seen.icon, seen.label) + 24, cap) + 8) * qMax(0.0, seenK(seen));
            return sum;
        };
        double cap = harder.left() - 8 - x;
        while (cap > 96 && rowWidth(cap) > harder.left() - x) cap -= 2;
        for (int i = 0; i < d->sees.size(); i++) {
            Private::Sees& seen = d->sees[i];
            const double full = qMin(chipWidth(seen.icon, seen.label) + 24, cap);
            const double k = seenK(seen);
            if (k <= 0.01) continue;
            const QRectF r(x, top, full * k, 32);
            p.save();
            p.setOpacity(p.opacity() * c01(k * 1.5));
            p.setClipRect(r.adjusted(-1, -1, 1, 1));
            p.setPen(QPen(s.outlineVariant, 1));
            p.setBrush(r.contains(d->mouse) ? mix(s.surfaceContainer, s.onSurface, 0.08) : s.surfaceContainer);
            p.drawRoundedRect(r.adjusted(0.5, 0.5, -0.5, -0.5), 8, 8);
            icon(p, QRectF(r.left() + 8, r.top(), 20, 32), seen.icon, s.onSurfaceVariant, 17);
            text(p, QRectF(r.left() + 32, r.top(), full - 56, 32), QFontMetricsF(flex(13, 520)).elidedText(seen.label, Qt::ElideRight, full - 56), flex(13, 520), s.onSurface);
            icon(p, QRectF(r.left() + full - 28, r.top(), 24, 32), QStringLiteral("close"), s.onSurfaceVariant, 16);
            p.restore();
            if (!seen.dismissed) {
                d->hits.append({r, [this, i](QPointF) {
                    d->sees[i].dismissed = true;
                    d->sees[i].dismissedAt = d->now();
                    animateFor(600);
                }});
                d->solid.append(r);
            }
            x += (full + 8) * k;
        }
        p.restore();
    }

    // The stage: the turn being looked at (the latest, or one flipped back to)
    const QRectF view = d->sheet ? QRectF(this->width() / 2.0 - stageWidth() / 2, pill.top() - sheetInner - 10, stageWidth(), sheetInner)
                                 : QRectF(this->width() / 2.0 - stageWidth() / 2, pill.bottom() + 64, stageWidth(), this->height() - pill.bottom() - 64 - 24);
    d->viewHeight = d->sheet ? this->height() * 0.55 : view.height();
    d->echoFrom = view.top() - cy;
    p.save();
    p.setClipRect(view.adjusted(-24, -60, 24, 0), Qt::IntersectClip);
    // the turn before falls away as the new one comes in
    const double leaving = d->since(d->leftAt);
    if (d->leavingStart >= 0 && leaving >= 0 && leaving < 0.7 && starts.contains(d->leavingStart)) {
        const qsizetype hits = d->hits.size(), solid = d->solid.size();
        p.save();
        p.setOpacity(p.opacity() * (1 - c01(springAt(M3Motion::EffectsFast, leaving))));
        p.translate(0, 22 * springAt(M3Motion::SpatialDefault, leaving));
        paintStage(p, view, starts, 0, int(starts.indexOf(d->leavingStart)), true);
        p.restore();
        d->hits.resize(hits); // it's going: nothing on it answers clicks
        d->solid.resize(solid);
    }
    d->contentHeight = paintStage(p, view, starts, seesK);
    p.restore();
    if (d->items.isEmpty() && d->introToPanelAt < 0 && !d->sheet) {
        text(p, view.adjusted(0, 4, 0, 0), tr("Ask me to change a setting, explain what you copied, or find a file."), flex(17, 450), s.onSurfaceVariant, Qt::AlignHCenter | Qt::AlignTop);
    }
    paintFocus(p);
}

QImage AssistantPanel::imageOf(const QString& source) {
    // a picture by path or URL, loaded once (a null image until it's here); a PDF shows its first page
    if (!d->images.contains(source)) {
        d->images.insert(source, QImage());
        if (source.startsWith(QStringLiteral("http"))) {
            if (!d->network) d->network = new QNetworkAccessManager(this);
            QNetworkRequest request{QUrl(source)};
            // some sites (Wikimedia) refuse requests that don't say what they are
            request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("theDesk/3.0 (desktop assistant)"));
            QNetworkReply* reply = d->network->get(request);
            connect(reply, &QNetworkReply::finished, this, [this, reply, source] {
                QImage image = QImage::fromData(reply->readAll());
                if (image.isNull()) image = QImage(1, 1, QImage::Format_ARGB32); // failed: a gap, not a spinner forever
                d->images.insert(source, image);
                reply->deleteLater();
                animateFor(600);
            });
        } else if (source.endsWith(QStringLiteral(".pdf"), Qt::CaseInsensitive)) {
            auto* render = new QProcess(this);
            connect(render, &QProcess::finished, this, [this, render, source] {
                QImage page;
                page.loadFromData(render->readAllStandardOutput(), "PNG");
                d->images.insert(source, page.isNull() ? QImage(1, 1, QImage::Format_ARGB32) : page);
                render->deleteLater();
                animateFor(600);
            });
            render->start(QStringLiteral("pdftoppm"), {QStringLiteral("-f"), QStringLiteral("1"), QStringLiteral("-l"), QStringLiteral("1"), QStringLiteral("-scale-to"), QStringLiteral("520"), QStringLiteral("-png"), source});
        } else {
            QImage image(source);
            d->images.insert(source, image.isNull() ? QImage(1, 1, QImage::Format_ARGB32) : image.scaled(900, 900, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        }
    }
    return d->images.value(source);
}

double AssistantPanel::glide(const QString& key, double target, M3Motion::Token token) {
    // a layout value on a spring: the first time it's simply there, then it glides to each new place
    auto it = d->glides.find(key);
    if (it == d->glides.end()) it = d->glides.insert(key, {target, 0, target, token, d->now()});
    it->used = d->now();
    it->token = token;
    if (std::abs(it->target - target) > 0.01) {
        it->target = target;
        animateFor(900);
    }
    return it->value;
}

QRectF AssistantPanel::pillRect() const {
    const double w = qMin(PillWidth, this->width() - 80.0);
    if (d->sheet) return QRectF((this->width() - w) / 2, this->height() - PillHeight - 40, w, PillHeight); // circle to search's spot
    return QRectF((this->width() - w) / 2, qMax(72.0, this->height() * 0.14), w, PillHeight);
}

double AssistantPanel::stageWidth() const {
    return qMin(StageWidth, this->width() - 80.0);
}

QList<int> AssistantPanel::turnStarts() const {
    // a turn starts at each question, and at each greeting on opening
    QList<int> starts;
    for (int i = 0; i < d->items.size(); i++)
        if (d->items.at(i).kind == Private::Item::User || d->items.at(i).opens) starts.append(i);
    if (starts.isEmpty() && !d->items.isEmpty()) starts.append(0);
    return starts;
}

void AssistantPanel::markLeaving() {
    // the turn being looked at falls away as the new one comes in
    const QList<int> starts = turnStarts();
    if (starts.isEmpty() || d->listOpen) return;
    d->leavingStart = starts.at(qBound(0, int(starts.size()) - 1 - d->viewBack, int(starts.size()) - 1));
    d->leftAt = d->now();
    d->leavingPos = d->carousel->currentValue().toDouble();
}

void AssistantPanel::showTurn(int back) {
    // a turn picked from the list of earlier ones (0: the latest)
    d->listOpen = false;
    d->viewBack = qMax(0, back);
    d->flipAt = d->now();
    d->carousel->snapTo(0.0);
    d->scrollTarget = d->scroll = 0;
    animateFor(900);
}

double AssistantPanel::paintStage(QPainter& p, const QRectF& view, const QList<int>& starts, double seesK, int only, bool leaving) {
    const M3::Scheme s = M3::currentScheme();
    if (starts.isEmpty()) return 0;
    const int turn = only >= 0 ? only : qBound(0, int(starts.size()) - 1 - d->viewBack, int(starts.size()) - 1);
    const QString key = QString::number(starts.at(turn)) + QLatin1Char('.'); // its layout springs
    if (leaving && d->listOpen) return 0;
    const int from = starts.at(turn), to = turn + 1 < starts.size() ? starts.at(turn + 1) : int(d->items.size());
    const double W = view.width();

    // flipping: the turn slides in from the side it came from
    const double flipK = d->flipAt >= 0 ? springAt(M3Motion::SpatialDefault, d->since(d->flipAt)) : 1;
    p.translate(view.left(), view.top() - d->scroll + 24 * (1 - c01(flipK)));
    p.setOpacity(p.opacity() * c01(d->flipAt >= 0 ? d->since(d->flipAt) / 0.2 : 1));

    auto hit = [&](const QRectF& r, std::function<void(QPointF)> act, bool drags = false) {
        const QRectF mapped = p.transform().mapRect(r);
        if (mapped.intersects(view)) d->hits.append({mapped.intersected(view), act, drags});
    };
    auto hovered = [&](const QRectF& r) {
        return p.transform().mapRect(r).contains(d->mouse);
    };
    auto solid = [&](const QRectF& r) {
        d->solid.append(p.transform().mapRect(r));
    };
    auto button = [&](const QRectF& r, const QString& ic, const QString& label, bool tonal, std::function<void(QPointF)> act) {
        p.setPen(tonal ? Qt::NoPen : QPen(s.outlineVariant, 1));
        QColor bg = tonal ? s.secondaryContainer : QColor(Qt::transparent);
        if (hovered(r)) bg = mix(bg.alpha() ? bg : s.surfaceContainerHigh, tonal ? s.onSecondaryContainer : s.primary, 0.08);
        p.setBrush(bg);
        p.drawRoundedRect(r.adjusted(0.5, 0.5, -0.5, -0.5), r.height() / 2, r.height() / 2);
        p.setPen(Qt::NoPen);
        const QColor ink = tonal ? s.onSecondaryContainer : s.primary;
        if (ic.isEmpty()) {
            text(p, r, label, flex(13, 600), ink, Qt::AlignCenter);
        } else {
            icon(p, QRectF(r.left() + 8, r.top(), 22, r.height()), ic, ink, 17);
            text(p, r.adjusted(34, 0, 0, 0), label, flex(13, 600), ink);
        }
        hit(r, act);
    };

    // The questions asked so far, newest first
    QList<int> asked; // turn numbers
    for (int t = int(starts.size()) - 1; t >= 0; t--)
        if (d->items.at(starts.at(t)).kind == Private::Item::User) asked.append(t);
    if (d->listOpen && !leaving) {
        // a grouped list: the outer corners round, the rows joined by small ones
        double y = 0;
        const double since = d->since(d->listAt);
        for (int row = 0; row < asked.size(); row++) {
            const int t = asked.at(row);
            const int end = t + 1 < starts.size() ? starts.at(t + 1) : int(d->items.size());
            QString answer;
            for (int i = starts.at(t); i < end && answer.isEmpty(); i++)
                if (d->items.at(i).kind == Private::Item::Answer) answer = plainOf(parseAnswer(d->items.at(i).text).tokens, QStringLiteral(" "));
            const double k = springAt(M3Motion::SpatialDefault, since - row * 0.04);
            if (k <= 0) break;
            const QRectF r(W * 0.1, y + (1 - c01(k)) * 20, W * 0.8, 64);
            const bool first = row == 0, last = row == asked.size() - 1, current = t == turn;
            const double top = first ? 20 : 4, bottom = last ? 20 : 4;
            QPainterPath shape; // top and bottom corners rounded apart
            shape.moveTo(r.left() + top, r.top());
            shape.arcTo(QRectF(r.right() - 2 * top, r.top(), 2 * top, 2 * top), 90, -90);
            shape.arcTo(QRectF(r.right() - 2 * bottom, r.bottom() - 2 * bottom, 2 * bottom, 2 * bottom), 0, -90);
            shape.arcTo(QRectF(r.left(), r.bottom() - 2 * bottom, 2 * bottom, 2 * bottom), 270, -90);
            shape.arcTo(QRectF(r.left(), r.top(), 2 * top, 2 * top), 180, -90);
            shape.closeSubpath();
            const QColor bg = current ? s.secondaryContainer : s.surfaceContainerHigh, ink = current ? s.onSecondaryContainer : s.onSurface;
            p.save();
            p.setOpacity(p.opacity() * c01((since - row * 0.04) / 0.2));
            p.setPen(Qt::NoPen);
            p.setBrush(hovered(r) ? mix(bg, ink, 0.08) : bg);
            p.drawPath(shape);
            text(p, QRectF(r.left() + 20, r.top() + 10, r.width() - 40, 24), QFontMetricsF(flex(15, 600)).elidedText(d->items.at(starts.at(t)).text, Qt::ElideRight, r.width() - 40), flex(15, 600), ink);
            text(p, QRectF(r.left() + 20, r.top() + 33, r.width() - 40, 20), QFontMetricsF(flex(13, 450)).elidedText(answer, Qt::ElideRight, r.width() - 40), flex(13, 450), current ? ink : s.onSurfaceVariant);
            p.restore();
            solid(r);
            const int back = int(starts.size()) - 1 - t;
            hit(r, [this, back](QPointF) { showTurn(back); });
            y += 64 + 2;
        }
        return y;
    }

    // What's in the turn
    int user = -1, chips = -1;
    QList<int> answers, cards;
    int pendingAsk = -1;
    for (int i = from; i < to; i++) {
        const Private::Item& item = d->items.at(i);
        if (item.kind == Private::Item::User) user = i;
        else if (item.kind == Private::Item::Answer) answers.append(i);
        else if (item.kind == Private::Item::Chips) chips = i;
        else if (item.kind == Private::Item::Card) {
            if (item.card.kind == AssistantCard::Ask && item.card.value == 0) pendingAsk = i;
            cards.append(i);
        }
    }

    // An answer longer than about three lines: the line says its first
    // sentence and the whole of it goes on a Read card
    const double LinePx = 30;
    QString line;
    QList<qint64> lineBorn;
    QHash<QString, double> lineFrom;
    bool long_ = false;
    double lineGone = 0;
    if (!answers.isEmpty()) {
        const Private::Item& first = d->items.at(answers.first());
        const QString plain = plainOf(parseAnswer(first.text).tokens, QStringLiteral("\n"));
        const QFontMetricsF metrics(flex(LinePx, 480));
        long_ = answers.size() > 1 || metrics.boundingRect(QRectF(0, 0, W, 1e6), Qt::TextWordWrap, plain).height() > metrics.lineSpacing() * 3.2;
        line = first.text;
        if (long_) {
            static const QRegularExpression sentence(QStringLiteral("^[^\\n]{8,}?[.!?](\\s|$)"));
            const QRegularExpressionMatch m = sentence.match(first.text);
            line = m.hasMatch() ? m.captured(0).trimmed() : first.text.section('\n', 0, 0).left(140);
        }
        lineBorn = first.wordBorn;
        lineFrom = first.countFrom;
        // once it's all there the Read card says it all: the line folds away
        // (unless there are other cards: then they lead and the line stays to say what they are)
        bool otherCards = false;
        for (int c : std::as_const(cards)) otherCards = otherCards || c != pendingAsk;
        if (long_ && first.doneAt >= 0 && !otherCards) {
            const qint64 from = qMax(first.doneAt, first.wordBorn.isEmpty() ? first.doneAt : first.wordBorn.last()) + LineGoesAfterMs;
            lineGone = c01(springAt(M3Motion::SpatialDefault, (d->now() - from) / 1000.0));
        }
    }

    double y = 0;
    // The line: the answer; while it works, the question waiting faintly; an ask says what it wants
    if (pendingAsk >= 0) {
        const AssistantCard& ask = d->items.at(pendingAsk).card;
        QString wants = ask.subtitle.isEmpty() ? ask.title : ask.subtitle;
        if (!wants.isEmpty()) wants[0] = wants.at(0).toUpper();
        const QFont f = flex(26, 560);
        const QRectF r = QFontMetricsF(f).boundingRect(QRectF(0, 0, W, 1e6), Qt::AlignHCenter | Qt::TextWordWrap, wants);
        p.save();
        rise(p, QRectF(0, y, W, r.height()), (d->now() - d->items.at(pendingAsk).born) / 1000.0);
        text(p, QRectF(0, y, W, r.height() + 4), wants, f, s.onSurface, Qt::AlignHCenter | Qt::AlignTop | Qt::TextWordWrap);
        p.restore();
        y += r.height() + 24;
    } else if (!line.isEmpty()) {
        if (lineGone < 1) {
            p.save();
            p.setOpacity(p.opacity() * (1 - c01(lineGone * 1.6)));
            const double h = paintLiving(p, line, lineBorn, QPointF(0, y - 12 * lineGone), W, LinePx, hit, hovered, lineFrom, true);
            p.restore();
            solid(QRectF(0, y, W, h)); // clicking the words doesn't close it
            y += (h + 22) * (1 - lineGone);
        }
    } else if (user >= 0 && d->busy && d->viewBack == 0 && !leaving) {
        // the question drops out of the pill to where the answer will land, and waits there
        const double k = springAt(M3Motion::SpatialDefault, (d->now() - d->items.at(user).born) / 1000.0);
        const QFont f = flex(lerp(17, 20, c01(k)), lerp(560, 450, c01(k)));
        const QRectF r = QFontMetricsF(flex(20, 450)).boundingRect(QRectF(0, 0, W * 0.8, 1e6), Qt::AlignHCenter | Qt::TextWordWrap, d->items.at(user).text);
        text(p, QRectF(W * 0.1, y - (d->echoFrom + 12) * (1 - k), W * 0.8, r.height() + 4), d->items.at(user).text, f, mix(s.onSurface, alpha(s.onSurfaceVariant, 0.7), c01(k)), Qt::AlignHCenter | Qt::AlignTop | Qt::TextWordWrap);
        y += r.height() + 24;
    } else if (user >= 0) {
        // an earlier question with no words back (only cards, or stopped)
        text(p, QRectF(0, y, W, 30), d->items.at(user).text, flex(20, 450), alpha(s.onSurfaceVariant, 0.7), Qt::AlignCenter);
        y += 30 + 22;
    }

    // The carousel: an ask first, then the Read card, then the rest
    struct Slot {
            int item = -1; // -1: the Read card
            qint64 born = 0;
            int folder = -1; // a disk card's folders come as tiles of their own
    };
    QList<Slot> tiles;
    if (pendingAsk >= 0) tiles.append({pendingAsk, d->items.at(pendingAsk).born});
    const bool readFirst = std::all_of(cards.begin(), cards.end(), [pendingAsk](int c) { return c == pendingAsk; });
    if (long_ && readFirst) tiles.append({-1, d->items.at(answers.first()).born + 300});
    for (int i : std::as_const(cards)) {
        if (i == pendingAsk) continue;
        const AssistantCard& c = d->items.at(i).card;
        if (c.kind == AssistantCard::Notifications || c.kind == AssistantCard::Files) { // one tile each, nothing around them
            for (int f = 0; f < c.lines.size(); f++) tiles.append({i, d->items.at(i).born + 70 * f, f});
            continue;
        }
        tiles.append({i, d->items.at(i).born});
        if (c.kind == AssistantCard::Storage || c.kind == AssistantCard::Processes) // the whole first, then its parts
            for (int f = 0; f < c.lines.size(); f++) tiles.append({i, d->items.at(i).born + 90 * (f + 1), f});
    }

    if (long_ && !readFirst) tiles.append({-1, d->items.at(answers.first()).born + 300}); // the cards lead, the words follow
    const double readPx = 16;
    auto readHeight = [&](double w) {
        double h = 40;
        for (int a : std::as_const(answers)) {
            const QString plain = plainOf(parseAnswer(d->items.at(a).text).tokens, QStringLiteral("\n"));
            const QFontMetricsF metrics(flex(readPx, 480));
            h += metrics.boundingRect(QRectF(0, 0, w - 48, 1e6), Qt::TextWordWrap, plain).height() / metrics.lineSpacing() * metrics.height() * 1.3 * 1.08 + 12;
        }
        return h;
    };
    auto heightOf = [&](const Slot& slot, double w) -> double {
        if (slot.item < 0) return readHeight(w);
        const Private::Item& item = d->items.at(slot.item);
        const AssistantCard& card = item.card;
        switch (card.kind) {
            case AssistantCard::Ask: {
                const double full = 64 + qMin<qsizetype>(card.lines.size(), 8) * 21 + 12 + 40 + 16;
                const double settle = item.changedAt >= 0 ? c01(springAt(M3Motion::SpatialDefault, d->since(item.changedAt))) : 0;
                return lerp(full, 64, settle);
            }
            case AssistantCard::Level: return 140;
            case AssistantCard::Storage: return 200;
            case AssistantCard::Battery: return 170;
            case AssistantCard::Processes: return 190;
            case AssistantCard::Media: return 160;
            case AssistantCard::Timer: return 170;
            case AssistantCard::Weather: return 230;
            case AssistantCard::Notifications: return 160;
            case AssistantCard::Files: return 210;
            case AssistantCard::Switch: return 96;
            case AssistantCard::Code: return 48 + card.lines.size() * 23 + 12;
            case AssistantCard::Image: {
                const QImage image = d->images.value(card.path);
                if (image.isNull() || image.width() <= 1) return 200;
                return qMin(320.0, image.height() * qMin(1.0, w / image.width()));
            }
            default: return 128;
        }
    };

    auto paintCard = [&](int index, const QRectF& r) {
        Private::Item& item = d->items[index];
        const AssistantCard& card = item.card;
        if (card.kind == AssistantCard::Ask) {
            // what it wants to do, Allow / Don't; once answered it shrinks to one line
            const int shownLines = int(qMin<qsizetype>(card.lines.size(), 8));
            const double settle = item.changedAt >= 0 ? c01(springAt(M3Motion::SpatialDefault, d->since(item.changedAt))) : 0;
            p.setPen(Qt::NoPen);
            p.setBrush(card.value > 0 ? s.secondaryContainer : s.surfaceContainerHigh);
            p.drawRect(r);
            const QPointF disc(r.left() + 32, r.top() + 32);
            const QString ic = card.value > 0 ? QStringLiteral("check") : card.value < 0 ? QStringLiteral("block") : QStringLiteral("shield_question");
            p.setBrush(card.value > 0 ? s.primary : s.tertiaryContainer);
            p.drawEllipse(disc, 16, 16);
            icon(p, QRectF(disc.x() - 16, disc.y() - 16, 32, 32), ic, card.value > 0 ? s.onPrimary : s.onTertiaryContainer, 18, true);
            const QString title = card.value > 0 ? tr("Allowed · %1").arg(card.title.chopped(1)) : card.value < 0 ? tr("Not allowed · %1").arg(card.title.chopped(1)) : card.title;
            text(p, QRectF(r.left() + 58, r.top() + 14, r.width() - 80, 36), title, flex(15, 600), card.value > 0 ? s.onSecondaryContainer : s.onSurface);
            if (settle >= 1) return;
            p.save();
            p.setOpacity(p.opacity() * (1 - settle));
            if (!card.subtitle.isEmpty()) text(p, QRectF(r.left() + 58, r.top() + 40, r.width() - 80, 20), card.subtitle, flex(12, 450), s.onSurfaceVariant);
            const QRectF code(r.left() + 14, r.top() + 64, r.width() - 28, shownLines * 21 + 8);
            p.setBrush(s.surfaceContainerLowest);
            p.drawRoundedRect(code, 14, 14);
            for (int i = 0; i < shownLines; i++) {
                const QString& l = card.lines.at(i);
                const QChar mark = l.isEmpty() ? ' ' : l.at(0);
                const bool diff = mark == '+' || mark == '-';
                text(p, QRectF(code.left() + 12, code.top() + 4 + i * 21, code.width() - 24, 21), l, mono(13, 450), diff ? (mark == '+' ? s.primary : s.error) : s.onSurface);
            }
            if (card.value == 0) {
                const QRectF allow(r.right() - 116, r.bottom() - 54, 100, 40), deny(r.right() - 228, r.bottom() - 54, 100, 40);
                p.setPen(QPen(s.outlineVariant, 1));
                p.setBrush(hovered(deny) ? mix(s.surfaceContainerHigh, s.primary, 0.08) : QColor(Qt::transparent));
                p.drawRoundedRect(deny.adjusted(0.5, 0.5, -0.5, -0.5), 20, 20);
                text(p, deny, tr("Don't"), flex(14, 600), s.primary, Qt::AlignCenter);
                p.setPen(Qt::NoPen);
                p.setBrush(hovered(allow) ? mix(s.primary, s.onPrimary, 0.08) : s.primary);
                p.drawRoundedRect(allow, 20, 20);
                text(p, allow, tr("Allow"), flex(14, 600), s.onPrimary, Qt::AlignCenter);
                auto decide = [this, index](bool yes) {
                    Private::Item& it = d->items[index];
                    if (it.card.value != 0) return;
                    it.card.value = yes ? 1 : -1;
                    it.changedAt = d->now();
                    d->brain->answer(it.card.id, yes);
                    animateFor(800);
                };
                hit(allow, [decide](QPointF) { decide(true); });
                hit(deny, [decide](QPointF) { decide(false); });
            }
            p.restore();
        } else if (card.kind == AssistantCard::Level || card.kind == AssistantCard::Switch) {
            const double flash = item.changedAt >= 0 ? qMax(0.0, 1 - d->since(item.changedAt) / 0.9) : 0;
            p.setPen(Qt::NoPen);
            p.setBrush(mix(s.surfaceContainerHigh, s.secondaryContainer, flash));
            p.drawRect(r);
            const bool level = card.kind == AssistantCard::Level;
            const QPointF disc(r.left() + 40, r.top() + 40);
            p.setBrush(level ? s.secondaryContainer : s.tertiaryContainer);
            p.drawEllipse(disc, 20, 20);
            icon(p, QRectF(disc.x() - 20, disc.y() - 20, 40, 40), card.icon.isEmpty() ? (level ? QStringLiteral("tune") : QStringLiteral("toggle_on")) : card.icon, level ? s.onSecondaryContainer : s.onTertiaryContainer, 22, true);
            if (level) {
                text(p, QRectF(r.left() + 72, r.top() + 20, r.width() - 180, 40), card.title, flex(16, 600), s.onSurface);
                const double v = c01(card.value);
                text(p, QRectF(r.right() - 140, r.top() + 14, 120, 52), QStringLiteral("%1%").arg(qRound(v * 100)), flex(30, 700, 110), s.onSurface, Qt::AlignRight | Qt::AlignVCenter);
                // the M3E slider: a thin handle with gaps, a stop dot at the end
                const QRectF track(r.left() + 24, r.bottom() - 60, r.width() - 48, 40);
                const double hx = track.left() + track.width() * v, ty = track.center().y();
                p.setBrush(s.primary);
                p.drawRoundedRect(QRectF(track.left(), ty - 8, qMax(0.0, hx - 6 - track.left()), 16), 8, 8);
                p.setBrush(s.secondaryContainer);
                p.drawRoundedRect(QRectF(hx + 6, ty - 8, qMax(0.0, track.right() - hx - 6), 16), 8, 8);
                p.setBrush(s.onSecondaryContainer);
                p.drawEllipse(QPointF(track.right() - 8, ty), 2, 2);
                p.setBrush(s.primary);
                p.drawRoundedRect(QRectF(hx - 2, ty - 20, 4, 40), 2, 2);
                // dragging works in window coordinates, so map the track there
                const QRectF mapped = p.transform().mapRect(track);
                if (mapped.intersects(view)) {
                    d->hits.append({mapped.adjusted(0, -4, 0, 4), [this, index, mapped](QPointF at) {
                        Private::Item& it = d->items[index];
                        it.card.value = c01((at.x() - mapped.left()) / mapped.width());
                        d->brain->setCardValue(it.card.id, it.card.value);
                        animateFor(200);
                    }, true, [this, index](int step) {
                        Private::Item& it = d->items[index];
                        it.card.value = c01(std::round(it.card.value * 20 + step) / 20);
                        d->brain->setCardValue(it.card.id, it.card.value);
                        animateFor(200);
                    }});
                }
            } else {
                text(p, QRectF(r.left() + 72, r.top() + 22, r.width() - 160, 24), card.title, flex(16, 600), s.onSurface);
                // the time rolls over to the new one
                const QRectF sub(r.left() + 72, r.top() + 46, r.width() - 160, 20);
                const double roll = item.changedAt >= 0 ? springAt(M3Motion::SpatialDefault, d->since(item.changedAt)) : 1;
                p.save();
                p.setClipRect(sub, Qt::IntersectClip);
                if (roll < 1) text(p, sub.translated(0, -20 * roll), item.was, flex(13, 450), s.onSurfaceVariant);
                text(p, sub.translated(0, 20 * (1 - roll)), card.subtitle, flex(13, 450), s.onSurfaceVariant);
                p.restore();
                const QRectF track(r.right() - 76, r.top() + 24, 52, 32);
                const double on = c01(card.value);
                p.setPen(on < 0.5 ? QPen(s.outline, 2) : Qt::NoPen);
                p.setBrush(mix(s.surfaceContainerHighest, s.primary, on));
                p.drawRoundedRect(track.adjusted(1, 1, -1, -1), 15, 15);
                p.setPen(Qt::NoPen);
                const double knob = lerp(16, 24, on);
                const QPointF kc(lerp(track.left() + 16, track.left() + 36, on), track.center().y());
                p.setBrush(mix(s.outline, s.onPrimary, on));
                p.drawEllipse(kc, knob / 2, knob / 2);
                if (on > 0.5) icon(p, QRectF(kc.x() - 10, kc.y() - 10, 20, 20), QStringLiteral("check"), s.onPrimaryContainer, 16);
                hit(track.adjusted(-8, -8, 8, 8), [this, index](QPointF) {
                    Private::Item& it = d->items[index];
                    it.card.value = it.card.value > 0.5 ? 0 : 1;
                    d->brain->setCardValue(it.card.id, it.card.value);
                    animateFor(300);
                });
            }
        } else if (card.kind == AssistantCard::Storage) {
            // the disk: what's free, big, and a bar of what's used
            p.setPen(Qt::NoPen);
            p.setBrush(s.surfaceContainerHigh);
            p.drawRect(r);
            const QPointF disc(r.left() + 40, r.top() + 40);
            p.setBrush(s.primaryContainer);
            p.drawEllipse(disc, 20, 20);
            icon(p, QRectF(disc.x() - 20, disc.y() - 20, 40, 40), QStringLiteral("hard_drive"), s.onPrimaryContainer, 22, true);
            text(p, QRectF(r.left() + 72, r.top() + 20, r.width() - 92, 40), card.title, flex(16, 600), s.onSurface);
            const QString free = card.subtitle.section(' ', 0, 1), rest = card.subtitle.section(' ', 2);
            const double age = (d->now() - item.born) / 1000.0;
            const QFont big = flex(34, 700, 110);
            text(p, QRectF(r.left() + 24, r.top() + 74, r.width() - 48, 44), free, big, s.onSurface);
            text(p, QRectF(r.left() + 24 + textWidth(free, big) + 10, r.top() + 74, r.width() - 48, 48), rest, flex(15, 500), s.onSurfaceVariant);
            // the bar fills on a spring: used in primary, the gap, what's free, a stop dot
            const QRectF bar(r.left() + 24, r.bottom() - 48, r.width() - 48, 16);
            const double used = c01(card.value) * c01(springAt(M3Motion::SpatialSlow, age - 0.2));
            const double edge = bar.left() + bar.width() * used;
            p.setBrush(c01(card.value) > 0.9 ? s.error : s.primary);
            p.drawRoundedRect(QRectF(bar.left(), bar.top(), qMax(bar.height(), edge - bar.left() - 3), bar.height()), 8, 8);
            p.setBrush(s.secondaryContainer);
            if (bar.right() - edge > 12) p.drawRoundedRect(QRectF(edge + 3, bar.top(), bar.right() - edge - 3, bar.height()), 8, 8);
            p.setBrush(s.onSecondaryContainer);
            p.drawEllipse(QPointF(bar.right() - 8, bar.center().y()), 2, 2);
            text(p, QRectF(bar.left(), bar.top() - 24, bar.width(), 20), tr("%1% used").arg(qRound(c01(card.value) * 100)), flex(13, 500), s.onSurfaceVariant, Qt::AlignRight | Qt::AlignVCenter);
        } else if (card.kind == AssistantCard::Processes) {
            // the whole computer, live: CPU and memory as bars that glide
            p.setPen(Qt::NoPen);
            p.setBrush(s.surfaceContainerHigh);
            p.drawRect(r);
            const double cpu = cpuOfSystem();
            const auto [used, total] = memoryOfSystem();
            const double memory = total > 0 ? double(used) / total : 0;
            icon(p, QRectF(r.left() + 22, r.top() + 20, 28, 28), QStringLiteral("memory"), s.primary, 24, true);
            text(p, QRectF(r.left() + 58, r.top() + 20, r.width() - 80, 28), card.title, flex(16, 600), s.onSurface);
            auto meter = [&](double y, const QString& label, const QString& value, double fraction, const QString& glideKey) {
                const double shown = c01(glide(key + glideKey, c01(fraction), M3Motion::SpatialSlow));
                text(p, QRectF(r.left() + 22, y, 120, 24), label, flex(14, 500), s.onSurfaceVariant);
                text(p, QRectF(r.left() + 22, y, r.width() - 44, 24), value, flex(18, 700, 110), s.onSurface, Qt::AlignRight | Qt::AlignVCenter);
                const QRectF bar(r.left() + 22, y + 30, r.width() - 44, 12);
                const double edge = bar.left() + bar.width() * shown;
                p.setBrush(shown > 0.85 ? s.error : s.primary);
                p.drawRoundedRect(QRectF(bar.left(), bar.top(), qMax(bar.height(), edge - bar.left() - 3), bar.height()), 6, 6);
                p.setBrush(s.secondaryContainer);
                if (bar.right() - edge > 10) p.drawRoundedRect(QRectF(edge + 3, bar.top(), bar.right() - edge - 3, bar.height()), 6, 6);
            };
            meter(r.top() + 62, tr("CPU"), cpu < 0 ? QStringLiteral("…") : QStringLiteral("%1%").arg(qRound(cpu)), qMax(0.0, cpu) / 100, QStringLiteral("cpu"));
            const QLocale locale;
            meter(r.top() + 118, tr("Memory"), QStringLiteral("%1 / %2").arg(locale.formattedDataSize(used, 1, QLocale::DataSizeTraditionalFormat), locale.formattedDataSize(total, 0, QLocale::DataSizeTraditionalFormat)), memory, QStringLiteral("memory"));
            if (!d->secondTick) {
                d->secondTick = true;
                QTimer::singleShot(1000, this, [this] {
                    d->secondTick = false;
                    redraw();
                });
            }
        } else if (card.kind == AssistantCard::Battery) {
            // the charge as a ring (wavy while it charges), what it's doing, the time left
            p.setPen(Qt::NoPen);
            p.setBrush(s.surfaceContainerHigh);
            p.drawRect(r);
            const double age = (d->now() - item.born) / 1000.0;
            const bool charging = card.icon == QStringLiteral("charging");
            const double level = c01(card.value), shown = level * c01(springAt(M3Motion::SpatialSlow, age - 0.15));
            const QPointF centre(r.left() + 24 + 62, r.center().y());
            const double radius = 56;
            p.setBrush(Qt::NoBrush);
            p.setPen(QPen(s.secondaryContainer, 12, Qt::SolidLine, Qt::RoundCap));
            p.drawEllipse(centre, radius, radius);
            const QColor active = level < 0.2 && !charging ? s.error : s.primary;
            QPainterPath arc;
            const double t = d->now() / 1000.0;
            for (int i = 0; i <= 240; i++) {
                const double a = -M_PI / 2 + 2 * M_PI * shown * i / 240.0;
                const double rr = radius + (charging ? 2.5 * std::sin(a * 10 - t * 5) : 0);
                const QPointF pt = centre + QPointF(std::cos(a), std::sin(a)) * rr;
                if (i == 0) arc.moveTo(pt);
                else arc.lineTo(pt);
            }
            if (charging) d->animateUntil = qMax(d->animateUntil, d->now() + 40); // the wave keeps moving while it shows
            p.setPen(QPen(active, 12, Qt::SolidLine, Qt::RoundCap));
            if (shown > 0.005) p.drawPath(arc);
            p.setPen(Qt::NoPen);
            text(p, QRectF(centre.x() - radius, centre.y() - 24, radius * 2, 48), QStringLiteral("%1%").arg(qRound(shown * 100)), flex(30, 700, 110), s.onSurface, Qt::AlignCenter);
            const double tx = centre.x() + radius + 28;
            if (charging) icon(p, QRectF(tx, r.center().y() - 46, 28, 28), QStringLiteral("bolt"), s.primary, 24, true);
            text(p, QRectF(tx + (charging ? 30 : 0), r.center().y() - 46, r.right() - tx - 20, 28), card.title, flex(20, 650), s.onSurface);
            text(p, QRectF(tx, r.center().y() - 12, r.right() - tx - 20, 24), card.subtitle, flex(15, 450), s.onSurfaceVariant);
        } else if (card.kind == AssistantCard::Media) {
            // the player, read live: its art, what's on, and the controls right here
            p.setPen(Qt::NoPen);
            p.setBrush(s.surfaceContainerHigh);
            p.drawRect(r);
            MprisPlayerPtr player;
            for (const MprisPlayerPtr& candidate : MprisEngine::players())
                if (candidate->identity() == card.path) player = candidate;
            const MetadataMap meta = player ? player->metadata() : MetadataMap();
            const QString title = player ? meta.value(QStringLiteral("xesam:title")).toString() : card.title;
            const QString artist = player ? meta.value(QStringLiteral("xesam:artist")).toStringList().join(QStringLiteral(", ")) : card.subtitle;
            const bool playing = player && player->playbackStatus() == MprisPlayerInterface::Playing;
            QString art = player ? meta.value(QStringLiteral("mpris:artUrl")).toString() : QString();
            if (art.startsWith(QStringLiteral("file://"))) art = QUrl(art).toLocalFile();
            const QRectF cover(r.left() + 16, r.top() + 16, r.height() - 32, r.height() - 32);
            QPainterPath coverShape;
            coverShape.addRoundedRect(cover, 20, 20);
            const QImage image = art.isEmpty() ? QImage() : imageOf(art);
            p.save();
            p.setClipPath(coverShape, Qt::IntersectClip);
            if (!image.isNull() && image.width() > 1) {
                const QSizeF fit = QSizeF(image.size()).scaled(cover.size(), Qt::KeepAspectRatioByExpanding);
                p.setRenderHint(QPainter::SmoothPixmapTransform);
                p.drawImage(QRectF(cover.center().x() - fit.width() / 2, cover.center().y() - fit.height() / 2, fit.width(), fit.height()), image);
            } else {
                p.fillRect(cover, s.tertiaryContainer);
                icon(p, cover, QStringLiteral("music_note"), s.onTertiaryContainer, 40, true);
            }
            p.restore();
            const double tx = cover.right() + 20, tw = r.right() - tx - 20;
            text(p, QRectF(tx, r.top() + 22, tw, 28), QFontMetricsF(flex(18, 650)).elidedText(title.isEmpty() ? tr("Nothing playing") : title, Qt::ElideRight, tw), flex(18, 650), s.onSurface);
            text(p, QRectF(tx, r.top() + 50, tw, 22), QFontMetricsF(flex(14, 450)).elidedText(player ? artist : tr("The player closed"), Qt::ElideRight, tw), flex(14, 450), s.onSurfaceVariant);
            if (player) {
                // previous, play/pause (a circle that squares off while it plays, on a spring), next
                const double cy = r.bottom() - 44;
                const QRectF previous(tx, cy - 20, 40, 40), toggle(tx + 52, cy - 28, 56, 56), next(tx + 120, cy - 20, 40, 40);
                const double corner = glide(key + card.path + QStringLiteral(".corner"), playing ? 16 : 28, M3Motion::SpatialFast);
                for (const QRectF& b : {previous, next}) {
                    p.setBrush(hovered(b) ? mix(s.secondaryContainer, s.onSecondaryContainer, 0.08) : s.secondaryContainer);
                    p.drawEllipse(b);
                }
                icon(p, previous, QStringLiteral("skip_previous"), s.onSecondaryContainer, 22, true);
                icon(p, next, QStringLiteral("skip_next"), s.onSecondaryContainer, 22, true);
                p.setBrush(hovered(toggle) ? mix(s.primary, s.onPrimary, 0.08) : s.primary);
                p.drawRoundedRect(toggle, corner, corner);
                icon(p, toggle, playing ? QStringLiteral("pause") : QStringLiteral("play_arrow"), s.onPrimary, 28, true);
                const QPointer<MprisPlayerInterface> target = player.data();
                auto press = [this, target](void (MprisPlayerInterface::*what)()) {
                    return [this, target, what](QPointF) {
                        if (target) (target.data()->*what)();
                        animateFor(1500);
                    };
                };
                hit(previous, press(&MprisPlayerInterface::previous));
                hit(toggle, press(&MprisPlayerInterface::playPause));
                hit(next, press(&MprisPlayerInterface::next));
            }
        } else if (card.kind == AssistantCard::Timer) {
            // counts down, once a second; Cancel stops the reminder
            p.setPen(Qt::NoPen);
            p.setBrush(s.surfaceContainerHigh);
            p.drawRect(r);
            const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
            const bool cancelled = card.value < 0;
            const double due = card.value, set = card.lines.value(0).toDouble();
            const qint64 left = qMax<qint64>(0, qint64(due) - nowMs);
            const qint64 secs = (left + 999) / 1000;
            const QString count = cancelled ? tr("Cancelled") : left == 0 ? tr("Now") : secs >= 3600 ? QStringLiteral("%1:%2:%3").arg(secs / 3600).arg(secs / 60 % 60, 2, 10, QChar('0')).arg(secs % 60, 2, 10, QChar('0')) : QStringLiteral("%1:%2").arg(secs / 60).arg(secs % 60, 2, 10, QChar('0'));
            QFont big = flex(44, 700, 110);
            big.setFeature(QFont::Tag("tnum"), 1);
            icon(p, QRectF(r.left() + 22, r.top() + 22, 28, 28), QStringLiteral("alarm"), s.primary, 24, true);
            text(p, QRectF(r.left() + 58, r.top() + 22, r.width() - 80, 28), QFontMetricsF(flex(16, 600)).elidedText(card.title, Qt::ElideRight, r.width() - 80), flex(16, 600), s.onSurface);
            text(p, QRectF(r.left() + 22, r.top() + 58, r.width() - 44, 56), count, big, cancelled ? s.onSurfaceVariant : s.onSurface);
            text(p, QRectF(r.left() + 22, r.top() + 112, r.width() - 160, 20), card.subtitle, flex(13, 450), s.onSurfaceVariant);
            // what's left as a wavy line that drains; still waves once a second
            const QRectF bar(r.left() + 22, r.bottom() - 30, r.width() - 44, 12);
            const double fraction = cancelled ? 0 : due > set ? c01(left / (due - set)) : 0;
            const double end = bar.left() + bar.width() * fraction;
            QPainterPath wave;
            for (double x = bar.left(); x <= end; x += 2) {
                const QPointF pt(x, bar.center().y() + 3 * std::sin((x - bar.left()) / 40.0 * 2 * M_PI - secs));
                if (x == bar.left()) wave.moveTo(pt);
                else wave.lineTo(pt);
            }
            p.setBrush(Qt::NoBrush);
            p.setPen(QPen(s.primary, 4, Qt::SolidLine, Qt::RoundCap));
            if (fraction > 0.005) p.drawPath(wave);
            p.setPen(QPen(s.secondaryContainer, 4, Qt::SolidLine, Qt::RoundCap));
            if (end + 8 < bar.right()) p.drawLine(QPointF(qMax(bar.left(), end + 8), bar.center().y()), QPointF(bar.right(), bar.center().y()));
            p.setPen(Qt::NoPen);
            if (!cancelled && left > 0) {
                button(QRectF(r.right() - 120, r.top() + 104, 100, 36), {}, tr("Cancel"), false, [this, index](QPointF) {
                    Private::Item& it = d->items[index];
                    d->brain->setCardValue(it.card.id, 0);
                    it.card.value = -1;
                    it.changedAt = d->now();
                    animateFor(400);
                });
                // the next second
                if (!d->secondTick) {
                    d->secondTick = true;
                    QTimer::singleShot(int(1000 - nowMs % 1000) + 5, this, [this] {
                        d->secondTick = false;
                        redraw();
                    });
                }
            }
        } else if (card.kind == AssistantCard::Weather) {
            // the sky as a big shape, the temperature, the next hours
            p.setPen(Qt::NoPen);
            p.setBrush(s.surfaceContainerHigh);
            p.drawRect(r);
            const double age = (d->now() - item.born) / 1000.0;
            const double in = springAt(M3Motion::SpatialDefault, age - 0.1);
            const QRectF sky(r.right() - 150, r.top() + 18, 120, 120);
            p.save();
            p.translate(sky.center());
            p.rotate(-40 * (1 - c01(in)));
            p.scale(lerp(0.6, 1, in), lerp(0.6, 1, in));
            paintSky(p, card.icon, 120, s);
            p.restore();
            QFont big = flex(56, 700, 110);
            text(p, QRectF(r.left() + 24, r.top() + 18, r.width() - 200, 70), QStringLiteral("%1°").arg(qRound(card.value)), big, s.onSurface);
            text(p, QRectF(r.left() + 26, r.top() + 88, r.width() - 200, 24), card.subtitle, flex(17, 600), s.onSurface);
            text(p, QRectF(r.left() + 26, r.top() + 112, r.width() - 200, 20), card.title, flex(13, 450), s.onSurfaceVariant);
            // the next hours
            const double cw = (r.width() - 40) / qMax(1, int(card.lines.size()));
            for (int i = 0; i < card.lines.size(); i++) {
                const QStringList hour = card.lines.at(i).split('\t');
                const QRectF col(r.left() + 20 + i * cw, r.bottom() - 82, cw, 72);
                text(p, QRectF(col.left(), col.top(), col.width(), 18), hour.value(0), flex(12, 500), s.onSurfaceVariant, Qt::AlignCenter);
                p.save();
                p.translate(col.center().x(), col.top() + 34);
                paintSky(p, hour.value(2), 22, s);
                p.restore();
                text(p, QRectF(col.left(), col.top() + 50, col.width(), 20), hour.value(1) + QStringLiteral("°"), flex(14, 600), s.onSurface, Qt::AlignCenter);
            }
        } else if (card.kind == AssistantCard::Image) {
            // A picture: a file, or fetched from the web
            const QString source = card.path;
            const QImage image = imageOf(source);
            p.setPen(Qt::NoPen);
            p.setBrush(s.surfaceContainerHigh);
            p.drawRect(r);
            if (!image.isNull() && image.width() > 1) {
                // cover the tile, cropped from the middle
                const QSizeF fit = QSizeF(image.size()).scaled(r.size(), Qt::KeepAspectRatioByExpanding);
                p.setRenderHint(QPainter::SmoothPixmapTransform);
                p.drawImage(QRectF(r.center().x() - fit.width() / 2, r.center().y() - fit.height() / 2, fit.width(), fit.height()), image);
            } else {
                icon(p, r, QStringLiteral("image"), s.onSurfaceVariant, 32);
            }
            if (!card.title.isEmpty()) {
                const QRectF label(r.left(), r.bottom() - 36, r.width(), 36);
                p.setBrush(alpha(s.scrim, 0.45));
                p.drawRect(label);
                text(p, label.adjusted(16, 0, -16, 0), QFontMetricsF(flex(13, 500)).elidedText(card.title, Qt::ElideRight, r.width() - 32), flex(13, 500), Qt::white);
            }
            hit(r, [source](QPointF) {
                QDesktopServices::openUrl(source.startsWith(QStringLiteral("http")) ? QUrl(source) : QUrl::fromLocalFile(source));
            });
        } else if (card.kind == AssistantCard::Code) {
            p.setPen(Qt::NoPen);
            p.setBrush(s.surfaceContainerLowest);
            p.drawRect(r);
            const bool isFile = card.path.startsWith('/') || card.path.startsWith('~');
            text(p, QRectF(r.left() + 20, r.top() + 8, 300, 32), isFile ? QFileInfo(card.path).fileName() : (card.path.isEmpty() ? tr("code") : card.path), mono(13, 600), s.onSurfaceVariant);
            const QString path = card.path;
            const QStringList lines = card.lines;
            button(QRectF(r.right() - (isFile ? 252 : 108), r.top() + 8, 96, 32), QStringLiteral("content_copy"), tr("Copy"), false, [lines](QPointF) {
                // the code as it is now: without the removed lines or the markers
                QStringList now;
                for (const QString& l : lines) {
                    if (!l.startsWith('-')) now.append(l.mid(1));
                }
                QGuiApplication::clipboard()->setText(now.join('\n'));
            });
            if (isFile) {
                button(QRectF(r.right() - 148, r.top() + 8, 136, 32), QStringLiteral("open_in_new"), tr("Open in Zed"), true, [path](QPointF) {
                    QProcess::startDetached(QStringLiteral("zeditor"), {QString(path).replace(QRegularExpression(QStringLiteral("^~")), QDir::homePath())});
                });
            }
            for (int i = 0; i < card.lines.size(); i++) {
                const QString& l = card.lines.at(i);
                const QRectF lr(r.left() + 10, r.top() + 48 + i * 23, r.width() - 20, 23);
                const QChar k = l.isEmpty() ? ' ' : l.at(0);
                if (k == '+' || k == '-') {
                    p.setPen(Qt::NoPen);
                    p.setBrush(alpha(k == '+' ? s.primaryContainer : s.errorContainer, 0.3));
                    p.drawRoundedRect(lr, 6, 6);
                }
                text(p, QRectF(lr.left() + 10, lr.top(), 20, lr.height()), k == ' ' ? QString() : QString(k), mono(14, 600), k == '+' ? s.primary : s.error);
                text(p, QRectF(lr.left() + 30, lr.top(), lr.width() - 40, lr.height()), l.mid(1), mono(14, 450), s.onSurface);
            }
        } else {
            // a file: what it is, Open / Show in folder
            p.setPen(Qt::NoPen);
            p.setBrush(s.surfaceContainerHigh);
            p.drawRect(r);
            const QString suffix = QFileInfo(card.path).suffix().toLower();
            const QString ic = suffix == "css" ? QStringLiteral("css") : suffix == "html" ? QStringLiteral("html") : suffix == "js" ? QStringLiteral("javascript") : QStringLiteral("description");
            const QPointF disc(r.left() + 40, r.top() + 40);
            p.setBrush(s.tertiaryContainer);
            p.drawEllipse(disc, 20, 20);
            icon(p, QRectF(disc.x() - 20, disc.y() - 20, 40, 40), ic, s.onTertiaryContainer, 22);
            text(p, QRectF(r.left() + 72, r.top() + 18, r.width() - 92, 22), QFontMetricsF(flex(15, 600)).elidedText(card.title, Qt::ElideMiddle, r.width() - 92), flex(15, 600), s.onSurface);
            text(p, QRectF(r.left() + 72, r.top() + 40, r.width() - 92, 20), QFontMetricsF(flex(12, 450)).elidedText(card.subtitle, Qt::ElideMiddle, r.width() - 92), flex(12, 450), s.onSurfaceVariant);
            const QString path = QString(card.path).replace(QRegularExpression(QStringLiteral("^~")), QDir::homePath());
            button(QRectF(r.left() + 20, r.bottom() - 52, 80, 36), {}, tr("Open"), true, [path](QPointF) {
                QDesktopServices::openUrl(QUrl::fromLocalFile(path));
            });
            button(QRectF(r.left() + 108, r.bottom() - 52, 130, 36), {}, tr("Show in folder"), false, [path](QPointF) {
                QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).absolutePath()));
            });
        }
    };

    // A tile too narrow for the card: its icon, and its name if there's room
    auto paintCompact = [&](const Slot& slot, const QRectF& r, int place) {
        const bool tonal = place % 2 == 0;
        const QColor bg = tonal ? s.secondaryContainer : s.tertiaryContainer, ink = tonal ? s.onSecondaryContainer : s.onTertiaryContainer;
        p.setPen(Qt::NoPen);
        p.setBrush(hovered(r) ? mix(bg, ink, 0.08) : bg);
        p.drawRect(r);
        QString ic = QStringLiteral("notes"), title = tr("Read");
        if (slot.item >= 0) {
            const AssistantCard& card = d->items.at(slot.item).card;
            title = card.title;
            ic = card.kind == AssistantCard::Ask ? QStringLiteral("shield_question") : card.kind == AssistantCard::Code ? QStringLiteral("code") : card.kind == AssistantCard::Image ? QStringLiteral("image") : card.kind == AssistantCard::Storage ? QStringLiteral("hard_drive") : card.kind == AssistantCard::Processes ? QStringLiteral("memory") : card.kind == AssistantCard::Battery ? QStringLiteral("battery_full") : card.kind == AssistantCard::Media ? QStringLiteral("music_note") : card.kind == AssistantCard::Timer ? QStringLiteral("alarm") : card.kind == AssistantCard::Weather ? QStringLiteral("partly_cloudy_day") : card.kind == AssistantCard::Level ? (card.icon.isEmpty() ? QStringLiteral("tune") : card.icon) : card.kind == AssistantCard::Switch ? (card.icon.isEmpty() ? QStringLiteral("toggle_on") : card.icon) : QStringLiteral("description");
        }
        if (r.width() < 110) {
            icon(p, QRectF(r.center().x() - 16, r.center().y() - 16, 32, 32), ic, ink, 26);
            return;
        }
        icon(p, QRectF(r.left() + 20, r.top() + 20, 32, 32), ic, ink, 28);
        text(p, QRectF(r.left() + 20, r.bottom() - 44, r.width() - 40, 28), QFontMetricsF(flex(15, 600)).elidedText(title, Qt::ElideRight, r.width() - 40), flex(15, 600), ink);
    };

    // Multi-browse: the focused item large, the next one medium, one small at the edge
    if (!tiles.isEmpty()) {
        const int n = int(tiles.size());
        const double pos = qBound(0.0, leaving ? d->leavingPos : d->carousel->currentValue().toDouble(), double(n - 1));
        const double gap = 8, small = 56;
        // the small one at the edge arrives wide and shrinks into place
        const double smallIn = lerp(150, small, c01(springAt(M3Motion::SpatialSlow, (d->now() - tiles.first().born) / 1000.0 - 0.35)));
        const double large = n == 1 ? W : n == 2 ? W * 0.64 : W * 0.56;
        const double medium = n == 2 ? W - large - gap : W - large - small - 2 * gap;
        auto widthAt = [&](double off) {
            auto at = [&](int k) {
                return k <= -2 ? 0.0 : k == -1 ? small : k == 0 ? large : k == 1 ? medium : k == 2 ? smallIn : 0.0;
            };
            const int lo = int(std::floor(off));
            return lerp(at(lo), at(lo + 1), off - lo);
        };
        const int lo = int(std::floor(pos)), hi = qMin(n - 1, lo + 1);
        const double height = glide(key + QStringLiteral("carouselHeight"), qMax(n == 1 ? 0.0 : 120.0, lerp(heightOf(tiles.at(lo), large), heightOf(tiles.at(hi), large), pos - lo)));
        y = glide(key + QStringLiteral("carouselTop"), y);
        double total = -gap;
        for (int i = 0; i < n; i++) {
            const double w = widthAt(i - pos);
            if (w > 0.5) total += w + gap;
        }
        double x = qMax(0.0, (W - total) / 2);
        for (int i = 0; i < n; i++) {
            const double w = widthAt(i - pos);
            if (w <= 0.5) continue;
            const Slot& slot = tiles.at(i);
            const double age = (d->now() - slot.born) / 1000.0 - i * 0.09;
            if (age < 0) {
                x += w + gap;
                continue;
            }
            const double in = springAt(M3Motion::SpatialDefault, age);
            const QRectF r(x + (1 - c01(in)) * (420 - i * 50), y, w, height);
            p.save();
            p.setOpacity(p.opacity() * c01(age / 0.2));
            QPainterPath tile;
            tile.addRoundedRect(r, 28, 28);
            p.setClipPath(tile, Qt::IntersectClip);
            const bool picture = slot.item >= 0 && d->items.at(slot.item).card.kind == AssistantCard::Image;
            const double full = picture || slot.folder >= 0 ? 1 : c01((w - (large - 60)) / 50); // only the large one shows the whole card
            if (full < 1) {
                p.save();
                p.setOpacity(p.opacity() * (1 - full));
                paintCompact(slot, r, i);
                p.restore();
            }
            if (full > 0) {
                p.save();
                p.setOpacity(p.opacity() * full);
                const int hitsBefore = int(d->hits.size());
                const AssistantCard::Kind partOf = slot.item >= 0 ? d->items.at(slot.item).card.kind : AssistantCard::Level;
                if (slot.folder >= 0 && partOf == AssistantCard::Notifications) {
                    // a notification: its app, when, what it said
                    const QStringList f = d->items.at(slot.item).card.lines.at(slot.folder).split('\t');
                    p.setPen(Qt::NoPen);
                    p.setBrush(hovered(r) ? mix(s.surfaceContainerHigh, s.onSurface, 0.08) : s.surfaceContainerHigh);
                    p.drawRect(r);
                    const QIcon appIcon = QIcon::fromTheme(f.value(1), QIcon::fromTheme(QStringLiteral("dialog-information")));
                    if (r.width() < 140) {
                        p.drawPixmap(QRectF(r.center().x() - 16, r.top() + 20, 32, 32).toRect(), appIcon.pixmap(64));
                    } else {
                        p.drawPixmap(QRectF(r.left() + 20, r.top() + 18, 24, 24).toRect(), appIcon.pixmap(48));
                        text(p, QRectF(r.left() + 52, r.top() + 18, r.width() - 130, 24), QFontMetricsF(flex(13, 600)).elidedText(f.value(0), Qt::ElideRight, r.width() - 130), flex(13, 600), s.onSurfaceVariant);
                        text(p, QRectF(r.right() - 74, r.top() + 18, 54, 24), f.value(4), flex(12, 500), s.onSurfaceVariant, Qt::AlignRight | Qt::AlignVCenter);
                        const QRectF summary(r.left() + 20, r.top() + 52, r.width() - 40, 26);
                        text(p, summary, QFontMetricsF(flex(16, 650)).elidedText(f.value(2), Qt::ElideRight, summary.width()), flex(16, 650), s.onSurface);
                        p.save();
                        p.setClipRect(QRectF(r.left() + 20, r.top() + 80, r.width() - 40, r.height() - 96), Qt::IntersectClip);
                        text(p, QRectF(r.left() + 20, r.top() + 80, r.width() - 40, r.height() - 96), f.value(3), flex(14, 450), s.onSurfaceVariant, Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap);
                        p.restore();
                    }
                    const QString app = f.value(0);
                    hit(r, [app](QPointF) {
                        if (AssistantTools::instance()->has(QStringLiteral("open_app"))) AssistantTools::instance()->tool(QStringLiteral("open_app")).run(QJsonObject{{"name", app}});
                    });
                } else if (slot.folder >= 0 && partOf == AssistantCard::Processes) {
                    // one busy program, live: its CPU big, its memory, Close (pressed twice)
                    const QStringList f = d->items.at(slot.item).card.lines.at(slot.folder).split('\t');
                    const int pid = f.value(0).toInt();
                    const QString name = f.value(1);
                    const bool gone = d->closedPids.contains(pid) || !QFileInfo::exists(QStringLiteral("/proc/%1").arg(pid));
                    const double cpu = gone ? 0 : cpuOfProcess(pid);
                    const qint64 memory = gone ? 0 : memoryOfProcess(pid);
                    p.setPen(Qt::NoPen);
                    p.setBrush(s.surfaceContainerHigh);
                    p.drawRect(r);
                    const QIcon appIcon = QIcon::fromTheme(name, QIcon::fromTheme(QStringLiteral("application-x-executable")));
                    if (r.width() < 140) {
                        p.drawPixmap(QRectF(r.center().x() - 16, r.top() + 20, 32, 32).toRect(), appIcon.pixmap(64));
                    } else {
                        p.drawPixmap(QRectF(r.left() + 20, r.top() + 20, 28, 28).toRect(), appIcon.pixmap(56));
                        text(p, QRectF(r.left() + 56, r.top() + 20, r.width() - 76, 28), QFontMetricsF(flex(16, 650)).elidedText(name, Qt::ElideRight, r.width() - 76), flex(16, 650), gone ? s.onSurfaceVariant : s.onSurface);
                        if (gone) {
                            icon(p, QRectF(r.left() + 20, r.top() + 64, 32, 40), QStringLiteral("check_circle"), s.primary, 28, true);
                            text(p, QRectF(r.left() + 58, r.top() + 64, r.width() - 80, 40), tr("Closed"), flex(22, 650), s.onSurfaceVariant);
                        } else {
                            QFont big = flex(34, 700, 110);
                            big.setFeature(QFont::Tag("tnum"), 1);
                            text(p, QRectF(r.left() + 20, r.top() + 58, r.width() - 40, 48), cpu < 0 ? QStringLiteral("…") : QStringLiteral("%1%").arg(qRound(cpu)), big, s.onSurface);
                            text(p, QRectF(r.left() + 22, r.top() + 104, r.width() - 44, 20), tr("CPU · %1").arg(QLocale().formattedDataSize(memory, 0, QLocale::DataSizeTraditionalFormat)), flex(13, 500), s.onSurfaceVariant);
                            const double shown = c01(glide(key + QString::number(pid), c01(qMax(0.0, cpu) / 100), M3Motion::SpatialSlow));
                            const QRectF bar(r.left() + 20, r.bottom() - 62, r.width() - 40, 8);
                            p.setBrush(s.secondaryContainer);
                            p.drawRoundedRect(bar, 4, 4);
                            p.setBrush(shown > 0.5 ? s.tertiary : s.primary);
                            p.drawRoundedRect(QRectF(bar.left(), bar.top(), qMax(8.0, bar.width() * shown), bar.height()), 4, 4);
                            // Close: once to arm, again within a few seconds to really close it
                            const bool armed = d->armedPid == pid && d->since(d->armedAt) < 4;
                            const QRectF close(r.right() - (armed ? 128 : 92), r.bottom() - 46, armed ? 108 : 72, 32);
                            p.setPen(armed ? Qt::NoPen : QPen(s.outlineVariant, 1));
                            p.setBrush(armed ? s.errorContainer : hovered(close) ? mix(s.surfaceContainerHigh, s.primary, 0.08) : QColor(Qt::transparent));
                            p.drawRoundedRect(close.adjusted(0.5, 0.5, -0.5, -0.5), 16, 16);
                            p.setPen(Qt::NoPen);
                            text(p, close, armed ? tr("Close it?") : tr("Close"), flex(13, 650), armed ? s.onErrorContainer : s.primary, Qt::AlignCenter);
                            hit(close, [this, pid](QPointF) {
                                if (d->armedPid == pid && d->since(d->armedAt) < 4) {
                                    ::kill(pid, SIGTERM); // the user's own program, asked to quit
                                    d->closedPids.insert(pid);
                                    d->armedPid = -1;
                                } else {
                                    d->armedPid = pid;
                                    d->armedAt = d->now();
                                }
                                animateFor(4200);
                            });
                        }
                    }
                    if (!d->secondTick && !gone) {
                        d->secondTick = true;
                        QTimer::singleShot(1000, this, [this] {
                            d->secondTick = false;
                            redraw();
                        });
                    }
                } else if (slot.folder >= 0 && partOf == AssistantCard::Files) {
                    // a file it found: a look inside (pictures, a PDF's first page), its name, where it is
                    const QString path = d->items.at(slot.item).card.lines.at(slot.folder);
                    const QString suffix = QFileInfo(path).suffix().toLower();
                    const bool previewable = suffix == "pdf" || QStringList{"png", "jpg", "jpeg", "webp", "gif", "bmp"}.contains(suffix);
                    const QImage preview = previewable ? imageOf(path) : QImage();
                    p.setPen(Qt::NoPen);
                    p.setBrush(s.surfaceContainerHighest);
                    p.drawRect(r);
                    const QRectF look(r.left(), r.top(), r.width(), r.height() - (r.width() < 140 ? 0 : 56));
                    if (!preview.isNull() && preview.width() > 1) {
                        // a page shows from its top, a picture from its middle
                        const QSizeF fit = QSizeF(preview.size()).scaled(look.size(), Qt::KeepAspectRatioByExpanding);
                        p.save();
                        p.setClipRect(look, Qt::IntersectClip);
                        p.setRenderHint(QPainter::SmoothPixmapTransform);
                        p.drawImage(QRectF(look.center().x() - fit.width() / 2, suffix == "pdf" ? look.top() : look.center().y() - fit.height() / 2, fit.width(), fit.height()), preview);
                        p.restore();
                    } else {
                        p.fillRect(look, slot.folder % 2 ? s.tertiaryContainer : s.secondaryContainer);
                        icon(p, look, suffix == "pdf" ? QStringLiteral("picture_as_pdf") : QStringLiteral("description"), slot.folder % 2 ? s.onTertiaryContainer : s.onSecondaryContainer, 36, true);
                    }
                    if (r.width() >= 140) {
                        text(p, QRectF(r.left() + 16, look.bottom() + 8, r.width() - 32, 22), QFontMetricsF(flex(14, 650)).elidedText(QFileInfo(path).fileName(), Qt::ElideMiddle, r.width() - 32), flex(14, 650), s.onSurface);
                        const QString folder = QString(QFileInfo(path).absolutePath()).replace(QDir::homePath(), QStringLiteral("~"));
                        text(p, QRectF(r.left() + 16, look.bottom() + 28, r.width() - 32, 20), QFontMetricsF(flex(12, 450)).elidedText(folder, Qt::ElideMiddle, r.width() - 32), flex(12, 450), s.onSurfaceVariant);
                    }
                    hit(r, [path](QPointF) {
                        QDesktopServices::openUrl(QUrl::fromLocalFile(path));
                    });
                } else if (slot.folder >= 0) {
                    // one of the disk's biggest folders: its name, its size big
                    const QStringList parts = d->items.at(slot.item).card.lines.at(slot.folder).split('\t');
                    const bool tonal = slot.folder % 2 == 0;
                    const QColor bg = tonal ? s.secondaryContainer : s.tertiaryContainer, ink = tonal ? s.onSecondaryContainer : s.onTertiaryContainer;
                    p.setPen(Qt::NoPen);
                    p.setBrush(hovered(r) ? mix(bg, ink, 0.08) : bg);
                    p.drawRect(r);
                    const QString size = QLocale().formattedDataSize(parts.value(1).toLongLong(), 0, QLocale::DataSizeTraditionalFormat);
                    if (r.width() < 110) icon(p, QRectF(r.center().x() - 16, r.top() + 20, 32, 32), QStringLiteral("folder"), ink, 26, true);
                    else {
                        icon(p, QRectF(r.left() + 20, r.top() + 20, 32, 32), QStringLiteral("folder"), ink, 28, true);
                        text(p, QRectF(r.left() + 22, r.bottom() - 92, r.width() - 44, 24), QFontMetricsF(flex(16, 600)).elidedText(parts.value(0), Qt::ElideRight, r.width() - 44), flex(16, 600), ink);
                        text(p, QRectF(r.left() + 22, r.bottom() - 66, r.width() - 44, 44), size, flex(30, 700, 110), ink);
                    }
                    const QString path = parts.value(2);
                    hit(r, [path](QPointF) {
                        QDesktopServices::openUrl(QUrl::fromLocalFile(path));
                    });
                } else if (slot.item >= 0) paintCard(slot.item, r);
                else {
                    p.setPen(Qt::NoPen);
                    p.setBrush(s.surfaceContainerHigh);
                    p.drawRect(r);
                    double ty = r.top() + 20;
                    for (int a : std::as_const(answers)) {
                        const Private::Item& answer = d->items.at(a);
                        ty += paintLiving(p, answer.text, answer.wordBorn, QPointF(r.left() + 24, ty), r.width() - 48, readPx, hit, hovered, answer.countFrom) + 12;
                    }
                }
                if (i != qRound(pos)) d->hits.resize(hitsBefore); // only the large one is live
                p.restore();
            }
            p.restore();
            solid(r);
            if (i != qRound(pos)) {
                hit(r, [this, i](QPointF) {
                    d->carousel->animateTo(double(i));
                    animateFor(900);
                });
            }
            x += w + gap;
        }
        y += height + 20;
    }

    // Follow-ups: a connected button group, the outer ends round, the joins tight
    if (chips >= 0 && !d->items.at(chips).chips.isEmpty()) {
        y = glide(key + QStringLiteral("chipsTop"), y);
        Private::Item& item = d->items[chips];
        const double age = (d->now() - item.born) / 1000.0;
        const QFont f = flex(14, 560);
        QList<double> widths;
        double total = -2;
        for (const AssistantChip& chip : std::as_const(item.chips)) {
            widths.append(24 + (chip.icon.isEmpty() ? 0 : 28) + textWidth(chip.label, f) + 4);
            total += widths.last() + 2;
        }
        double x = qMax(0.0, (W - total) / 2);
        for (int i = 0; i < item.chips.size(); i++) {
            const AssistantChip& chip = item.chips.at(i);
            const double k = springAt(M3Motion::SpatialFast, age - i * 0.07);
            if (k <= 0) continue;
            const QRectF r(x, y + (1 - c01(k)) * 16, widths.at(i), 40);
            const bool pressed = item.pressed == chip.id && d->since(item.pressedAt) < 0.3;
            const double press = pressed ? std::sin(M_PI * c01(d->since(item.pressedAt) / 0.3)) : 0;
            const double left = i == 0 ? lerp(20, 10, press) : lerp(6, 10, press), right = i == item.chips.size() - 1 ? lerp(20, 10, press) : lerp(6, 10, press);
            const QPainterPath shape = sidedRect(r, left, right);
            p.save();
            p.setOpacity(p.opacity() * c01(age / 0.2));
            p.setPen(Qt::NoPen);
            p.setBrush(pressed ? mix(s.secondaryContainer, s.onSecondaryContainer, 0.12) : hovered(r) ? mix(s.secondaryContainer, s.onSecondaryContainer, 0.08) : s.secondaryContainer);
            p.drawPath(shape);
            double cx = r.left() + 14;
            if (!chip.icon.isEmpty()) {
                icon(p, QRectF(cx, r.top(), 20, r.height()), chip.icon, s.onSecondaryContainer, 18);
                cx += 28;
            }
            text(p, QRectF(cx, r.top(), r.right() - cx - 8, r.height()), chip.label, f, s.onSecondaryContainer);
            p.restore();
            solid(r);
            const QString id = chip.id, label = chip.label;
            hit(r, [this, chips, id, label](QPointF) {
                if (d->busy) return;
                d->items[chips].pressed = id;
                d->items[chips].pressedAt = d->now();
                if (!d->brain->chip(id)) sendText(label);
                animateFor(400);
            });
            x += widths.at(i) + 2;
        }
        y += 40 + 16;
    }

    // "N earlier": opens the questions asked before as a list
    const int earlier = int(asked.size()) - (user >= 0 ? 1 : 0);
    if (earlier > 0 && !leaving) {
        y = glide(key + QStringLiteral("earlierTop"), y);
        const QString label = d->viewBack > 0 ? tr("Back to the latest") : earlier == 1 ? tr("1 earlier") : tr("%1 earlier").arg(earlier);
        const QString ic = d->viewBack > 0 ? QStringLiteral("arrow_downward") : QStringLiteral("history");
        const QRectF r((W - chipWidth(ic, label) - 8) / 2, y, chipWidth(ic, label) + 8, 36);
        p.setPen(QPen(s.outlineVariant, 1));
        p.setBrush(hovered(r) ? mix(s.surfaceContainer, s.onSurface, 0.08) : s.surfaceContainer);
        p.drawRoundedRect(r.adjusted(0.5, 0.5, -0.5, -0.5), 18, 18);
        p.setPen(Qt::NoPen);
        icon(p, QRectF(r.left() + 12, r.top(), 20, 36), ic, s.onSurfaceVariant, 18);
        text(p, r.adjusted(38, 0, 0, 0), label, flex(13, 600), s.onSurface);
        solid(r);
        hit(r, [this](QPointF) {
            if (d->viewBack > 0) {
                showTurn(0);
                return;
            }
            d->listOpen = true;
            d->listAt = d->now();
            d->scrollTarget = d->scroll = 0;
            animateFor(900);
        });
        y += 36 + 16;
    }
    return y;
}

void AssistantPanel::paintFocus(QPainter& p) {
    // M3 focus ring: 3 dp, secondary, 2 dp outside the thing focused
    if (d->focus < 0 || d->focus >= d->hits.size() || !this->hasFocus()) return;
    const QRectF r = d->hits.at(d->focus).rect.adjusted(-4, -4, 4, 4);
    p.save();
    p.setClipping(false);
    p.setOpacity(1);
    p.setPen(QPen(M3::currentScheme().secondary, 3));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(r, qMin(r.height() / 2, 16.0), qMin(r.height() / 2, 16.0));
    p.restore();
}

bool AssistantPanel::focusKey(QKeyEvent* event) {
    // Tab walks the chips, cards and buttons; Enter or Space presses; arrows move sliders
    const int count = int(d->hits.size());
    auto moveTo = [this](int focus) {
        d->focus = focus;
        if (focus < 0 && d->field->isVisible()) d->field->setFocus();
        else this->setFocus();
        this->update();
    };
    switch (event->key()) {
        case Qt::Key_Tab:
            if (count == 0) return false;
            moveTo(d->focus + 1 >= count ? (d->field->isVisible() ? -1 : 0) : d->focus + 1);
            return true;
        case Qt::Key_Backtab:
            if (count == 0) return false;
            moveTo(d->focus < 0 ? count - 1 : d->focus - 1);
            return true;
        case Qt::Key_Return:
        case Qt::Key_Enter:
        case Qt::Key_Space:
            if (d->focus < 0 || d->focus >= count) return false;
            d->hits.at(d->focus).act(d->hits.at(d->focus).rect.center());
            return true;
        case Qt::Key_Left:
        case Qt::Key_Right:
            if (d->focus < 0 || d->focus >= count || !d->hits.at(d->focus).nudge) return false;
            d->hits.at(d->focus).nudge(event->key() == Qt::Key_Left ? -1 : 1);
            return true;
        case Qt::Key_Escape:
            if (d->focus < 0) return false;
            moveTo(-1);
            return true;
        default:
            return false;
    }
}

double AssistantPanel::paintLiving(QPainter& p, const QString& raw, const QList<qint64>& wordBorn, QPointF at, double W, double px, const HitFn& hit, const HoverFn& hovered, const QHash<QString, double>& countFrom, bool centred) {
    // Living type: words arrive thin and narrow and spring to their weight.
    // Key words land heavy, wide and in primary (and stay); a key number
    // counts up; files, apps and links turn into chips; the tone shapes the
    // whole answer; <t …> spans are Pebble's own styling
    const M3::Scheme s = M3::currentScheme();
    const ParsedAnswer parsed = parseAnswer(raw);
    const QFontMetricsF metrics(flex(px, 480));
    const QFontMetricsF keyMetrics(flex(px, 700, 110));
    const double space = metrics.horizontalAdvance(' '), lineHeight = metrics.height() * 1.3;
    const double toneAt = wordBorn.isEmpty() ? -1 : (wordBorn.last() + 350);
    static const QRegularExpression number(QStringLiteral("^(\\d+)(%?)([.,!?:;]?)$"));

    // What a word looks like once it has landed (a style's ramp runs across its words)
    struct Look {
            double wght = 480, wdth = 100, slnt = 0, size = 1;
            const WordStyle* style = nullptr;
    };
    auto lookOf = [&](const AnswerToken& token) {
        Look look;
        if (token.kind == 1) look.wght = 700, look.wdth = 110;
        if (token.style < 0 || token.kind > 1) return look;
        const WordStyle& style = parsed.styles.at(token.style);
        const double f = style.words > 1 ? double(token.inStyle) / (style.words - 1) : 0;
        if (style.wght[0] >= 0) look.wght = lerp(style.wght[0], style.wght[1], f);
        if (style.wdth[0] >= 0) look.wdth = lerp(style.wdth[0], style.wdth[1], f);
        look.size = lerp(style.size[0], style.size[1], f);
        look.slnt = style.slnt;
        look.style = &style;
        return look;
    };

    // First the layout: where each word goes, and how tall each line is
    struct Place {
            double x = 0, ww = 0;
            int line = 0;
    };
    QList<Place> places(parsed.tokens.size());
    QList<double> lineSize{1};
    double x = 0;
    for (int t = 0; t < parsed.tokens.size(); t++) {
        const AnswerToken& token = parsed.tokens.at(t);
        if (token.kind == 4) {
            x = 0;
            lineSize.append(1);
            continue;
        }
        const double born = t < wordBorn.size() ? d->since(wordBorn.at(t)) : -1;
        const double chipK = token.kind >= 2 ? c01(springAt(M3Motion::SpatialDefault, born - 0.55)) : 0;
        const Look look = lookOf(token);
        const double ww = (look.style ? QFontMetricsF(flex(px * look.size, look.wght, look.wdth)) : token.kind == 1 ? keyMetrics : metrics).horizontalAdvance(token.text) + 34 * chipK;
        if (token.glued && x > 0) x -= space;
        if (x > 0 && x + ww > W) {
            x = 0;
            lineSize.append(1);
        }
        places[t] = {x, ww, int(lineSize.size()) - 1};
        lineSize.last() = qMax(lineSize.last(), look.size);
        x += ww + space;
    }
    QList<double> lineTop{at.y()};
    for (double size : std::as_const(lineSize)) lineTop.append(lineTop.last() + lineHeight * qMax(1.0, size));
    // centred: each line sits in the middle of the width
    QList<double> lineShift(lineSize.size(), 0.0);
    if (centred) {
        QList<double> lineWidth(lineSize.size(), 0.0);
        for (int t = 0; t < parsed.tokens.size(); t++)
            if (parsed.tokens.at(t).kind != 4) lineWidth[places.at(t).line] = qMax(lineWidth.at(places.at(t).line), places.at(t).x + places.at(t).ww);
        for (int l = 0; l < lineSize.size(); l++) lineShift[l] = qMax(0.0, (W - lineWidth.at(l)) / 2);
    }

    for (int t = 0; t < parsed.tokens.size(); t++) {
        const AnswerToken& token = parsed.tokens.at(t);
        if (token.kind == 4) continue;
        Place place = places.at(t);
        place.x += lineShift.at(place.line);
        const double born = t < wordBorn.size() ? d->since(wordBorn.at(t)) : -1;
        const double chipK = token.kind >= 2 ? c01(springAt(M3Motion::SpatialDefault, born - 0.55)) : 0;
        const double k = born > 0 ? springAt(M3Motion::SpatialDefault, born) : 0;
        if (k <= 0) continue;
        const Look look = lookOf(token);
        double wght = lerp(100, look.wght, k), wdth = lerp(25, look.wdth, k), slnt = look.slnt * c01(k);
        if (parsed.tone == 1 && toneAt >= 0 && !look.style) wght += 80 * c01(springAt(M3Motion::SpatialDefault, d->since(qint64(toneAt)) - t * 0.02));
        if (parsed.tone == 2 && !look.style) {
            wght = lerp(100, 400, k);
            slnt = -9 * c01(k);
        }
        if (parsed.tone == 3 && toneAt >= 0 && d->since(qint64(toneAt)) > 0) {
            const double u = d->since(qint64(toneAt));
            wdth += 22 * std::sin(u * 20) * std::exp(-u * 4.5);
        }
        QColor ink = alpha(s.onSurface, k * 2.5);
        double scale = 1, dx = 0, dy = 0;
        QString shown = token.text;
        if (token.kind == 1) {
            scale = lerp(1.35, 1.0, c01(k));
            ink = alpha(s.primary, k * 2.5);
            const QRegularExpressionMatch m = number.match(token.text);
            if (m.hasMatch()) shown = QString::number(qRound(lerp(countFrom.value(m.captured(2), 0), m.captured(1).toDouble(), c01(springAt(M3Motion::SpatialSlow, born))))) + m.captured(2) + m.captured(3);
        }
        if (look.style) {
            const QString& c = look.style->colour;
            const QColor role = c == "primary" ? s.primary : c == "secondary" ? s.secondary : c == "tertiary" ? s.tertiary : c == "error" ? s.error : c == "muted" ? s.onSurfaceVariant : QColor::fromString(c);
            if (role.isValid()) ink = alpha(role, k * 2.5);
            // an effect plays once, from when the word lands (so the frames stop again)
            const QString& fx = look.style->fx;
            const double u = born - 0.35, i = token.inStyle;
            if (!fx.isEmpty() && u > 0 && u < 2.2) {
                d->animateUntil = qMax(d->animateUntil, d->now() + 40);
                const double fade = std::exp(-u * 1.8);
                if (fx == "wave") dy = -7 * look.size * std::sin(u * 9 - i * 0.8) * fade;
                else if (fx == "shake") dx = 4 * std::sin(u * 42 + i) * std::exp(-u * 3.5);
                else if (fx == "pulse") wght += 300 * std::sin(u * 7 - i * 0.7) * fade;
                else if (fx == "stretch") wdth += 45 * std::sin(u * 7 - i * 0.7) * fade;
                else if (fx == "pop") scale *= 1 + 0.35 * std::sin(M_PI * c01((u - i * 0.06) / 0.45)) * (u - i * 0.06 > 0);
            }
        }
        QFont font = flex(px * look.size, wght, wdth);
        font.setFeature(QFont::Tag("tnum"), 1);
        font.setVariableAxis(QFont::Tag("slnt"), float(std::round(slnt)));
        const double lineH = lineTop.at(place.line + 1) - lineTop.at(place.line);
        const double baselineOfLine = lineTop.at(place.line) + lineH - lineHeight + metrics.ascent();
        const double top = baselineOfLine - metrics.ascent();
        const double baseline = baselineOfLine + 10 * (1 - k) + dy;
        const double ww = place.ww;
        p.save();
        if (token.kind >= 2) {
            if (chipK > 0.01) {
                const QRectF pill(at.x() + place.x - 4, top + 1, ww + 8, lineHeight - 4);
                p.setPen(Qt::NoPen);
                p.setBrush(alpha(hovered(pill) ? mix(s.secondaryContainer, s.onSecondaryContainer, 0.08) : s.secondaryContainer, chipK));
                p.drawRoundedRect(pill, 9, 9);
                p.save();
                p.translate(pill.left() + 17, pill.center().y());
                p.scale(chipK, chipK);
                const QString suffix = QFileInfo(token.target).suffix().toLower();
                const QString ic = token.kind == 5 ? QStringLiteral("link") : token.kind == 3 ? QStringLiteral("apps") : QStringList{"png", "jpg", "jpeg", "webp", "gif", "svg"}.contains(suffix) ? QStringLiteral("image") : QStringList{"css", "html", "js", "ts", "py", "cpp", "h", "rs", "sh", "json"}.contains(suffix) ? QStringLiteral("code") : QFileInfo(token.target).suffix().isEmpty() ? QStringLiteral("folder") : QStringLiteral("description");
                icon(p, QRectF(-10, -10, 20, 20), ic, s.onSecondaryContainer, 17);
                p.restore();
                ink = mix(ink, s.onSecondaryContainer, chipK);
                if (token.kind == 3) {
                    const QString app = token.target;
                    hit(pill, [app](QPointF) {
                        if (AssistantTools::instance()->has(QStringLiteral("open_app"))) AssistantTools::instance()->tool(QStringLiteral("open_app")).run(QJsonObject{{"name", app}});
                    });
                }
                if (token.kind == 5) {
                    const QUrl url(token.target);
                    hit(pill, [url](QPointF) {
                        QDesktopServices::openUrl(url);
                    });
                }
                if (token.kind == 2) {
                    const QString path = QString(token.target).replace(QRegularExpression(QStringLiteral("^~")), QDir::homePath());
                    hit(pill, [path](QPointF) {
                        QDesktopServices::openUrl(QUrl::fromLocalFile(path));
                    });
                }
            }
            p.setFont(font);
            p.setPen(ink);
            p.drawText(QPointF(at.x() + place.x + 30 * chipK, baseline), shown);
        } else {
            const QFontMetricsF own(font);
            const double cx = at.x() + place.x + ww / 2 + dx;
            p.setFont(font);
            p.setPen(ink);
            p.translate(cx, baseline - own.ascent() / 2.6);
            p.scale(scale, scale);
            p.drawText(QPointF(-own.horizontalAdvance(shown) / 2, own.ascent() / 2.6), shown);
        }
        p.restore();
    }
    return lineTop.last() - at.y();
}

void AssistantPanel::mousePressEvent(QMouseEvent* event) {
    const QPointF at = event->position();
    for (int i = int(d->hits.size()) - 1; i >= 0; i--) {
        if (!d->hits.at(i).rect.contains(at)) continue;
        if (d->hits.at(i).drags) d->dragging = i;
        d->hits.at(i).act(at);
        return;
    }
    // anywhere Pebble didn't draw: away it goes (into the bar while it works)
    for (const QRectF& r : std::as_const(d->solid))
        if (r.contains(at)) return;
    close();
}

void AssistantPanel::mouseMoveEvent(QMouseEvent* event) {
    d->mouse = event->position();
    if (d->dragging >= 0 && d->dragging < d->hits.size()) {
        d->hits.at(d->dragging).act(d->mouse);
        return;
    }
    bool over = false;
    for (const Private::Hit& hit : std::as_const(d->hits)) over = over || hit.rect.contains(d->mouse);
    this->setCursor(over ? Qt::PointingHandCursor : Qt::ArrowCursor);
    redraw();
}

void AssistantPanel::mouseReleaseEvent(QMouseEvent* event) {
    Q_UNUSED(event)
    d->dragging = -1;
}

void AssistantPanel::wheelEvent(QWheelEvent* event) {
    // sideways: through the carousel; up past the top: back to earlier turns
    if (std::abs(event->angleDelta().x()) > std::abs(event->angleDelta().y())) {
        d->carousel->animateTo(qMax(0.0, std::round(d->carousel->endValue().toDouble() + (event->angleDelta().x() < 0 ? 1 : -1))));
        animateFor(900);
        return;
    }
    const double max = qMax(0.0, d->contentHeight - d->viewHeight);
    const double dy = event->angleDelta().y();
    d->scrollTarget = qBound(0.0, d->scrollTarget - dy / 2.0, max);
    animateFor(600);
}

void AssistantPanel::changeEvent(QEvent* event) {
    QDialog::changeEvent(event);
    if (event->type() == QEvent::ActivationChange && !this->isActiveWindow() && d->open) close();
}

bool AssistantPanel::eventFilter(QObject* watched, QEvent* event) {
    if (watched == d->field && event->type() == QEvent::KeyPress) {
        auto* key = static_cast<QKeyEvent*>(event);
        if ((key->key() == Qt::Key_Tab || key->key() == Qt::Key_Backtab) && focusKey(key)) return true;
        if (key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) {
            send();
            return true;
        }
        if (key->key() == Qt::Key_Escape) {
            if (d->listOpen) { // the list of earlier ones goes first
                d->listOpen = false;
                animateFor(600);
            } else close();
            return true;
        }

    }
    return QDialog::eventFilter(watched, event);
}

QRectF AssistantPanel::introCard() const {
    const double w = qMin(986.0, this->width() - 120.0), h = qMin(600.0, this->height() - 140.0);
    return QRectF((this->width() - w) / 2, qMax(56.0, (this->height() - h) / 2 + 20), w, h);
}

void AssistantPanel::paintIntro(QPainter& p) {
    // The first run, in theDesk's onboarding style: the steps on the left,
    // shapes at the edges, wavy progress, a hero shape, words in Pebble's own
    // living type. No sound. It ends by turning into the sheet.
    const M3::Scheme s = M3::currentScheme();
    p.setRenderHint(QPainter::Antialiasing);
    const QRectF card = introCard();
    const double since = d->since(d->introAt);
    auto hovered = [this](const QRectF& r) {
        return r.contains(d->mouse);
    };
    auto hit = [this](const QRectF& r, std::function<void(QPointF)> act) {
        d->hits.append({r, act});
    };

    // turning into the sheet
    const double away = d->introToPanelAt >= 0 ? c01(springAt(M3Motion::SpatialDefault, d->since(d->introToPanelAt))) : 0;
    const QRectF box = lerpRect(card, pillRect(), away);
    const double appear = springAt(M3Motion::SpatialDefault, d->since(d->openedAt >= 0 ? d->openedAt : 0));
    p.save();
    if (away == 0 && d->openedAt >= 0) {
        p.translate(box.center());
        p.scale(lerp(0.92, 1, appear), lerp(0.92, 1, appear));
        p.translate(-box.center());
    }
    p.setPen(Qt::NoPen);
    p.setBrush(s.surfaceContainer);
    p.drawRoundedRect(box, lerp(28, PillHeight / 2, away), lerp(28, PillHeight / 2, away));
    p.setClipRect(box);
    p.setOpacity(1 - c01(away * 3));
    if (p.opacity() <= 0) {
        p.restore();
        return;
    }

    // shapes at the card's edges, drifting
    const double t = d->now() / 1000.0;
    struct Shape {
            const m3shapes::RoundedPolygon* polygon;
            QPointF at;
            double size;
            QColor colour;
            double spin;
    };
    const QList<Shape> shapes{{&MaterialShapes::sunny(), {card.right() - 70, card.top() + 120}, 44, s.tertiary, 14},
        {&MaterialShapes::cookie6Sided(), {card.left() + card.width() * 0.53, card.bottom() - 30}, 70, s.secondaryContainer, -9},
        {&MaterialShapes::circle(), {card.right() - 40, card.bottom() - 130}, 30, s.primary, 0},
        {&MaterialShapes::softBurst(), {card.right() - 210, card.bottom() + 10}, 110, s.tertiaryContainer, 6}};
    for (const Shape& shape : shapes) {
        p.save();
        p.translate(shape.at + QPointF(6 * std::sin(t * 0.7 + shape.size), 5 * std::cos(t * 0.5 + shape.size)));
        if (M3Motion::expressive()) p.rotate(t * shape.spin);
        p.setBrush(shape.colour);
        p.drawPath(m3shapes::toPath(*shape.polygon, QRectF(-shape.size / 2, -shape.size / 2, shape.size, shape.size)));
        p.restore();
    }

    // the steps
    const int step = qMax(0, d->introStep);
    const QList<QPair<QString, QString>> steps{{"waving_hand", tr("Hello")}, {"visibility", tr("What I see")}, {"verified_user", tr("Asking first")}, {"palette", tr("My colour")}, {"keyboard", tr("Try it")}};
    const double sy = card.top() + 170;
    p.setBrush(s.secondaryContainer);
    p.drawRoundedRect(QRectF(card.left() + 14, sy + step * 34 - 2, 170, 32), 16, 16);
    for (int i = 0; i < steps.size(); i++) {
        const QRectF row(card.left() + 22, sy + i * 34, 160, 28);
        const bool done = i < step;
        p.setBrush(i == step ? s.primary : done ? s.primaryContainer : s.surfaceContainerHighest);
        p.drawEllipse(QPointF(row.left() + 11, row.center().y()), 10, 10);
        icon(p, QRectF(row.left() + 1, row.top() + 4, 20, 20), done ? QStringLiteral("check") : steps.at(i).first, i == step ? s.onPrimary : s.onSurfaceVariant, 13, true);
        text(p, row.adjusted(30, 0, 0, 0), steps.at(i).second, flex(13, i == step ? 650 : 480), i == step ? s.onSecondaryContainer : s.onSurfaceVariant);
    }

    const QRectF content(card.left() + 230, card.top() + 28, card.width() - 270, card.height() - 56);
    // M3E wavy progress (amplitude 3, wavelength 40, stroke 4, gap, then the track)
    if (step > 0) {
        const QRectF bar(content.left(), content.top(), content.width(), 10);
        const double progress = lerp((step - 1) / 4.0, step / 4.0, c01(springAt(M3Motion::SpatialSlow, since)));
        const double end = bar.left() + bar.width() * progress;
        QPainterPath wave;
        for (double x = bar.left(); x <= end; x += 2) {
            const QPointF pt(x, bar.center().y() + 3 * std::sin((x - bar.left()) / 40.0 * 2 * M_PI - t * 5));
            if (x == bar.left()) wave.moveTo(pt);
            else wave.lineTo(pt);
        }
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(s.primary, 4, Qt::SolidLine, Qt::RoundCap));
        p.drawPath(wave);
        if (end + 8 < bar.right()) {
            p.setPen(QPen(s.secondaryContainer, 4, Qt::SolidLine, Qt::RoundCap));
            p.drawLine(QPointF(end + 8, bar.center().y()), QPointF(bar.right(), bar.center().y()));
        }
        p.setPen(Qt::NoPen);
    }

    // Words in Pebble's living type, arriving from [delay] into this step
    auto words = [&](const QString& raw, QPointF at, double width, double px, double delay) {
        QList<qint64> births;
        const int n = int(parseAnswer(raw).tokens.size());
        for (int i = 0; i < n; i++) births.append(d->introAt + qint64(delay * 1000) + i * 65);
        return paintLiving(p, raw, births, at, width, px, hit, hovered);
    };
    auto hero = [&](const QString& ic) {
        const double k = c01(springAt(M3Motion::SpatialDefault, since));
        const QPointF c(content.left() + 36, content.top() + 70);
        p.save();
        p.translate(c);
        p.scale(k, k);
        p.save();
        p.rotate((1 - k) * -60 + (M3Motion::expressive() ? t * 6 : 0));
        p.setBrush(s.primaryContainer);
        p.drawPath(m3shapes::toPath(MaterialShapes::cookie9Sided(), QRectF(-36, -36, 72, 72)));
        p.restore();
        icon(p, QRectF(-36, -36, 72, 72), ic, s.onPrimaryContainer, 30, true);
        p.restore();
    };
    auto pill = [&](const QRectF& r, const QString& ic, const QString& label, std::function<void()> act) {
        if (since < 0.6) return;
        p.save();
        rise(p, r, since - 0.6);
        p.setBrush(hovered(r) ? mix(s.primaryContainer, s.onPrimaryContainer, 0.08) : s.primaryContainer);
        p.drawRoundedRect(r, r.height() / 2, r.height() / 2);
        icon(p, QRectF(r.left() + 14, r.top(), 22, r.height()), ic, s.onPrimaryContainer, 19);
        text(p, r.adjusted(42, 0, 0, 0), label, flex(15, 600), s.onPrimaryContainer);
        p.restore();
        hit(r, [act](QPointF) { act(); });
    };
    auto nextAndBack = [&] {
        pill(QRectF(content.right() - 128, content.bottom() - 44, 128, 44), QStringLiteral("arrow_forward"), tr("Next"), [this] { introNext(); });
        const QRectF back(content.left() - 8, content.bottom() - 44, 90, 44);
        p.setBrush(hovered(back) ? alpha(s.primary, 0.08) : QColor(Qt::transparent));
        p.drawRoundedRect(back, 22, 22);
        text(p, back, tr("Back"), flex(15, 600), s.primary, Qt::AlignCenter);
        hit(back, [this](QPointF) { introBack(); });
    };
    const QPointF headAt(content.left(), content.top() + 120);
    const QPointF bodyAt(content.left(), content.top() + 172);
    const auto [faceInk, faceFill] = faceColours(s, false);

    if (step == 0) {
        // Pebble's own opening: its face grows and thinks through its shapes,
        // settles, then pulls its name out from behind itself
        const QString name = Name;
        const QFont big = flex(120, 760, 110);
        const double baseY = content.top() + 130;
        const QPointF face(content.left() + 56, baseY - 42);
        const double grow = c01(springAt(M3Motion::SpatialSlow, since));
        const QRectF faceRect(face.x() - 56 * grow, face.y() - 56 * grow, 112 * grow, 112 * grow);
        if (since < 1.6) M3LoadingIndicator::paint(&p, faceRect, d->now() - d->introAt, faceInk, faceFill);
        else M3LoadingIndicator::paintDeterminate(&p, faceRect, 0.0f, faceInk, faceFill);
        if (since > 1.6) {
            const double k = c01(springAt(M3Motion::SpatialDefault, since - 1.6));
            p.save();
            p.setClipRect(QRectF(face.x(), baseY - 140, (QFontMetricsF(big).horizontalAdvance(name) + 90) * k, 200), Qt::IntersectClip);
            p.setFont(flex(120, lerp(300, 760, k), lerp(50, 110, k)));
            p.setPen(s.primary);
            p.drawText(QPointF(face.x() + 76 - 60 * (1 - k), baseY), name);
            p.restore();
        }
        const double h = words(tr("Hey. I'm your desk's own assistant."), QPointF(content.left(), baseY + 40), content.width(), 30, 2.2);
        words(tr("I change settings, look at things with you and get stuff done. Super+Space, any time."), QPointF(content.left(), baseY + 52 + h), content.width() * 0.8, 17, 2.7);
        if (since > 3.2) {
            const QRectF b(content.right() - 160, content.bottom() - 44, 160, 44);
            p.save();
            rise(p, b, since - 3.2);
            p.setBrush(hovered(b) ? mix(s.primaryContainer, s.onPrimaryContainer, 0.08) : s.primaryContainer);
            p.drawRoundedRect(b, 22, 22);
            icon(p, QRectF(b.left() + 14, b.top(), 22, 44), QStringLiteral("arrow_forward"), s.onPrimaryContainer, 19);
            text(p, b.adjusted(42, 0, 0, 0), tr("Get started"), flex(15, 600), s.onPrimaryContainer);
            p.restore();
            hit(b, [this](QPointF) { introNext(); });
        }
    } else if (step == 1) {
        hero(QStringLiteral("visibility"));
        words(tr("I only see what you let me"), headAt, content.width(), 32, 0.15);
        words(tr("Each time you open me, these show what I can see. Tap the x to hide one."), bodyAt, content.width() * 0.8, 16, 0.5);
        if (since > 0.8) {
            const QRectF row(content.left(), content.top() + 272, 420, 34);
            p.save();
            rise(p, row, since - 0.8);
            text(p, QRectF(row.left(), row.top(), 50, 34), tr("Sees"), flex(14, 600), s.onSurfaceVariant);
            const QList<QPair<QString, QString>> chips{{"terminal", tr("kitty · ~/site")}, {"content_paste", tr("Copied text")}, {"screenshot_monitor", tr("Screen")}};
            double x = row.left() + 50;
            for (int i = 0; i < chips.size(); i++) {
                const double full = chipWidth(chips.at(i).first, chips.at(i).second) + 24;
                const double k = i == 1 && d->introSeesGoneAt >= 0 ? 1 - c01(springAt(M3Motion::SpatialFast, d->since(d->introSeesGoneAt))) : 1;
                if (k <= 0.01) continue;
                const QRectF r(x, row.top(), full * k, 34);
                p.save();
                p.setOpacity(p.opacity() * c01(k * 1.5));
                p.setClipRect(r.adjusted(-1, -1, 1, 1), Qt::IntersectClip);
                p.setPen(QPen(s.outlineVariant, 1));
                p.setBrush(hovered(r) ? mix(s.surfaceContainer, s.onSurface, 0.08) : s.surfaceContainer);
                p.drawRoundedRect(r.adjusted(0.5, 0.5, -0.5, -0.5), 8, 8);
                p.setPen(Qt::NoPen);
                icon(p, QRectF(r.left() + 8, r.top(), 20, 34), chips.at(i).first, s.onSurfaceVariant, 17);
                text(p, QRectF(r.left() + 32, r.top(), full - 56, 34), chips.at(i).second, flex(13, 520), s.onSurface);
                icon(p, QRectF(r.left() + full - 28, r.top(), 24, 34), QStringLiteral("close"), s.onSurfaceVariant, 16);
                p.restore();
                if (i == 1 && d->introSeesGoneAt < 0) hit(r, [this](QPointF) {
                        d->introSeesGoneAt = d->now();
                        animateFor(600);
                    });
                x += (full + 8) * k;
            }
            p.restore();
        }
        nextAndBack();
    } else if (step == 2) {
        hero(QStringLiteral("verified_user"));
        words(tr("I ask before anything risky"), headAt, content.width(), 32, 0.15);
        words(tr("Running commands, changing files and closing windows wait for your yes."), bodyAt, content.width() * 0.8, 16, 0.5);
        if (since > 0.8) {
            const double settle = d->introAskAt >= 0 ? c01(springAt(M3Motion::SpatialDefault, d->since(d->introAskAt))) : 0;
            const QRectF ask(content.left(), content.top() + 262, 520, lerp(150, 52, settle));
            p.save();
            rise(p, ask, since - 0.8);
            p.setBrush(d->introAsk > 0 ? s.secondaryContainer : s.surfaceContainerHigh);
            p.drawRoundedRect(ask, 24, 24);
            p.setClipRect(ask, Qt::IntersectClip);
            p.setBrush(d->introAsk > 0 ? s.primary : s.tertiaryContainer);
            p.drawEllipse(QPointF(ask.left() + 30, ask.top() + 26), 16, 16);
            icon(p, QRectF(ask.left() + 14, ask.top() + 10, 32, 32), d->introAsk > 0 ? QStringLiteral("check") : d->introAsk < 0 ? QStringLiteral("block") : QStringLiteral("shield_question"), d->introAsk > 0 ? s.onPrimary : s.onTertiaryContainer, 18, true);
            text(p, QRectF(ask.left() + 56, ask.top() + 8, 400, 36), d->introAsk > 0 ? tr("Allowed · Run this") : d->introAsk < 0 ? tr("Not allowed · Run this") : tr("Run this?"), flex(15, 600), s.onSurface);
            p.setOpacity(p.opacity() * (1 - settle));
            p.setBrush(s.surfaceContainerLowest);
            p.drawRoundedRect(QRectF(ask.left() + 12, ask.top() + 56, ask.width() - 24, 34), 12, 12);
            text(p, QRectF(ask.left() + 24, ask.top() + 56, 400, 34), QStringLiteral("rm -r ~/Downloads/old"), mono(13, 450), s.onSurface);
            if (d->introAsk == 0) {
                const QRectF allow(ask.right() - 108, ask.top() + 100, 92, 36), deny(ask.right() - 208, ask.top() + 100, 92, 36);
                p.setPen(QPen(s.outlineVariant, 1));
                p.setBrush(hovered(deny) ? alpha(s.primary, 0.08) : QColor(Qt::transparent));
                p.drawRoundedRect(deny, 18, 18);
                p.setPen(Qt::NoPen);
                text(p, deny, tr("Don't"), flex(13, 600), s.primary, Qt::AlignCenter);
                p.setBrush(hovered(allow) ? mix(s.primary, s.onPrimary, 0.08) : s.primary);
                p.drawRoundedRect(allow, 18, 18);
                text(p, allow, tr("Allow"), flex(13, 600), s.onPrimary, Qt::AlignCenter);
                auto decide = [this](int answer) {
                    d->introAsk = answer;
                    d->introAskAt = d->now();
                    animateFor(800);
                };
                hit(allow, [decide](QPointF) { decide(1); });
                hit(deny, [decide](QPointF) { decide(-1); });
            }
            p.restore();
        }
        nextAndBack();
    } else if (step == 3) {
        // Arc's moment: one playful choice
        hero(QStringLiteral("palette"));
        words(tr("Pick my colour"), headAt, content.width(), 32, 0.15);
        words(tr("It's me, thinking. Tap one."), bodyAt, content.width() * 0.8, 16, 0.5);
        const QStringList keys{"primary", "secondary", "tertiary", "ink"};
        const QStringList names{tr("Primary"), tr("Calm"), tr("Bright"), tr("Ink")};
        const QList<QPair<QColor, QColor>> colours{{s.onPrimaryContainer, s.primaryContainer}, {s.onSecondaryContainer, s.secondaryContainer}, {s.onTertiaryContainer, s.tertiaryContainer}, {s.inverseOnSurface, s.inverseSurface}};
        const QString current = tSettings().value(QStringLiteral("Assistant/face")).toString();
        for (int i = 0; i < 4; i++) {
            const double t0 = 0.8 + i * 0.08;
            if (since < t0) continue;
            const QPointF c(content.left() + 70 + i * 150, content.top() + 330);
            const bool chosen = keys.at(i) == (current.isEmpty() ? QStringLiteral("primary") : current);
            const double grow = c01(springAt(M3Motion::SpatialDefault, since - t0)) * (chosen ? lerp(1, 1.2, d->introPickAt >= 0 ? c01(springAt(M3Motion::SpatialDefault, d->since(d->introPickAt))) : 1) : 1);
            p.save();
            p.translate(c);
            p.scale(grow, grow);
            M3LoadingIndicator::paint(&p, QRectF(-42, -42, 84, 84), d->now() - d->introAt + i * 300, colours.at(i).first, colours.at(i).second);
            if (chosen) {
                p.setPen(QPen(s.primary, 3));
                p.setBrush(Qt::NoBrush);
                p.drawEllipse(QPointF(0, 0), 52, 52);
                p.setPen(Qt::NoPen);
            }
            p.restore();
            text(p, QRectF(c.x() - 60, c.y() + 60, 120, 24), names.at(i), flex(14, chosen ? 650 : 480), chosen ? s.primary : s.onSurfaceVariant, Qt::AlignCenter);
            const QString key = keys.at(i);
            hit(QRectF(c.x() - 56, c.y() - 56, 112, 140), [this, key](QPointF) {
                tSettings().setValue(QStringLiteral("Assistant/face"), key);
                d->introPickAt = d->now();
                animateFor(800);
            });
        }
        nextAndBack();
    } else {
        hero(QStringLiteral("keyboard"));
        words(tr("Now you try"), headAt, content.width(), 32, 0.15);
        words(tr("Press Super and Space."), bodyAt, content.width() * 0.8, 16, 0.5);
        double kx = content.left();
        const QList<QPair<QString, double>> keys{{tr("Super"), 150}, {tr("Space"), 320}};
        for (int i = 0; i < keys.size(); i++) {
            const QRectF k(kx, content.top() + 290, keys.at(i).second, 72);
            const double pulse = M3Motion::expressive() ? 0.5 + 0.5 * std::sin(t * 4 - i) : 0.5;
            p.setBrush(s.surfaceContainerHighest.darker(130));
            p.drawRoundedRect(k.translated(0, 6), 16, 16);
            p.setBrush(mix(s.surfaceContainerHighest, s.primaryContainer, 0.25 * pulse));
            p.drawRoundedRect(k, 16, 16);
            text(p, k, keys.at(i).first, flex(22, 650), s.onSurface, Qt::AlignCenter);
            hit(k, [this](QPointF) { finishIntro(); });
            kx += keys.at(i).second + 20;
            if (i == 0) {
                text(p, QRectF(kx - 16, k.top(), 12, 72), QStringLiteral("+"), flex(26, 600), s.onSurfaceVariant, Qt::AlignCenter);
                kx += 4;
            }
        }
        const QRectF back(content.left() - 8, content.bottom() - 44, 90, 44);
        text(p, back, tr("Back"), flex(15, 600), s.primary, Qt::AlignCenter);
        hit(back, [this](QPointF) { introBack(); });
    }
    p.restore();
    animateFor(100); // the shapes drift and the faces think while it shows
}

void AssistantPanel::keyPressEvent(QKeyEvent* event) {
    if (focusKey(event)) return;
    if (d->introStep < 0 && d->focus >= 0 && !event->text().isEmpty() && event->text().at(0).isPrint()) {
        // typing goes back to the field
        d->focus = -1;
        d->field->setFocus();
        d->field->insert(event->text());
        return;
    }
    if (d->introStep >= 0) {
        const bool forward = event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter || event->key() == Qt::Key_Right;
        if (forward && d->introStep < 4) introNext();
        else if (forward || event->key() == Qt::Key_Escape) finishIntro(); // the last step, or skipping: done, it doesn't come back
        else if (event->key() == Qt::Key_Left || event->key() == Qt::Key_Backspace) introBack();
        return;
    }
    // not QDialog's: its Escape rejects, past close() (and the bar while it works)
    if (event->key() == Qt::Key_Escape) close();
}
