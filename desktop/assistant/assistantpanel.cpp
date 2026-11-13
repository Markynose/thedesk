#include "assistantpanel.h"

#include "../clipboard/clipboardhistory.h"
#include "assistantchunk.h"
#include "assistantface.h"
#include "selectionbutton.h"
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

namespace {
    constexpr double SheetWidth = 720, FieldHeight = 60, Edge = 20, Side = 28, Radius = 36;
    constexpr double ScrimOpacity = 0.45;
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
        font.setVariableAxis(QFont::Tag("wght"), float(qBound(100.0, wght, 1000.0)));
        font.setVariableAxis(QFont::Tag("wdth"), float(qBound(25.0, wdth, 151.0)));
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
    };
    struct ParsedAnswer {
            int tone = 0; // 0 plain, 1 done, 2 unsure, 3 oops
            QList<AnswerToken> tokens;
    };
    ParsedAnswer parseAnswer(const QString& raw) {
        ParsedAnswer out;
        QString s = raw;
        while (!s.isEmpty() && s.at(0).isSpace()) s.remove(0, 1);
        if (s.startsWith('{')) {
            const int close = int(s.indexOf('}'));
            if (close < 0) return out;
            const QString tag = s.mid(1, close - 1).trimmed().toLower();
            out.tone = tag == "done" ? 1 : tag == "unsure" ? 2 : tag == "oops" ? 3 : 0;
            s = s.mid(close + 1);
        }
        bool key = false, glued = false;
        int closedAt = -2; // where the last ** or ]] ended: a word right after it has no space before it
        QString word;
        auto flush = [&] {
            if (word.isEmpty()) return;
            const bool lineStart = out.tokens.isEmpty() || out.tokens.last().kind == 4;
            if (lineStart && (word == "-" || word == "*")) word = QStringLiteral("•");
            out.tokens.append({word, key ? 1 : 0, {}, glued});
            word.clear();
        };
        for (int k = 0; k < s.size(); k++) {
            const QChar c = s.at(k);
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
        QVariantAnimation* leave;

        double scroll = 0, scrollVelocity = 0, scrollTarget = 0;
        double contentHeight = 0, viewHeight = 0;
        bool stickToEnd = true;
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
        else if (id == "pebble-dim" && AssistantTools::instance()->has(QStringLiteral("set_brightness"))) AssistantTools::instance()->tool(QStringLiteral("set_brightness")).run(QJsonObject{{"percent", 40}});
    });
    // a nudge at low battery: Pebble offers to dim the screen
    QDBusConnection::systemBus().connect(QStringLiteral("org.freedesktop.UPower"), QStringLiteral("/org/freedesktop/UPower/devices/DisplayDevice"),
        QStringLiteral("org.freedesktop.DBus.Properties"), QStringLiteral("PropertiesChanged"), instance(), SLOT(batteryChanged()));
    PebbleSelectionButton::setUp();
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
    connect(d->seesShown, &M3SpringAnimation::valueChanged, this, [this] {
        animateFor(100);
    });

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
    });

    d->chunk = new AssistantChunk();
    connect(d->chunk, &Chunk::clicked, this, &AssistantPanel::open);

    // TD_ASSISTANT_DEMO=1: the scripted stand-in (for tests; it changes nothing)
    if (qEnvironmentVariableIsSet("TD_ASSISTANT_DEMO")) d->brain = new DemoBrain(this);
    else d->brain = new ClaudeBrain(this);
    connect(d->brain, &AssistantBrain::stepStarted, this, [this](QString id, QString text) {
        // steps after some of the answer start a new part of the thread
        if (d->turnAnswer >= 0) {
            d->turnAnswer = -1;
            d->turnSteps = -1;
        }
        if (d->turnSteps < 0) {
            d->items.append({Private::Item::Steps, d->now()});
            d->turnSteps = int(d->items.size()) - 1;
        }
        d->items[d->turnSteps].steps.append({id, text});
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
        const int words = int(parseAnswer(answer.text).tokens.size());
        while (answer.wordBorn.size() < words) {
            const qint64 last = answer.wordBorn.isEmpty() ? 0 : answer.wordBorn.last();
            // a steady beat, quicker when words pile up so the end doesn't lag
            answer.wordBorn.append(qMax(d->now(), last + (last > d->now() + 250 ? 22 : 65)));
        }
        scrollToEnd();
        animateFor(1500);
    });
    connect(d->brain, &AssistantBrain::cardAdded, this, [this](AssistantCard card) {
        qint64 born = d->now();
        if (!d->items.isEmpty() && d->items.last().kind == Private::Item::Card) born = qMax(born, d->items.last().born + 100);
        Private::Item item{Private::Item::Card, born};
        item.card = card;
        d->items.append(item);
        d->turnAnswer = -1; // more words go under the card
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
        d->seesShown->animateTo(1.0);
        if (d->turnAnswer >= 0) {
            static const QRegularExpression keyNumber(QStringLiteral("\\*\\*(\\d+)(%?)\\*\\*"));
            auto it = keyNumber.globalMatch(d->items[d->turnAnswer].text);
            while (it.hasNext()) {
                const QRegularExpressionMatch m = it.next();
                d->lastNumbers.insert(m.captured(2), m.captured(1).toDouble());
            }
        }
        if (d->inBar) {
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
    const QRect screen = this->rect();
    const double top = qMax(56.0, screen.height() * 0.094);
    const double height = qMin(640.0, screen.height() - top - 48);
    return QRectF((screen.width() - SheetWidth) / 2.0, top, SheetWidth, height);
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
    if (!Compositor::isActive()) this->setMask(sheetRect().toAlignedRect());
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
    else if (!fromBar) greet();
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
    else QDialog::hide();
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
    });
}

void AssistantPanel::send() {
    sendText(d->field->text());
}

void AssistantPanel::sendText(QString text) {
    text = text.trimmed();
    if (text.isEmpty() || d->busy) return;
    d->field->clear();
    d->field->setPlaceholderText(tr("Ask %1 more").arg(Name));
    QStringList context;
    bool screen = false;
    for (const Private::Sees& seen : std::as_const(d->sees)) {
        if (seen.dismissed) continue;
        if (seen.context.isEmpty()) screen = true; // the screen goes as a picture, when it asks
        else context.append(seen.context);
    }
    d->brain->setScreen(screen ? d->screen : QImage());
    if (!d->items.isEmpty()) d->items.last().greeting = false;
    d->items.append({Private::Item::User, d->now(), text});
    d->turnSteps = d->turnAnswer = d->turnChips = -1;
    d->busy = true;
    d->busySince = d->now();
    d->busyText.clear();
    d->lastDone.clear();
    d->seesShown->animateTo(0.0);
    scrollToEnd();
    d->brain->send(text, context, d->harder);
    d->harder = false;
}

void AssistantPanel::animateFor(int ms) {
    d->animateUntil = qMax(d->animateUntil, d->now() + ms);
    if (!d->frame.isActive()) d->frame.start();
    this->update();
}

void AssistantPanel::scrollToEnd() {
    d->stickToEnd = true;
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
    const bool settled = std::abs(d->scroll - d->scrollTarget) < 0.5 && std::abs(d->scrollVelocity) < 1;
    if (settled) d->scroll = d->scrollTarget;
    this->update();
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

    // Where the sheet is: opening, going to the bar or coming back from it
    const QRectF base = sheetRect();
    QRectF bar(this->width() / 2.0 - 110, 14, 220, 34);
    if (d->chunk->isVisible() && d->chunk->width() > 0) bar = QRectF(QPointF(d->chunk->mapToGlobal(QPoint(0, 0)) - this->mapToGlobal(QPoint(0, 0))), QSizeF(d->chunk->size()));
    double toward = 0; // 0 = the sheet, 1 = the chip in the bar
    if (d->toBarAt >= 0) toward = springAt(M3Motion::SpatialDefault, d->since(d->toBarAt));
    else if (d->fromBarAt >= 0) toward = 1 - springAt(M3Motion::SpatialDefault, d->since(d->fromBarAt));
    const QRectF sheet = lerpRect(base, bar, toward);
    const double presence = (1 - c01(toward)) * (d->openedAt >= 0 ? c01(d->since(d->openedAt) / 0.2) : 1);

    if (Compositor::isActive()) {
        p.setCompositionMode(QPainter::CompositionMode_Source);
        p.fillRect(this->rect(), alpha(s.scrim, ScrimOpacity * presence));
        p.setCompositionMode(QPainter::CompositionMode_SourceOver);
    }

    p.save();
    if (d->openedAt >= 0 && toward == 0) {
        const double k = springAt(M3Motion::SpatialDefault, d->since(d->openedAt));
        p.translate(base.center().x(), base.top());
        p.scale(1, lerp(0.85, 1, k));
        p.translate(-base.center().x(), -base.top());
        p.setOpacity(c01(d->since(d->openedAt) / 0.2));
    }
    const double radius = lerp(Radius, sheet.height() / 2, c01(toward));
    p.setPen(Qt::NoPen);
    p.setBrush(toward > 0.85 ? s.surfaceContainerHigh : s.surfaceContainer);
    p.drawRoundedRect(sheet, radius, radius);
    double content = 1 - c01(toward * 4);
    if (d->introToPanelAt >= 0) content *= c01((d->since(d->introToPanelAt) - 0.45) / 0.25); // after the tour, the sheet's insides fade in
    if (content <= 0) {
        p.restore();
        return;
    }
    p.setOpacity(p.opacity() * content);
    p.setClipRect(sheet);

    // The field: the face, the words (a real line edit), send / stop
    const QRectF field(sheet.left() + Edge, sheet.top() + Edge, sheet.width() - 2 * Edge, FieldHeight);
    p.setBrush(s.surfaceContainerHighest);
    p.drawRoundedRect(field, FieldHeight / 2, FieldHeight / 2);
    const QRectF face(field.left() + 13, field.center().y() - 19, 38, 38);
    // thinking harder: the face turns tertiary
    const auto [faceInk, faceFill] = faceColours(s, d->deep);
    if (d->busy) M3LoadingIndicator::paint(&p, face, d->now() - d->busySince, faceInk, faceFill);
    else M3LoadingIndicator::paintDeterminate(&p, face, float(lerp(0.1, 1, c01(d->field->text().size() / 40.0))), faceInk, faceFill);
    if (toward == 0) d->field->setGeometry(QRectF(field.left() + 66, field.center().y() - 20, field.width() - 66 - 64, 40).toRect());
    const QPointF button(field.right() - 30, field.center().y());
    const QRectF buttonRect(button.x() - 24, button.y() - 24, 48, 48);
    if (d->busy) {
        p.setBrush(buttonRect.contains(d->mouse) ? mix(s.secondaryContainer, s.onSecondaryContainer, 0.08) : s.secondaryContainer);
        p.drawEllipse(button, 24, 24);
        icon(p, buttonRect, QStringLiteral("stop"), s.onSecondaryContainer, 22, true);
        d->hits.append({buttonRect, [this](QPointF) { d->brain->stop(); }});
    } else if (!d->field->text().trimmed().isEmpty()) {
        p.setBrush(buttonRect.contains(d->mouse) ? mix(s.primary, s.onPrimary, 0.08) : s.primary);
        p.drawEllipse(button, 24, 24);
        icon(p, buttonRect, QStringLiteral("arrow_upward"), s.onPrimary, 22);
        d->hits.append({buttonRect, [this](QPointF) { send(); }});
    }

    // What it sees: chips that can be dismissed, hidden while it works
    const double seesK = c01(d->seesShown->currentValue().toDouble());
    double top = field.bottom() + 12;
    const QList<Private::Sees> visibleSees = [this] {
        QList<Private::Sees> list;
        for (const Private::Sees& seen : std::as_const(d->sees)) {
            if (!seen.dismissed || d->since(seen.dismissedAt) < 0.4) list.append(seen);
        }
        return list;
    }();
    if (seesK > 0.01) {
        p.save();
        p.setOpacity(p.opacity() * seesK);
        const double left = sheet.left() + Side;
        // Think harder, at the end of the row: this message gets the deeper mode
        const QString harderLabel = tr("Think harder");
        const double harderWidth = 24 + 26 + textWidth(harderLabel, flex(13, 600));
        const QRectF harder(sheet.right() - Side - harderWidth, top, harderWidth, 32);
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
        if (!visibleSees.isEmpty()) text(p, QRectF(left, top, 50, 32), tr("Sees"), flex(13, 600), s.onSurfaceVariant);
        double x = left + 46;
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
            const QRectF x2(r.left() + full - 28, r.top(), 24, 32);
            icon(p, x2, QStringLiteral("close"), s.onSurfaceVariant, 16);
            p.restore();
            if (!seen.dismissed) {
                d->hits.append({r, [this, i](QPointF) {
                    d->sees[i].dismissed = true;
                    d->sees[i].dismissedAt = d->now();
                    animateFor(600);
                }});
            }
            x += (full + 8) * k;
        }
        p.restore();
        top += 44 * seesK;
    }

    // The thread
    const QRectF view(sheet.left() + Side - 8, top + 8, sheet.width() - 2 * Side + 16, sheet.bottom() - top - 8 - 16);
    d->viewHeight = view.height();
    p.save();
    p.setClipRect(view.adjusted(-4, 0, 4, 0), Qt::IntersectClip);
    d->contentHeight = paintThread(p, view, d->scroll);
    p.restore();
    if (d->items.isEmpty() && d->introToPanelAt < 0) {
        text(p, view.adjusted(0, 20, 0, 0), tr("Ask me to change a setting, explain what you copied, or find a file."), flex(15, 450), s.onSurfaceVariant, Qt::AlignHCenter | Qt::AlignTop);
    }
    p.restore();
    paintFocus(p);
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

double AssistantPanel::paintThread(QPainter& p, const QRectF& view, double scroll) {
    const M3::Scheme s = M3::currentScheme();
    p.translate(view.left(), view.top() - scroll);
    const double W = view.width();
    const double inset = 8; // text lines up with the field's text; cards reach the edge
    double y = 4;
    auto hit = [&](const QRectF& r, std::function<void(QPointF)> act, bool drags = false) {
        const QRectF mapped = p.transform().mapRect(r);
        if (mapped.intersects(view)) d->hits.append({mapped.intersected(view), act, drags});
    };
    auto hovered = [&](const QRectF& r) {
        return p.transform().mapRect(r).contains(d->mouse);
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

    for (int index = 0; index < d->items.size(); index++) {
        Private::Item& item = d->items[index];
        const double age = (d->now() - item.born) / 1000.0;
        if (age < 0) continue;
        switch (item.kind) {
            case Private::Item::User: {
                const QFont font = flex(15, 520);
                const double maxText = W * 0.75 - 36;
                const QRectF bounds = QFontMetricsF(font).boundingRect(QRectF(0, 0, maxText, 1000), Qt::TextWordWrap, item.text);
                const QRectF bubble(W - bounds.width() - 36, y, bounds.width() + 36, qMax(44.0, bounds.height() + 22));
                p.save();
                rise(p, bubble, age);
                p.setPen(Qt::NoPen);
                p.setBrush(s.primaryContainer);
                p.drawRoundedRect(bubble, 22, 22);
                text(p, bubble.adjusted(18, 0, -18, 0), item.text, font, s.onPrimaryContainer, Qt::AlignVCenter | Qt::AlignLeft | Qt::TextWordWrap);
                p.restore();
                y += bubble.height() + 20;
                break;
            }
            case Private::Item::Steps: {
                // Working: a wave and what it's doing; done: a chip with a check.
                // Once the answer lands they fold away; steps without a card
                // leave one "N steps" chip that opens them again
                int plain = 0;
                for (const Private::Step& step : std::as_const(item.steps)) plain += step.card ? 0 : 1;
                const double fold = item.unfolded || item.foldAt < 0 ? 0 : springAt(M3Motion::SpatialDefault, d->since(item.foldAt));
                double x = inset, rowY = 0, fullHeight = 36;
                QList<QPointF> places;
                for (const Private::Step& step : std::as_const(item.steps)) {
                    const double w = step.done ? chipWidth(QStringLiteral("check"), step.text) : 68 + textWidth(step.text, flex(14, 500));
                    if (x + w > W && x > inset) {
                        x = inset;
                        rowY += 44;
                        fullHeight += 44;
                    }
                    places.append(QPointF(x, rowY));
                    x += w + 8;
                }
                const double folded = plain > 0 ? 36 : 0;
                const double height = lerp(fullHeight, folded, c01(fold));
                p.save();
                p.setOpacity(p.opacity() * (1 - c01(fold * 2)));
                for (int i = 0; i < item.steps.size(); i++) {
                    const Private::Step& step = item.steps.at(i);
                    const QPointF at(places.at(i).x(), y + places.at(i).y());
                    if (!step.done) {
                        // M3E wavy progress: amplitude 3, wavelength 40, stroke 4
                        QPainterPath wave;
                        const double phase = d->now() / 1000.0 * 6;
                        for (double wx = 0; wx <= 56; wx += 2) {
                            const QPointF pt(at.x() + wx, at.y() + 18 + 3 * std::sin(wx / 40.0 * 2 * M_PI - phase));
                            if (wx == 0) wave.moveTo(pt);
                            else wave.lineTo(pt);
                        }
                        p.setBrush(Qt::NoBrush);
                        p.setPen(QPen(s.primary, 4, Qt::SolidLine, Qt::RoundCap));
                        p.drawPath(wave);
                        p.setPen(Qt::NoPen);
                        text(p, QRectF(at.x() + 68, at.y(), W, 36), step.text, flex(14, 500), s.onSurfaceVariant);
                    } else {
                        const QRectF r(at, QSizeF(chipWidth(QStringLiteral("check"), step.text), 36));
                        p.setPen(Qt::NoPen);
                        p.setBrush(s.secondaryContainer);
                        p.drawRoundedRect(r, 10, 10);
                        icon(p, QRectF(r.left() + 10, r.top(), 20, 36), QStringLiteral("check"), s.onSecondaryContainer, 18);
                        text(p, r.adjusted(36, 0, 0, 0), step.text, flex(14, 560), s.onSecondaryContainer);
                    }
                }
                if (item.unfolded) {
                    hit(QRectF(0, y, W, fullHeight), [this, index](QPointF) {
                        d->items[index].unfolded = false;
                        d->items[index].foldAt = d->now();
                        animateFor(800);
                    });
                }
                p.restore();
                if (plain > 0 && fold > 0.5) {
                    const QString label = item.steps.size() == 1 ? tr("1 step") : tr("%1 steps").arg(item.steps.size());
                    const QRectF r(inset, y, chipWidth(QStringLiteral("checklist"), label) + 22, 36);
                    p.save();
                    p.setOpacity(p.opacity() * c01((fold - 0.5) * 2));
                    p.setPen(QPen(s.outlineVariant, 1));
                    p.setBrush(hovered(r) ? mix(s.surfaceContainer, s.onSurface, 0.08) : s.surfaceContainer);
                    p.drawRoundedRect(r.adjusted(0.5, 0.5, -0.5, -0.5), 10, 10);
                    icon(p, QRectF(r.left() + 10, r.top(), 20, 36), QStringLiteral("checklist"), s.onSurfaceVariant, 18);
                    text(p, r.adjusted(36, 0, 0, 0), label, flex(14, 560), s.onSurface);
                    icon(p, QRectF(r.right() - 26, r.top(), 20, 36), QStringLiteral("expand_more"), s.onSurfaceVariant, 18);
                    p.restore();
                    hit(r, [this, index](QPointF) {
                        d->items[index].unfolded = true;
                        animateFor(400);
                    });
                }
                y += height + (height > 0.5 ? 16 : 0);
                break;
            }
            case Private::Item::Answer: {
                y += paintLiving(p, item.text, item.wordBorn, QPointF(inset, y), W - 2 * inset, 19, hit, hovered, item.countFrom) + 14;
                break;
            }
            case Private::Item::Card: {
                const AssistantCard& card = item.card;
                if (card.kind == AssistantCard::Ask) {
                    // A permission ask: what it wants to do, Allow / Don't.
                    // Once answered it shrinks to one line
                    const int shownLines = int(qMin<qsizetype>(card.lines.size(), 8));
                    const double full = 64 + shownLines * 21 + 12 + 40 + 16;
                    const double settle = item.changedAt >= 0 ? c01(springAt(M3Motion::SpatialDefault, d->since(item.changedAt))) : 0;
                    const double height = lerp(full, 52, settle);
                    const QRectF r(0, y, W, height);
                    p.save();
                    rise(p, r, age);
                    p.setPen(Qt::NoPen);
                    p.setBrush(card.value == 0 ? s.surfaceContainerHigh : card.value > 0 ? s.secondaryContainer : s.surfaceContainerHigh);
                    p.drawRoundedRect(r, 24, 24);
                    p.setClipRect(r, Qt::IntersectClip);
                    const QPointF disc(r.left() + 30, r.top() + 26);
                    const QString ic = card.value > 0 ? QStringLiteral("check") : card.value < 0 ? QStringLiteral("block") : QStringLiteral("shield_question");
                    p.setBrush(card.value > 0 ? s.primary : s.tertiaryContainer);
                    p.drawEllipse(disc, 16, 16);
                    icon(p, QRectF(disc.x() - 16, disc.y() - 16, 32, 32), ic, card.value > 0 ? s.onPrimary : s.onTertiaryContainer, 18, true);
                    const QString title = card.value > 0 ? tr("Allowed · %1").arg(card.title.chopped(1)) : card.value < 0 ? tr("Not allowed · %1").arg(card.title.chopped(1)) : card.title;
                    text(p, QRectF(r.left() + 56, r.top() + 8, W - 80, 36), title, flex(15, 600), card.value > 0 ? s.onSecondaryContainer : s.onSurface);
                    if (settle < 1) {
                        p.save();
                        p.setOpacity(p.opacity() * (1 - settle));
                        if (!card.subtitle.isEmpty()) text(p, QRectF(r.left() + 56, r.top() + 34, W - 80, 20), card.subtitle, flex(12, 450), s.onSurfaceVariant);
                        const QRectF code(r.left() + 12, r.top() + 60, W - 24, shownLines * 21 + 8);
                        p.setPen(Qt::NoPen);
                        p.setBrush(s.surfaceContainerLowest);
                        p.drawRoundedRect(code, 14, 14);
                        for (int i = 0; i < shownLines; i++) {
                            const QString& line = card.lines.at(i);
                            const QChar mark = line.isEmpty() ? ' ' : line.at(0);
                            const bool diff = mark == '+' || mark == '-';
                            text(p, QRectF(code.left() + 12, code.top() + 4 + i * 21, code.width() - 24, 21), line, mono(13, 450), diff ? (mark == '+' ? s.primary : s.error) : s.onSurface);
                        }
                        if (card.value == 0) {
                            const QRectF allow(r.right() - 116, r.bottom() - 52, 100, 40), deny(r.right() - 228, r.bottom() - 52, 100, 40);
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
                    }
                    p.restore();
                    y += height + 12;
                } else if (card.kind == AssistantCard::Level || card.kind == AssistantCard::Switch) {
                    // A segmented list: neighbours share the group's corners
                    auto isRow = [this](int i) {
                        return i >= 0 && i < d->items.size() && d->items.at(i).kind == Private::Item::Card && (d->items.at(i).card.kind == AssistantCard::Level || d->items.at(i).card.kind == AssistantCard::Switch);
                    };
                    const bool first = !isRow(index - 1), last = !isRow(index + 1);
                    const QRectF r(0, y, W, 68);
                    p.save();
                    rise(p, r, age);
                    const double flash = item.changedAt >= 0 ? qMax(0.0, 1 - d->since(item.changedAt) / 0.9) : 0;
                    QPainterPath shape;
                    shape.addRoundedRect(r, 6, 6);
                    if (first) {
                        QPainterPath topRound;
                        topRound.addRoundedRect(r.adjusted(0, 0, 0, -20), 22, 22);
                        shape = shape.united(topRound);
                    }
                    if (last) {
                        QPainterPath bottomRound;
                        bottomRound.addRoundedRect(r.adjusted(0, 20, 0, 0), 22, 22);
                        shape = shape.united(bottomRound);
                    }
                    p.setPen(Qt::NoPen);
                    p.setBrush(mix(s.surfaceContainerHigh, s.secondaryContainer, flash));
                    p.drawPath(shape);
                    const bool brightness = card.kind == AssistantCard::Level;
                    const QPointF disc(r.left() + 34, r.center().y());
                    p.setBrush(brightness ? s.secondaryContainer : s.tertiaryContainer);
                    p.drawEllipse(disc, 20, 20);
                    icon(p, QRectF(disc.x() - 20, disc.y() - 20, 40, 40), card.icon.isEmpty() ? (brightness ? QStringLiteral("tune") : QStringLiteral("toggle_on")) : card.icon, brightness ? s.onSecondaryContainer : s.onTertiaryContainer, 22, true);
                    if (brightness) {
                        text(p, QRectF(r.left() + 66, r.top(), 120, r.height()), card.title, flex(15, 600), s.onSurface);
                        // the M3E slider: a thin handle with gaps, a stop dot at the end
                        const QRectF track(r.left() + 196, r.center().y() - 20, r.width() - 196 - 84, 40);
                        const double v = c01(card.value), hx = track.left() + track.width() * v, cy = track.center().y();
                        p.setPen(Qt::NoPen);
                        p.setBrush(s.primary);
                        p.drawRoundedRect(QRectF(track.left(), cy - 8, qMax(0.0, hx - 6 - track.left()), 16), 8, 8);
                        p.setBrush(s.secondaryContainer);
                        p.drawRoundedRect(QRectF(hx + 6, cy - 8, qMax(0.0, track.right() - hx - 6), 16), 8, 8);
                        p.setBrush(s.onSecondaryContainer);
                        p.drawEllipse(QPointF(track.right() - 8, cy), 2, 2);
                        p.setBrush(s.primary);
                        p.drawRoundedRect(QRectF(hx - 2, cy - 20, 4, 40), 2, 2);
                        text(p, QRectF(r.right() - 72, r.top(), 54, r.height()), QStringLiteral("%1%").arg(qRound(v * 100)), flex(15, 650), s.onSurface, Qt::AlignRight | Qt::AlignVCenter);
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
                        text(p, QRectF(r.left() + 66, r.top() + 12, 300, 24), card.title, flex(15, 600), s.onSurface);
                        // the time rolls over to the new one
                        const QRectF sub(r.left() + 66, r.top() + 34, r.width() - 160, 20);
                        const double roll = item.changedAt >= 0 ? springAt(M3Motion::SpatialDefault, d->since(item.changedAt)) : 1;
                        p.save();
                        p.setClipRect(sub, Qt::IntersectClip);
                        if (roll < 1) text(p, sub.translated(0, -20 * roll), item.was, flex(13, 450), s.onSurfaceVariant);
                        text(p, sub.translated(0, 20 * (1 - roll)), card.subtitle, flex(13, 450), s.onSurfaceVariant);
                        p.restore();
                        // the switch
                        const QRectF track(r.right() - 72, r.center().y() - 16, 52, 32);
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
                    p.restore();
                    y += 68 + (last ? 14 : 2);
                } else if (card.kind == AssistantCard::Image) {
                    // A picture: a file, or fetched from the web
                    const QString source = card.path;
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
                        } else {
                            d->images.insert(source, QImage(source));
                        }
                    }
                    const QImage image = d->images.value(source);
                    const bool loaded = !image.isNull() && image.width() > 1;
                    QSizeF size(W, 180);
                    if (loaded) {
                        size = QSizeF(image.size()).scaled(QSizeF(W, 300), Qt::KeepAspectRatio);
                        if (size.width() > image.width()) size = image.size();
                    }
                    const double captionHeight = card.title.isEmpty() ? 0 : 24;
                    const QRectF r(0, y, size.width(), size.height());
                    p.save();
                    rise(p, r, age);
                    QPainterPath shape;
                    shape.addRoundedRect(r, 20, 20);
                    p.setPen(Qt::NoPen);
                    if (loaded) {
                        p.setClipPath(shape, Qt::IntersectClip);
                        p.setRenderHint(QPainter::SmoothPixmapTransform);
                        p.drawImage(r, image);
                    } else {
                        p.setBrush(s.surfaceContainerHigh);
                        p.drawPath(shape);
                        icon(p, r, QStringLiteral("image"), s.onSurfaceVariant, 32);
                    }
                    p.restore();
                    if (!card.title.isEmpty()) text(p, QRectF(4, y + size.height() + 4, W, 20), card.title, flex(13, 450), s.onSurfaceVariant);
                    hit(r, [source](QPointF) {
                        QDesktopServices::openUrl(source.startsWith(QStringLiteral("http")) ? QUrl(source) : QUrl::fromLocalFile(source));
                    });
                    y += size.height() + captionHeight + 12;
                } else if (card.kind == AssistantCard::Code) {
                    const QRectF r(0, y, W, 48 + card.lines.size() * 23 + 12);
                    p.save();
                    rise(p, r, age);
                    p.setPen(Qt::NoPen);
                    p.setBrush(s.surfaceContainerLowest);
                    p.drawRoundedRect(r, 20, 20);
                    const bool isFile = card.path.startsWith('/') || card.path.startsWith('~');
                    text(p, QRectF(r.left() + 18, r.top() + 8, 300, 32), isFile ? QFileInfo(card.path).fileName() : (card.path.isEmpty() ? tr("code") : card.path), mono(13, 600), s.onSurfaceVariant);
                    const QString path = card.path;
                    const QStringList lines = card.lines;
                    button(QRectF(r.right() - (isFile ? 252 : 108), r.top() + 8, 96, 32), QStringLiteral("content_copy"), tr("Copy"), false, [lines](QPointF) {
                        // the code as it is now: without the removed lines or the markers
                        QStringList now;
                        for (const QString& line : lines) {
                            if (!line.startsWith('-')) now.append(line.mid(1));
                        }
                        QGuiApplication::clipboard()->setText(now.join('\n'));
                    });
                    if (isFile) {
                        button(QRectF(r.right() - 148, r.top() + 8, 136, 32), QStringLiteral("open_in_new"), tr("Open in Zed"), true, [path](QPointF) {
                            QProcess::startDetached(QStringLiteral("zeditor"), {QString(path).replace(QRegularExpression(QStringLiteral("^~")), QDir::homePath())});
                        });
                    }
                    for (int i = 0; i < card.lines.size(); i++) {
                        const QString& line = card.lines.at(i);
                        const QRectF lr(r.left() + 8, r.top() + 48 + i * 23, r.width() - 16, 23);
                        const QChar k = line.isEmpty() ? ' ' : line.at(0);
                        if (k == '+' || k == '-') {
                            p.setPen(Qt::NoPen);
                            p.setBrush(alpha(k == '+' ? s.primaryContainer : s.errorContainer, 0.3));
                            p.drawRoundedRect(lr, 6, 6);
                        }
                        text(p, QRectF(lr.left() + 10, lr.top(), 20, lr.height()), k == ' ' ? QString() : QString(k), mono(14, 600), k == '+' ? s.primary : s.error);
                        text(p, QRectF(lr.left() + 30, lr.top(), lr.width() - 40, lr.height()), line.mid(1), mono(14, 450), s.onSurface);
                    }
                    p.restore();
                    y += r.height() + 10;
                } else {
                    const QRectF r(0, y, W, 60);
                    p.save();
                    rise(p, r, age);
                    p.setPen(Qt::NoPen);
                    p.setBrush(s.surfaceContainerHigh);
                    p.drawRoundedRect(r, 20, 20);
                    const QString suffix = QFileInfo(card.path).suffix().toLower();
                    const QString ic = suffix == "css" ? QStringLiteral("css") : suffix == "html" ? QStringLiteral("html") : suffix == "js" ? QStringLiteral("javascript") : QStringLiteral("description");
                    const QPointF disc(r.left() + 32, r.center().y());
                    p.setBrush(s.tertiaryContainer);
                    p.drawEllipse(disc, 20, 20);
                    icon(p, QRectF(disc.x() - 20, disc.y() - 20, 40, 40), ic, s.onTertiaryContainer, 22);
                    text(p, QRectF(r.left() + 64, r.top() + 9, r.width() - 300, 22), card.title, flex(15, 600), s.onSurface);
                    text(p, QRectF(r.left() + 64, r.top() + 30, r.width() - 300, 20), card.subtitle, flex(12, 450), s.onSurfaceVariant);
                    const QString path = QString(card.path).replace(QRegularExpression(QStringLiteral("^~")), QDir::homePath());
                    button(QRectF(r.right() - 230, r.top() + 12, 80, 36), {}, tr("Open"), false, [path](QPointF) {
                        QDesktopServices::openUrl(QUrl::fromLocalFile(path));
                    });
                    button(QRectF(r.right() - 142, r.top() + 12, 130, 36), {}, tr("Show in folder"), false, [path](QPointF) {
                        QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).absolutePath()));
                    });
                    p.restore();
                    y += 70;
                }
                break;
            }
            case Private::Item::Chips: {
                double x = inset;
                for (const AssistantChip& chip : std::as_const(item.chips)) {
                    const double w = chipWidth(chip.icon, chip.label);
                    if (x + w > W && x > inset) {
                        x = inset;
                        y += 44;
                    }
                    const QRectF r(x, y, w, 36);
                    p.save();
                    rise(p, r, age);
                    const bool pressed = item.pressed == chip.id && d->since(item.pressedAt) < 0.25;
                    p.setPen(QPen(s.outlineVariant, 1));
                    p.setBrush(pressed ? mix(s.surfaceContainer, s.onSurface, 0.12) : hovered(r) ? mix(s.surfaceContainer, s.onSurface, 0.08) : s.surfaceContainer);
                    p.drawRoundedRect(r.adjusted(0.5, 0.5, -0.5, -0.5), 10, 10);
                    double tx = r.left() + 12;
                    if (!chip.icon.isEmpty()) {
                        icon(p, QRectF(tx, r.top(), 20, 36), chip.icon, s.onSurface, 18);
                        tx += 26;
                    }
                    text(p, QRectF(tx, r.top(), r.right() - tx - 10, 36), chip.label, flex(14, 560), s.onSurface);
                    p.restore();
                    const QString id = chip.id, label = chip.label;
                    hit(r, [this, index, id, label](QPointF) {
                        if (d->busy) return;
                        d->items[index].pressed = id;
                        d->items[index].pressedAt = d->now();
                        if (!d->brain->chip(id)) sendText(label);
                        animateFor(400);
                    });
                    x += w + 8;
                }
                y += 36 + 16;
                break;
            }
        }
    }
    return y;
}

double AssistantPanel::paintLiving(QPainter& p, const QString& raw, const QList<qint64>& wordBorn, QPointF at, double W, double px, const HitFn& hit, const HoverFn& hovered, const QHash<QString, double>& countFrom) {
    // Living type: words arrive thin and narrow and spring to their weight.
    // Key words land heavy, wide and in primary (and stay); a key number
    // counts up; files, apps and links turn into chips; the tone shapes the
    // whole answer
    const M3::Scheme s = M3::currentScheme();
    const ParsedAnswer parsed = parseAnswer(raw);
    const QFontMetricsF metrics(flex(px, 480));
    const QFontMetricsF keyMetrics(flex(px, 700, 110));
    const double space = metrics.horizontalAdvance(' '), lineHeight = metrics.height() * 1.3;
    const double toneAt = wordBorn.isEmpty() ? -1 : (wordBorn.last() + 350);
    static const QRegularExpression number(QStringLiteral("^(\\d+)(%?)([.,!?:;]?)$"));
    double x = 0, line = 0;
    for (int t = 0; t < parsed.tokens.size(); t++) {
        const AnswerToken& token = parsed.tokens.at(t);
        if (token.kind == 4) {
            x = 0;
            line += 1;
            continue;
        }
        const double born = t < wordBorn.size() ? d->since(wordBorn.at(t)) : -1;
        const double chipK = token.kind >= 2 ? c01(springAt(M3Motion::SpatialDefault, born - 0.55)) : 0;
        double ww = (token.kind == 1 ? keyMetrics : metrics).horizontalAdvance(token.text) + 34 * chipK;
        if (token.glued && x > 0) x -= space;
        if (x > 0 && x + ww > W) {
            x = 0;
            line += 1;
        }
        const double k = born > 0 ? springAt(M3Motion::SpatialDefault, born) : 0;
        if (k > 0) {
            double wght = lerp(100, 480, k), wdth = lerp(25, 100, k), slnt = 0;
            if (parsed.tone == 1 && toneAt >= 0) wght += 80 * c01(springAt(M3Motion::SpatialDefault, d->since(qint64(toneAt)) - t * 0.02));
            if (parsed.tone == 2) {
                wght = lerp(100, 400, k);
                slnt = -9 * c01(k);
            }
            if (parsed.tone == 3 && toneAt >= 0 && d->since(qint64(toneAt)) > 0) {
                const double u = d->since(qint64(toneAt));
                wdth += 22 * std::sin(u * 20) * std::exp(-u * 4.5);
            }
            QColor ink = alpha(s.onSurface, k * 2.5);
            double scale = 1;
            QString shown = token.text;
            if (token.kind == 1) {
                wght = lerp(100, 700, k);
                wdth = lerp(25, 110, k);
                scale = lerp(1.35, 1.0, c01(k));
                ink = alpha(s.primary, k * 2.5);
                const QRegularExpressionMatch m = number.match(token.text);
                if (m.hasMatch()) shown = QString::number(qRound(lerp(countFrom.value(m.captured(2), 0), m.captured(1).toDouble(), c01(springAt(M3Motion::SpatialSlow, born))))) + m.captured(2) + m.captured(3);
            }
            QFont font = flex(px, wght, wdth);
            font.setFeature(QFont::Tag("tnum"), 1);
            font.setVariableAxis(QFont::Tag("slnt"), float(slnt));
            const double top = at.y() + line * lineHeight;
            const double baseline = top + metrics.ascent() + 10 * (1 - k);
            p.save();
            if (token.kind >= 2) {
                if (chipK > 0.01) {
                    const QRectF pill(at.x() + x - 4, top + 1, ww + 8, lineHeight - 4);
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
                p.drawText(QPointF(at.x() + x + 30 * chipK, baseline), shown);
            } else {
                const double cx = at.x() + x + ww / 2;
                p.setFont(font);
                p.setPen(ink);
                p.translate(cx, baseline - metrics.ascent() / 2.6);
                p.scale(scale, scale);
                p.drawText(QPointF(-QFontMetricsF(font).horizontalAdvance(shown) / 2, metrics.ascent() / 2.6), shown);
            }
            p.restore();
        }
        x += ww + space;
    }
    return (line + 1) * lineHeight;
}

void AssistantPanel::mousePressEvent(QMouseEvent* event) {
    const QPointF at = event->position();
    for (int i = int(d->hits.size()) - 1; i >= 0; i--) {
        if (!d->hits.at(i).rect.contains(at)) continue;
        if (d->hits.at(i).drags) d->dragging = i;
        d->hits.at(i).act(at);
        return;
    }
    // on the scrim: away it goes (into the bar while it works)
    if (!sheetRect().contains(at)) close();
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
    this->update();
}

void AssistantPanel::mouseReleaseEvent(QMouseEvent* event) {
    Q_UNUSED(event)
    d->dragging = -1;
}

void AssistantPanel::wheelEvent(QWheelEvent* event) {
    const double max = qMax(0.0, d->contentHeight - d->viewHeight);
    d->scrollTarget = qBound(0.0, d->scrollTarget - event->angleDelta().y() / 2.0, max);
    d->stickToEnd = d->scrollTarget >= max - 4;
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
            close();
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
    const QRectF box = lerpRect(card, sheetRect(), away);
    const double appear = springAt(M3Motion::SpatialDefault, d->since(d->openedAt >= 0 ? d->openedAt : 0));
    p.save();
    if (away == 0 && d->openedAt >= 0) {
        p.translate(box.center());
        p.scale(lerp(0.92, 1, appear), lerp(0.92, 1, appear));
        p.translate(-box.center());
    }
    p.setPen(Qt::NoPen);
    p.setBrush(s.surfaceContainer);
    p.drawRoundedRect(box, lerp(28, Radius, away), lerp(28, Radius, away));
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
        if ((event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter || event->key() == Qt::Key_Right) && d->introStep < 4) introNext();
        else if (event->key() == Qt::Key_Left || event->key() == Qt::Key_Backspace) introBack();
        else if (event->key() == Qt::Key_Escape) close(); // it shows again next time
        return;
    }
    // not QDialog's: its Escape rejects, past close() (and the bar while it works)
    if (event->key() == Qt::Key_Escape) close();
}
