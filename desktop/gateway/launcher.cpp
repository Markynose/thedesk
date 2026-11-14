/****************************************
 *
 *   theDesk - the Gateway: a launcher in the middle of the screen
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 * *************************************/
#include "launcher.h"

#include "bar/gatewaylogo.h"
#include "launcherdata.h"
#include <QApplication>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QDesktopServices>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QHash>
#include <QKeyEvent>
#include <QLineEdit>
#include <QLocale>
#include <QMenu>
#include <QMimeDatabase>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QProcess>
#include <QStandardPaths>
#include <QTimer>
#include <QToolTip>
#include <QUrl>
#include <Wm/desktopwm.h>
#include <cmath>
#include <m3motion.h>
#include <m3scheme.h>
#include <m3themedicon.h>
#include <m3usercard.h>
#include <powermanager.h>
#include <shapes/materialshapes.h>
#include <shapes/morph.h>
#include <shapes/shapepath.h>
#include <statemanager.h>
#include <statuscentermanager.h>
#include <statuscenterpane.h>
#include <tsettings.h>

namespace {
    constexpr int BarHeight = 64;     // the search bar
    constexpr int Gap = 12;           // between it and the panel
    constexpr int Pad = 20;           // inside the panel
    constexpr int PinCount = 7;
    constexpr int TileHeight = 96;    // a pinned app: its shape and its name
    constexpr int GridRow = 86;       // an app in the grid
    constexpr int RowHeight = 54;     // a file or a result
    constexpr int TopHeight = 112;    // the top hit
    constexpr int ToolbarHeight = 64;

    QFont flex(double px, int wght, int wdth = 100) {
        QFont font(QStringLiteral("Roboto Flex"));
        font.setPixelSize(qRound(px));
        font.setVariableAxis(QFont::Tag("wght"), float(wght));
        font.setVariableAxis(QFont::Tag("wdth"), float(wdth));
        return font;
    }

    QFont symbols(double px) {
        QFont font(QStringLiteral("Material Symbols Rounded"));
        font.setPixelSize(qRound(px));
        font.setVariableAxis(QFont::Tag("wght"), 400);
        font.setVariableAxis(QFont::Tag("opsz"), float(qBound(20.0, px, 48.0)));
        return font;
    }

    // A spring's step from rest to 1 (the entrance runs off one clock)
    double springAt(double t, double zeta, double stiffness) {
        if (t <= 0) return 0;
        const double w = std::sqrt(stiffness), wd = w * std::sqrt(1 - zeta * zeta);
        return 1 - std::exp(-zeta * w * t) * (std::cos(wd * t) + zeta * w / wd * std::sin(wd * t));
    }

    // A row in a group: the group's outer corners are large, the ones
    // between rows small (the Expressive list)
    QPainterPath segment(const QRectF& r, double top, double bottom) {
        QPainterPath path;
        path.moveTo(r.left() + top, r.top());
        path.lineTo(r.right() - top, r.top());
        path.arcTo(QRectF(r.right() - 2 * top, r.top(), 2 * top, 2 * top), 90, -90);
        path.lineTo(r.right(), r.bottom() - bottom);
        path.arcTo(QRectF(r.right() - 2 * bottom, r.bottom() - 2 * bottom, 2 * bottom, 2 * bottom), 0, -90);
        path.lineTo(r.left() + bottom, r.bottom());
        path.arcTo(QRectF(r.left(), r.bottom() - 2 * bottom, 2 * bottom, 2 * bottom), 270, -90);
        path.lineTo(r.left(), r.top() + top);
        path.arcTo(QRectF(r.left(), r.top(), 2 * top, 2 * top), 180, -90);
        path.closeSubpath();
        return path;
    }

    // Each pinned app sits in a shape of its own
    const m3shapes::RoundedPolygon& pinShape(int index) {
        static const char* names[] = {"Cookie9Sided", "Clover4Leaf", "Sunny", "Cookie6Sided", "Pentagon", "SoftBurst", "Cookie7Sided", "Clover8Leaf", "Cookie12Sided", "Gem"};
        static QList<const m3shapes::RoundedPolygon*> shapes = [] {
            QList<const m3shapes::RoundedPolygon*> list;
            for (const char* name : names) {
                for (const auto& named : MaterialShapes::all()) {
                    if (qstrcmp(named.name, name) == 0) list.append(&named.shape());
                }
            }
            if (list.isEmpty()) list.append(&MaterialShapes::cookie9Sided());
            return list;
        }();
        return *shapes.at(index % shapes.count());
    }

    // Under a command that isn't there, the way a spelling checker would
    void wavyUnderline(QPainter& p, double from, double to, double y, const QColor& colour) {
        QPainterPath path;
        for (double x = from; x <= to + 0.01; x += 1) {
            const QPointF point(x, y + 1.6 * std::sin((x - from) / 7.0 * 2 * M_PI));
            if (x == from) path.moveTo(point);
            else path.lineTo(point);
        }
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(colour, 1.6, Qt::SolidLine, Qt::RoundCap));
        p.drawPath(path);
        p.setPen(Qt::NoPen);
    }

    // How many letters apart two names are (for "did you mean")
    int distance(const QString& a, const QString& b) {
        QList<int> row(b.size() + 1);
        for (int j = 0; j <= b.size(); j++) row[j] = j;
        for (int i = 1; i <= a.size(); i++) {
            int diagonal = row[0];
            row[0] = i;
            for (int j = 1; j <= b.size(); j++) {
                const int above = row[j];
                row[j] = qMin(qMin(row[j] + 1, row[j - 1] + 1), diagonal + (a.at(i - 1) == b.at(j - 1) ? 0 : 1));
                diagonal = above;
            }
        }
        return row[b.size()];
    }

    void searchWeb(const QString& text) {
        tSettings settings;
        QString engine = settings.value(QStringLiteral("Gateway/webSearch")).toString();
        if (!engine.contains(QLatin1String("%1"))) engine = QStringLiteral("https://duckduckgo.com/?q=%1");
        QDesktopServices::openUrl(QUrl(engine.arg(QString::fromLatin1(QUrl::toPercentEncoding(text)))));
    }

    QString ago(const QDateTime& when) {
        if (!when.isValid()) return QString();
        const QDateTime now = QDateTime::currentDateTime();
        const qint64 minutes = when.secsTo(now) / 60;
        if (minutes < 1) return Launcher::tr("just now");
        if (minutes < 60) return Launcher::tr("%n min ago", nullptr, int(minutes));
        if (minutes < 24 * 60 && when.toLocalTime().date() == now.date()) return Launcher::tr("%n h ago", nullptr, int(minutes / 60));
        if (when.toLocalTime().date() == now.date().addDays(-1)) return Launcher::tr("yesterday");
        return QLocale().toString(when.toLocalTime().date(), QStringLiteral("d MMM"));
    }

    QString symbolFor(const QString& path) {
        static QMimeDatabase mimes;
        const QString mime = mimes.mimeTypeForFile(path, QMimeDatabase::MatchExtension).name();
        if (QFileInfo(path).isDir()) return QStringLiteral("folder");
        if (mime.startsWith(QLatin1String("image/"))) return QStringLiteral("image");
        if (mime.startsWith(QLatin1String("video/"))) return QStringLiteral("movie");
        if (mime.startsWith(QLatin1String("audio/"))) return QStringLiteral("music_note");
        if (mime == QLatin1String("application/pdf")) return QStringLiteral("picture_as_pdf");
        if (mime.contains(QLatin1String("zip")) || mime.contains(QLatin1String("tar")) || mime.contains(QLatin1String("compressed"))) return QStringLiteral("folder_zip");
        if (mime.startsWith(QLatin1String("text/"))) return QStringLiteral("description");
        return QStringLiteral("draft");
    }

    // "12 * (3 + 4)": sums typed into the search bar are worked out
    class Sum {
        public:
            explicit Sum(QString text) :
                text(std::move(text)) {}

            bool work(double* out) {
                bool digit = false, sign = false;
                for (const QChar c : std::as_const(text)) {
                    if (c.isDigit()) digit = true;
                    else if (QStringLiteral("+-*/^%x×÷").contains(c)) sign = true;
                    else if (!QStringLiteral(" .,()").contains(c)) return false;
                }
                if (!digit || !sign) return false;
                at = 0;
                ok = true;
                const double value = sum();
                skip();
                if (!ok || at != text.size() || !std::isfinite(value)) return false;
                *out = value;
                return true;
            }

        private:
            QString text;
            int at = 0;
            bool ok = true;

            void skip() {
                while (at < text.size() && text.at(at).isSpace()) at++;
            }
            bool take(const QString& any, QChar* which = nullptr) {
                skip();
                if (at < text.size() && any.contains(text.at(at))) {
                    if (which) *which = text.at(at);
                    at++;
                    return true;
                }
                return false;
            }
            double sum() {
                double value = product();
                QChar sign;
                while (take(QStringLiteral("+-"), &sign)) value = sign == QLatin1Char('+') ? value + product() : value - product();
                return value;
            }
            double product() {
                double value = power();
                QChar sign;
                while (take(QStringLiteral("*/%x×÷"), &sign)) {
                    const double other = power();
                    if (sign == QLatin1Char('*') || sign == QLatin1Char('x') || sign == QChar(0xD7)) value *= other;
                    else if (sign == QLatin1Char('%')) value = std::fmod(value, other);
                    else value /= other;
                }
                return value;
            }
            double power() {
                const double base = unary();
                if (take(QStringLiteral("^"))) return std::pow(base, power());
                return base;
            }
            double unary() {
                if (take(QStringLiteral("-"))) return -unary();
                return primary();
            }
            double primary() {
                if (take(QStringLiteral("("))) {
                    const double value = sum();
                    if (!take(QStringLiteral(")"))) ok = false;
                    return value;
                }
                skip();
                const int from = at;
                while (at < text.size() && (text.at(at).isDigit() || text.at(at) == QLatin1Char('.') || text.at(at) == QLatin1Char(','))) at++;
                if (at == from) {
                    ok = false;
                    return 0;
                }
                bool number = false;
                const double value = text.mid(from, at - from).replace(QLatin1Char(','), QLatin1Char('.')).toDouble(&number);
                if (!number) ok = false;
                return value;
            }
    };

    // The person, at the end of the search bar: their picture in a shape
    class Avatar : public QWidget {
        public:
            explicit Avatar(QWidget* parent) :
                QWidget(parent) {
                this->setAttribute(Qt::WA_TransparentForMouseEvents);
                const bool standIn = qEnvironmentVariableIsSet("TD_DISPLAY_NAME");
                name = standIn ? qEnvironmentVariable("TD_DISPLAY_NAME") : DesktopWm::userDisplayName();
                const QString path = M3UserCard::currentUserPicture();
                if (!path.isEmpty()) picture = QImage(path);
            }

        protected:
            void paintEvent(QPaintEvent*) override {
                const M3::Scheme s = M3::currentScheme();
                QPainter p(this);
                p.setRenderHint(QPainter::Antialiasing);
                p.setRenderHint(QPainter::SmoothPixmapTransform);
                const QRectF box = QRectF(this->rect()).adjusted(2, 2, -2, -2);
                const QPainterPath shape = m3shapes::toPath(MaterialShapes::clover4Leaf(), box);
                if (!picture.isNull()) {
                    p.setClipPath(shape);
                    const QImage scaled = picture.scaled(box.size().toSize() * this->devicePixelRatioF(), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
                    p.drawImage(box, scaled, QRectF((scaled.width() - box.width() * this->devicePixelRatioF()) / 2, (scaled.height() - box.height() * this->devicePixelRatioF()) / 2, box.width() * this->devicePixelRatioF(), box.height() * this->devicePixelRatioF()));
                    return;
                }
                p.fillPath(shape, s.primaryContainer);
                p.setFont(flex(17, 800, 110));
                p.setPen(s.onPrimaryContainer);
                p.drawText(box, Qt::AlignCenter, name.left(1).toUpper());
            }

        private:
            QString name;
            QImage picture;
    };

    // Over the search bar in Run mode: the "Run" chip after the mark, and
    // what Tab would add after the cursor
    class RunBar : public QWidget {
        public:
            explicit RunBar(QLineEdit* edit) :
                QWidget(edit), edit(edit) {
                this->setAttribute(Qt::WA_TransparentForMouseEvents);
            }
            double chip = 0; // 0 -> 1: the chip's width, on a spring
            QString ghost;

        protected:
            void paintEvent(QPaintEvent*) override {
                const M3::Scheme s = M3::currentScheme();
                QPainter p(this);
                p.setRenderHint(QPainter::Antialiasing);
                p.setRenderHint(QPainter::TextAntialiasing);
                if (chip > 0.01) {
                    const QRectF box(ChipLeft, this->height() / 2.0 - 16, ChipWidth * chip, 32);
                    p.setPen(Qt::NoPen);
                    p.setBrush(s.secondaryContainer);
                    p.drawRoundedRect(box, 8, 8);
                    p.setClipRect(box);
                    p.setFont(symbols(18));
                    p.setPen(s.onSecondaryContainer);
                    p.drawText(QRectF(box.left() + 8, box.top(), 20, 32), Qt::AlignCenter, QStringLiteral("chevron_right"));
                    p.setFont(flex(14, 560));
                    p.drawText(QRectF(box.left() + 30, box.top(), ChipWidth - 30, 32), Qt::AlignLeft | Qt::AlignVCenter, Launcher::tr("Run"));
                    p.setClipping(false);
                }
                if (!ghost.isEmpty() && edit->hasFocus() && edit->cursorPosition() == edit->text().size()) {
                    const QRect cursor = edit->inputMethodQuery(Qt::ImCursorRectangle).toRect();
                    p.setFont(edit->font());
                    p.setPen(s.outline);
                    p.drawText(QRectF(cursor.center().x() + 1, 0, this->width() - cursor.center().x() - 60, this->height()), Qt::AlignLeft | Qt::AlignVCenter, ghost);
                }
            }

        private:
            QLineEdit* edit;

        public:
            static constexpr double ChipLeft = 64, ChipWidth = 74;
    };

    enum class Mode {
        Home,    // pinned, recent files
        AllApps, // every app, by category
        Search,
        Run // a command: Alt+F2, or ">" typed
    };

    struct Action {
            QString label;
            std::function<void()> run;
            QRectF rect;
    };

    struct Item {
            enum Kind {
                Pin,    // a pinned app in its shape
                Grid,   // an app in the grid
                Row,    // a file, a result
                Top,    // the top hit
                Web,    // "search the web"
                Tool,   // a toolbar button
                Chip,   // a category
                Toggle  // "All apps" / "Pinned"
            };
            Kind kind = Row;
            QString id; // stays the same across layouts: its springs are kept by it
            QRectF rect;
            QString title, subtitle, symbol, group, tip;
            const LauncherApp* app = nullptr;
            QImage icon; // a settings page's own icon
            int index = 0;
            int prefix = 0;                   // the title's first letters, as typed: drawn lighter
            bool first = false, last = false; // of its group
            bool checked = false;
            bool destructive = false; // a toolbar button that ends the session; a command that isn't there
            double score = 0;
            std::function<void()> run;
            QList<Action> actions;
    };

    struct Motion {
            double hover = 0, hoverSpeed = 0;
            double press = 0, pressSpeed = 0;
    };
} // namespace

struct Launcher::Private {
        QLineEdit* edit;
        Avatar* avatar;
        GatewayLogo* logo;
        RunBar* runBar;
        M3SpringAnimation* runChip;
        QString failed;          // Run mode: the command that wouldn't start
        QStringList closest;     // and the names on PATH nearest to it
        QElapsedTimer failedAt;
        Mode mode = Mode::Home;
        QString category;

        QList<Item> items;
        QList<Item> results; // what a search found, best first
        QList<QPair<QPointF, QString>> headers;
        QRectF panel, toolbar, grid, hints;
        double scroll = 0, scrollMax = 0;

        int selected = -1, selectedAction = 0;
        int hovered = -1, pressed = -1, pressedAction = 0;
        QHash<QString, Motion> motion;
        QHash<QString, QImage> glyphs;
        QTimer frames;
        QElapsedTimer clock, openedAt, topSince;
        qint64 lastFrame = 0;
        QString topId;

        Motion& moving(const QString& id) {
            return motion[id];
        }
};

Launcher::Launcher(QWidget* parent) :
    QWidget(parent) {
    d = new Private();
    this->setMouseTracking(true);
    this->setFocusPolicy(Qt::StrongFocus);

    // The search bar is a real text field (the style draws it as an M3
    // search bar); the Gateway mark leads it, the person ends it
    d->edit = new QLineEdit(this);
    d->edit->setProperty("m3SearchBar", true);
    d->edit->setPlaceholderText(tr("Search apps, files and settings"));
    d->edit->setFont(flex(18, 420));
    d->edit->setClearButtonEnabled(false);
    d->logo = new GatewayLogo(d->edit, GatewayLogo::Placement::Leading, 36);
    d->edit->setTextMargins(36 + 14, 0, 52, 0);
    d->edit->installEventFilter(this);
    d->avatar = new Avatar(d->edit);
    d->runBar = new RunBar(d->edit);
    this->setFocusProxy(d->edit);
    connect(d->edit, &QLineEdit::textChanged, this, [this](const QString& text) {
        // ">" first is the way into Run mode from a search
        if (d->mode != Mode::Run && text.startsWith(QLatin1Char('>'))) {
            enterRun();
            d->edit->setText(text.mid(1));
            return;
        }
        const bool searching = !text.trimmed().isEmpty();
        if (d->mode == Mode::Run) d->failed.clear();
        else if (searching) d->mode = Mode::Search;
        else if (d->mode == Mode::Search) d->mode = Mode::Home;
        d->selected = searching ? 0 : -1;
        d->selectedAction = 0;
        this->relayout();
    });
    connect(d->edit, &QLineEdit::cursorPositionChanged, d->runBar, qOverload<>(&QWidget::update));

    // The Run chip grows in after the mark, and the text moves over for it
    d->runChip = new M3SpringAnimation(this);
    d->runChip->setToken(M3Motion::SpatialFast);
    d->runChip->snapTo(0.0);
    connect(d->runChip, &M3SpringAnimation::valueChanged, this, [this](const QVariant& value) {
        d->runBar->chip = qMax(0.0, value.toDouble());
        d->edit->setTextMargins(36 + 14 + qRound((RunBar::ChipWidth + 10) * d->runBar->chip), 0, 52, 0);
        d->runBar->update();
    });

    d->frames.setInterval(16);
    d->frames.setTimerType(Qt::PreciseTimer);
    connect(&d->frames, &QTimer::timeout, this, &Launcher::frame);
    d->clock.start();

    LauncherData* data = LauncherData::instance();
    connect(data, &LauncherData::appsChanged, this, [this] {
        d->glyphs.clear();
        this->relayout();
    });
    connect(data, &LauncherData::pinsChanged, this, &Launcher::relayout);
    connect(data, &LauncherData::recentsChanged, this, &Launcher::relayout);
}

Launcher::~Launcher() {
    delete d;
}

int Launcher::columnWidth(int available) {
    return qMin(780, available - 32);
}

void Launcher::opened(bool run) {
    LauncherData::instance()->refresh();
    d->runChip->snapTo(0.0);
    d->logo->setRun(false, false);
    d->edit->setPlaceholderText(tr("Search apps, files and settings"));
    d->failed.clear();
    d->mode = Mode::Home;
    d->category.clear();
    d->scroll = 0;
    d->selected = -1;
    d->hovered = -1;
    d->pressed = -1;
    d->motion.clear();
    d->edit->clear();
    d->edit->setFocus();
    d->openedAt.start();
    if (run) enterRun();
    this->relayout();
    if (!d->frames.isActive()) d->frames.start();
}

void Launcher::enterRun() {
    d->mode = Mode::Run;
    d->failed.clear();
    LauncherData::instance()->scanPath();
    d->logo->setRun(true);
    d->runChip->animateTo(1.0);
    d->edit->setPlaceholderText(tr("Type a command"));
}

void Launcher::leaveRun() {
    d->mode = Mode::Home;
    d->failed.clear();
    d->runBar->ghost.clear();
    d->logo->setRun(false);
    d->runChip->animateTo(0.0);
    d->edit->setPlaceholderText(tr("Search apps, files and settings"));
    this->relayout();
}

void Launcher::runCommand(const QString& command, bool inTerminal) {
    // Split as a shell would split it (quotes), but not run by one
    QStringList arguments = QProcess::splitCommand(command);
    if (arguments.isEmpty()) return;
    const QString program = arguments.takeFirst();
    bool started = false;
    if (!QStandardPaths::findExecutable(program).isEmpty()) {
        const QString terminal = LauncherData::instance()->terminal();
        if (!inTerminal) {
            started = QProcess::startDetached(program, arguments, QDir::homePath());
        } else if (!terminal.isEmpty()) {
            // The terminal stays open on a shell when the command is done.
            // Most run what follows -e; these two take it after their own flag
            const QString name = QFileInfo(terminal).fileName();
            const QString flag = name == QLatin1String("xfce4-terminal") ? QStringLiteral("-x") : name == QLatin1String("gnome-terminal") ? QStringLiteral("--") : QStringLiteral("-e");
            started = QProcess::startDetached(terminal, {flag, QStringLiteral("sh"), QStringLiteral("-c"), command + QStringLiteral("; exec \"${SHELL:-sh}\"")}, QDir::homePath());
        }
    }
    if (started) {
        LauncherData::instance()->ran(command);
        emit closeRequested();
        return;
    }

    // It stays open, red, with what it might have been
    d->failed = command;
    d->closest.clear();
    QList<QPair<int, QString>> near;
    const QString name = QFileInfo(program).fileName();
    const QMap<QString, QString>& commands = LauncherData::instance()->commands();
    for (auto i = commands.constBegin(); i != commands.constEnd(); i++) {
        if (qAbs(i.key().size() - name.size()) > 2) continue;
        const int apart = distance(name, i.key());
        if (apart > 0 && apart <= qMax(1, int(name.size()) / 3)) near.append({apart, i.key()});
    }
    std::stable_sort(near.begin(), near.end(), [](const QPair<int, QString>& a, const QPair<int, QString>& b) {
        return a.first < b.first;
    });
    for (int i = 0; i < qMin(3, int(near.count())); i++) d->closest.append(near.at(i).second);
    d->failedAt.start();
    d->selected = 0;
    d->selectedAction = 0;
    this->relayout();
    if (!d->frames.isActive()) {
        d->lastFrame = d->clock.elapsed();
        d->frames.start();
    }
}

QRegion Launcher::shape() const {
    QPainterPath path;
    path.addRoundedRect(QRectF(0, 0, this->width(), BarHeight), BarHeight / 2.0, BarHeight / 2.0);
    path.addRoundedRect(d->panel, 28, 28);
    QRegion region;
    for (const QPolygonF& polygon : path.toFillPolygons()) region += QRegion(polygon.toPolygon());
    return region;
}

void Launcher::resizeEvent(QResizeEvent* event) {
    Q_UNUSED(event)
    d->edit->setGeometry(0, 0, this->width(), BarHeight);
    d->avatar->setGeometry(this->width() - 10 - 44, 10, 44, 44);
    d->runBar->setGeometry(0, 0, this->width(), BarHeight);
    this->relayout();
}

void Launcher::thenClose(const std::function<void()>& run) {
    // What opens another part of the shell waits for the Gateway to be gone
    emit closeRequested();
    QTimer::singleShot(160, qApp, run);
}

void Launcher::relayout() {
    if (this->width() <= 0) return;
    const QString selectedId = d->selected >= 0 && d->selected < d->items.count() ? d->items.at(d->selected).id : QString();
    d->items.clear();
    d->headers.clear();
    d->toolbar = QRectF();
    d->grid = QRectF();
    d->hints = QRectF();

    double y = BarHeight + Gap + Pad;
    switch (d->mode) {
        case Mode::Home:
            layoutHome(y);
            break;
        case Mode::AllApps:
            layoutAllApps(y);
            break;
        case Mode::Search:
            layoutSearch(y);
            break;
        case Mode::Run:
            layoutRun(y);
            break;
    }
    d->panel = QRectF(0, BarHeight + Gap, this->width(), y - (BarHeight + Gap));

    // The same thing stays selected when the layout changes under it
    if (d->mode == Mode::Search || (d->mode == Mode::Run && !d->edit->text().trimmed().isEmpty())) {
        d->selected = d->items.isEmpty() ? -1 : qBound(0, d->selected, int(d->items.count()) - 1);
    } else {
        d->selected = -1;
        for (int i = 0; i < d->items.count(); i++) {
            if (!selectedId.isEmpty() && d->items.at(i).id == selectedId) d->selected = i;
        }
    }
    d->hovered = -1;
    d->pressed = -1;
    emit shapeChanged();
    this->update();
}

void Launcher::layoutToolbar(double& y) {
    struct Tool {
            const char* id;
            const char* symbol;
            QString tip;
            bool destructive;
            std::function<void()> run;
    };
    PowerManager* power = StateManager::instance()->powerManager();
    const QList<Tool> tools = {
        {"lock",     "lock",               tr("Lock"),             false, [this, power] { thenClose([power] { power->performPowerOperation(PowerManager::Lock); }); }                                                                                                    },
        {"sleep",    "bedtime",            tr("Sleep"),            false, [this, power] { thenClose([power] { power->performPowerOperation(PowerManager::Suspend); }); }                                                                                                 },
        {"restart",  "restart_alt",        tr("Restart..."),       false, [this, power] { thenClose([power] { power->showPowerOffConfirmation(PowerManager::Reboot); }); }                                                                                               },
        {"",         "",                   QString(),              false, nullptr                                                                                                                                                                                           },
        {"settings", "settings",           tr("System Settings"),  false, [this] { thenClose([] { StateManager::statusCenterManager()->show(); StateManager::statusCenterManager()->setPane(QStringLiteral("SystemSettings")); }); }                                      },
        {"status",   "notifications",      tr("Status Center"),    false, [this] { thenClose([] { StateManager::statusCenterManager()->show(); }); }                                                                                                                      },
        {"power",    "power_settings_new", tr("End session..."),   true,  [this, power] { thenClose([power] { power->showPowerOffConfirmation(); }); }                                                                                                                    },
    };
    const double width = 12 + 5 * 56 + 16 + 64 + 12;
    d->toolbar = QRectF((this->width() - width) / 2, y, width, ToolbarHeight);
    double x = d->toolbar.left() + 12;
    for (const Tool& tool : tools) {
        if (!tool.run) {
            x += 16;
            continue;
        }
        Item item;
        item.kind = Item::Tool;
        item.id = QStringLiteral("tool:") + QLatin1String(tool.id);
        item.symbol = QLatin1String(tool.symbol);
        item.tip = tool.tip;
        item.destructive = tool.destructive;
        item.run = tool.run;
        item.rect = tool.destructive ? QRectF(x + 4, y + 8, 60, 48) : QRectF(x, y, 56, ToolbarHeight);
        x += tool.destructive ? 64 : 56;
        d->items.append(item);
    }
    y += ToolbarHeight + 16;
}

void Launcher::layoutHome(double& y) {
    LauncherData* data = LauncherData::instance();
    const double width = this->width();

    d->headers.append({QPointF(Pad + 8, y + 16), data->hasPins() ? tr("Pinned") : tr("Your apps")});
    Item toggle;
    toggle.kind = Item::Toggle;
    toggle.id = QStringLiteral("toggle");
    toggle.title = tr("All apps");
    toggle.symbol = QStringLiteral("apps");
    const double toggleWidth = 44 + QFontMetricsF(flex(13.5, 560)).horizontalAdvance(toggle.title) + 14;
    toggle.rect = QRectF(width - Pad - toggleWidth, y - 4, toggleWidth, 32);
    toggle.run = [this] {
        d->mode = Mode::AllApps;
        d->scroll = 0;
        this->relayout();
    };
    d->items.append(toggle);
    y += 40;

    const QList<const LauncherApp*> pinned = data->pinned(PinCount);
    const double tile = (width - 28) / PinCount;
    for (int i = 0; i < pinned.count(); i++) {
        Item item;
        item.kind = Item::Pin;
        item.app = pinned.at(i);
        item.id = QStringLiteral("pin:") + item.app->id;
        item.title = item.app->name;
        item.index = i;
        item.rect = QRectF(14 + i * tile, y, tile, TileHeight);
        const LauncherApp* app = item.app;
        item.run = [this, app] {
            LauncherData::instance()->launch(app);
            emit closeRequested();
        };
        d->items.append(item);
    }
    y += TileHeight + 8;

    const QList<LauncherFile> recents = data->recentFiles(3);
    if (!recents.isEmpty()) {
        d->headers.append({QPointF(Pad + 8, y + 16), tr("Pick up where you left off")});
        y += 28;
        for (int i = 0; i < recents.count(); i++) {
            const LauncherFile& file = recents.at(i);
            Item item;
            item.kind = Item::Row;
            item.id = QStringLiteral("recent:") + file.path;
            item.title = file.name;
            item.subtitle = QStringLiteral("%1 · %2").arg(QFileInfo(file.path).dir().dirName(), ago(file.when));
            item.symbol = symbolFor(file.path);
            item.first = i == 0;
            item.last = i == recents.count() - 1;
            item.rect = QRectF(Pad, y, width - 2 * Pad, RowHeight);
            const QString path = file.path;
            item.run = [this, path] {
                QDesktopServices::openUrl(QUrl::fromLocalFile(path));
                emit closeRequested();
            };
            d->items.append(item);
            y += RowHeight + 2;
        }
        y += 6;
    }
    y += 12;
    layoutToolbar(y);
}

void Launcher::layoutAllApps(double& y) {
    LauncherData* data = LauncherData::instance();
    const double width = this->width();

    d->headers.append({QPointF(Pad + 8, y + 16), tr("All apps")});
    Item toggle;
    toggle.kind = Item::Toggle;
    toggle.id = QStringLiteral("toggle");
    toggle.title = data->hasPins() ? tr("Pinned") : tr("Back");
    toggle.symbol = QStringLiteral("arrow_back");
    const double toggleWidth = 44 + QFontMetricsF(flex(13.5, 560)).horizontalAdvance(toggle.title) + 14;
    toggle.rect = QRectF(width - Pad - toggleWidth, y - 4, toggleWidth, 32);
    toggle.run = [this] {
        d->mode = Mode::Home;
        this->relayout();
    };
    d->items.append(toggle);
    y += 40;

    // Categories that hold something, as filter chips
    static const QList<QPair<QString, const char*>> categories = {
        {QStringLiteral("Development"), QT_TR_NOOP("Development")},
        {QStringLiteral("Network"),     QT_TR_NOOP("Internet")   },
        {QStringLiteral("AudioVideo"),  QT_TR_NOOP("Media")      },
        {QStringLiteral("Graphics"),    QT_TR_NOOP("Graphics")   },
        {QStringLiteral("Office"),      QT_TR_NOOP("Office")     },
        {QStringLiteral("Game"),        QT_TR_NOOP("Games")      },
        {QStringLiteral("Education"),   QT_TR_NOOP("Education")  },
        {QStringLiteral("Utility"),     QT_TR_NOOP("Utilities")  },
        {QStringLiteral("Settings"),    QT_TR_NOOP("Settings")   },
    };
    double x = Pad;
    auto chip = [&](const QString& category, const QString& label) {
        Item item;
        item.kind = Item::Chip;
        item.id = QStringLiteral("chip:") + category;
        item.title = label;
        item.checked = d->category == category;
        const double w = QFontMetricsF(flex(13.5, 520)).horizontalAdvance(label) + (item.checked ? 46 : 28);
        if (x + w > width - Pad) return;
        item.rect = QRectF(x, y, w, 32);
        item.run = [this, category] {
            d->category = category;
            d->scroll = 0;
            this->relayout();
        };
        d->items.append(item);
        x += w + 8;
    };
    chip(QString(), tr("All"));
    for (const auto& [category, label] : categories) {
        bool any = false;
        for (const LauncherApp& app : data->apps()) {
            if (app.categories.contains(category)) any = true;
        }
        if (any) chip(category, tr(label));
    }
    y += 32 + 16;

    // The grid: as many rows as there's room for, the rest a scroll away
    QList<const LauncherApp*> shown;
    for (const LauncherApp& app : data->apps()) {
        if (d->category.isEmpty() || app.categories.contains(d->category)) shown.append(&app);
    }
    const int columns = PinCount;
    const double tile = (width - 28) / columns;
    const int rows = (shown.count() + columns - 1) / columns;
    const double room = this->height() - y - 12 - ToolbarHeight - 16 - 8;
    const int visible = qBound(1, int(room / GridRow), qMax(1, rows));
    d->grid = QRectF(14, y, width - 28, visible * GridRow);
    d->scrollMax = qMax(0.0, rows * double(GridRow) - d->grid.height());
    d->scroll = qBound(0.0, d->scroll, d->scrollMax);
    for (int i = 0; i < shown.count(); i++) {
        Item item;
        item.kind = Item::Grid;
        item.app = shown.at(i);
        item.id = QStringLiteral("app:") + item.app->id;
        item.title = item.app->name;
        item.index = i;
        // In the grid's own space: the scroll is taken off when it's drawn and hit
        item.rect = QRectF(14 + (i % columns) * tile, y + (i / columns) * GridRow, tile, GridRow);
        const LauncherApp* app = item.app;
        item.run = [this, app] {
            LauncherData::instance()->launch(app);
            emit closeRequested();
        };
        d->items.append(item);
    }
    y += d->grid.height() + 12;
    layoutToolbar(y);
}

void Launcher::layoutSearch(double& y) {
    LauncherData* data = LauncherData::instance();
    const QString typed = d->edit->text().trimmed();
    const QString query = LauncherData::fold(typed);
    const double width = this->width();
    QList<Item> found;

    // A sum
    double value = 0;
    if (Sum(typed).work(&value)) {
        Item item;
        item.id = QStringLiteral("sum");
        item.title = QStringLiteral("= ") + QLocale().toString(value, 'g', 12);
        item.subtitle = tr("Sum");
        item.symbol = QStringLiteral("calculate");
        item.group = tr("Sum");
        item.score = 1000;
        const QString answer = QLocale::c().toString(value, 'g', 12);
        item.run = [this, answer] {
            QApplication::clipboard()->setText(answer);
            emit closeRequested();
        };
        item.actions.append({tr("Copy"), item.run, QRectF()});
        found.append(item);
    }

    // Apps: by name first, then by what they are; what's launched often rises
    for (const LauncherApp& app : data->apps()) {
        double score = 0;
        if (app.folded == query) score = 100;
        else if (app.folded.startsWith(query)) score = 80;
        else if (app.folded.contains(QLatin1Char(' ') + query)) score = 65;
        else if (app.folded.contains(query)) score = 45;
        else if (app.alsoFolded.contains(query)) score = 30;
        if (score <= 0) continue;
        score += qMin(20, data->launches(app.id)) * 0.6;
        Item item;
        item.app = &app;
        item.id = QStringLiteral("app:") + app.id;
        item.title = app.name;
        item.subtitle = app.generic.isEmpty() ? tr("Application") : app.generic;
        item.group = tr("Apps");
        item.score = score;
        const LauncherApp* pointer = &app;
        item.run = [this, pointer] {
            LauncherData::instance()->launch(pointer);
            emit closeRequested();
        };
        item.actions.append({tr("Open"), item.run, QRectF()});
        for (const QString& action : app.actions) {
            if (item.actions.count() >= 3) break;
            const QString label = app.app->getActionProperty(action, "Name").toString();
            if (label.isEmpty()) continue;
            item.actions.append({label, [this, pointer, action] {
                                     LauncherData::instance()->launch(pointer, action);
                                     emit closeRequested();
                                 },
                QRectF()});
        }
        found.append(item);
    }

    // Settings pages, by their names and by what's on them
    static const QHash<QString, QString> also = {
        {QStringLiteral("NetworkManagerPane"),    QStringLiteral("wifi wi-fi wireless ethernet vpn internet hotspot network")       },
        {QStringLiteral("BluetoothSettings"),     QStringLiteral("bluetooth pair headphones iphone")                                },
        {QStringLiteral("DisplaySettings"),       QStringLiteral("display monitor screen resolution scale redshift night tv mirror")},
        {QStringLiteral("ThemeSettings"),         QStringLiteral("theme colour color dark light wallpaper background font cursor")  },
        {QStringLiteral("PowerSettings"),         QStringLiteral("power battery sleep suspend lid charging")                        },
        {QStringLiteral("InputSettings"),         QStringLiteral("keyboard mouse touchpad trackpoint shortcuts")                    },
        {QStringLiteral("LocaleSettings"),        QStringLiteral("language region locale")                                          },
        {QStringLiteral("DateTimeSettings"),      QStringLiteral("date time timezone clock")                                        },
        {QStringLiteral("UsersSettings"),         QStringLiteral("users account password picture")                                  },
        {QStringLiteral("DefaultAppsSettings"),   QStringLiteral("default apps browser")                                            },
        {QStringLiteral("AccessibilitySettings"), QStringLiteral("accessibility sticky keys")                                       },
        {QStringLiteral("SystemAbout"),           QStringLiteral("about version system device")                                     },
    };
    StatusCenterManager* statusCenter = StateManager::statusCenterManager();
    for (StatusCenterPane* pane : statusCenter->panes()) {
        if (statusCenter->paneType(pane) != StatusCenterManager::SystemSettings) continue;
        const QString name = LauncherData::fold(pane->displayName());
        double score = 0;
        if (name.startsWith(query)) score = 70;
        else if (name.contains(query)) score = 50;
        else if (query.size() >= 2 && (QLatin1Char(' ') + also.value(pane->name())).contains(QLatin1Char(' ') + query)) score = 40;
        if (score <= 0) continue;
        Item item;
        item.id = QStringLiteral("pane:") + pane->name();
        item.title = pane->displayName();
        item.subtitle = tr("System Settings");
        item.icon = pane->icon().pixmap(QSize(20, 20), this->devicePixelRatioF()).toImage();
        item.symbol = QStringLiteral("settings");
        item.group = tr("Settings");
        item.score = score;
        const QString paneName = pane->name();
        item.run = [this, paneName] {
            thenClose([paneName] {
                StateManager::statusCenterManager()->show();
                StateManager::statusCenterManager()->setPane(QStringLiteral("SystemSettings"));
                StateManager::statusCenterManager()->setPane(paneName);
            });
        };
        item.actions.append({tr("Open"), item.run, QRectF()});
        found.append(item);
    }

    // The session: by its words
    struct Command {
            QString words, title, symbol;
            int operation; // PowerManager's; -1: the whole End Session screen
    };
    const QList<Command> commands = {
        {tr("lock"),                   tr("Lock"),         QStringLiteral("lock"),               PowerManager::Lock   },
        {tr("sleep suspend"),          tr("Sleep"),        QStringLiteral("bedtime"),            PowerManager::Suspend},
        {tr("restart reboot"),         tr("Restart..."),   QStringLiteral("restart_alt"),        PowerManager::Reboot },
        {tr("shut down power off"),    tr("Shut down..."), QStringLiteral("power_settings_new"), PowerManager::PowerOff},
        {tr("log out sign out"),       tr("Log out..."),   QStringLiteral("logout"),             PowerManager::LogOut },
    };
    for (const Command& command : commands) {
        if (query.size() < 2 || !(QLatin1Char(' ') + LauncherData::fold(command.words)).contains(QLatin1Char(' ') + query)) continue;
        Item item;
        item.id = QStringLiteral("power:") + command.symbol;
        item.title = command.title;
        item.subtitle = tr("This session");
        item.symbol = command.symbol;
        item.group = tr("Session");
        item.score = 60;
        const int operation = command.operation;
        item.run = [this, operation] {
            thenClose([operation] {
                PowerManager* power = StateManager::instance()->powerManager();
                // Locking and sleeping just happen; what ends the session asks first
                if (operation == PowerManager::Lock || operation == PowerManager::Suspend) power->performPowerOperation(PowerManager::PowerOperation(operation));
                else power->showPowerOffConfirmation(PowerManager::PowerOperation(operation));
            });
        };
        item.actions.append({command.title, item.run, QRectF()});
        found.append(item);
    }

    // Files, by name
    for (const LauncherFile& file : data->findFiles(typed, 4)) {
        Item item;
        item.id = QStringLiteral("file:") + file.path;
        item.title = file.name;
        item.subtitle = QStringLiteral("%1 · %2").arg(QFileInfo(file.path).dir().dirName(), ago(file.when));
        item.symbol = symbolFor(file.path);
        item.group = tr("Files");
        item.score = 20;
        const QString path = file.path;
        item.run = [this, path] {
            QDesktopServices::openUrl(QUrl::fromLocalFile(path));
            emit closeRequested();
        };
        item.actions.append({tr("Open"), item.run, QRectF()});
        const QString folder = QFileInfo(path).absolutePath();
        item.actions.append({tr("Show in folder"), [this, folder] {
                                 QDesktopServices::openUrl(QUrl::fromLocalFile(folder));
                                 emit closeRequested();
                             },
            QRectF()});
        found.append(item);
    }

    // The best of them all is the top hit; the rest stay with their kind
    std::stable_sort(found.begin(), found.end(), [](const Item& a, const Item& b) {
        return a.score > b.score;
    });
    const QStringList order = {tr("Sum"), tr("Apps"), tr("Settings"), tr("Session"), tr("Files")};
    const QHash<QString, int> most = {{tr("Apps"), 4}, {tr("Settings"), 2}, {tr("Session"), 2}, {tr("Files"), 3}};

    if (!found.isEmpty()) {
        Item top = found.takeFirst();
        top.kind = Item::Top;
        top.rect = QRectF(Pad, y, width - 2 * Pad, TopHeight);
        if (d->topId != top.id) {
            d->topId = top.id;
            d->topSince.start();
        }
        d->items.append(top);
        y += TopHeight + 12;
    } else {
        d->topId.clear();
    }

    // How many rows fit under it: the web search and the key hints stay
    const double room = this->height() - y - (RowHeight + 12) - 30 - 8;
    double used = 0;
    for (const QString& group : order) {
        QList<Item> rows;
        for (const Item& item : std::as_const(found)) {
            if (item.group == group && rows.count() < most.value(group, 2)) rows.append(item);
        }
        while (!rows.isEmpty() && used + 30 + rows.count() * (RowHeight + 2) + 8 > room) rows.removeLast();
        if (rows.isEmpty()) continue;
        d->headers.append({QPointF(Pad + 8, y + 16), group});
        y += 28;
        used += 28;
        for (int i = 0; i < rows.count(); i++) {
            Item item = rows.at(i);
            item.kind = Item::Row;
            item.first = i == 0;
            item.last = i == rows.count() - 1;
            item.rect = QRectF(Pad, y, width - 2 * Pad, RowHeight);
            d->items.append(item);
            y += RowHeight + 2;
            used += RowHeight + 2;
        }
        y += 8;
        used += 8;
    }

    Item web;
    web.kind = Item::Web;
    web.id = QStringLiteral("web");
    web.title = tr("Search the web for “%1”").arg(typed);
    web.symbol = QStringLiteral("travel_explore");
    web.rect = QRectF(Pad, y + 4, width - 2 * Pad, RowHeight - 4);
    web.run = [this, typed] {
        searchWeb(typed);
        emit closeRequested();
    };
    d->items.append(web);
    y += RowHeight + 12;

    d->hints = QRectF(Pad + 6, y, width - 2 * Pad, 20);
    y += 20 + 12;
}

void Launcher::layoutRun(double& y) {
    LauncherData* data = LauncherData::instance();
    const QString typed = d->edit->text();
    const QString trimmed = typed.trimmed();
    const double width = this->width();
    const QList<LauncherCommand> history = data->history();
    const QMap<QString, QString>& commands = data->commands();

    auto command = [this](const QString& line, const QString& subtitle, const QString& symbol, int prefix) {
        Item item;
        item.kind = Item::Row;
        item.id = QStringLiteral("command:") + line;
        item.title = line;
        item.subtitle = subtitle;
        item.symbol = symbol;
        item.prefix = prefix;
        item.run = [this, line] {
            runCommand(line, false);
        };
        return item;
    };
    QList<QPair<QString, QList<Item>>> groups;
    Item top;
    top.kind = Item::Top;
    d->runBar->ghost.clear();

    if (trimmed.isEmpty()) {
        QList<Item> recent;
        for (const LauncherCommand& past : history) {
            if (recent.count() < 5) recent.append(command(past.command, tr("ran %1").arg(ago(past.when)), QStringLiteral("history"), 0));
        }
        groups.append({tr("Recent commands"), recent});
    } else if (!d->failed.isEmpty()) {
        // What wouldn't start: in the error colours, with the nearest names
        const QString program = QProcess::splitCommand(d->failed).value(0);
        const QString rest = d->failed.trimmed().section(QLatin1Char(' '), 1);
        auto fixed = [&](const QString& name) {
            return rest.isEmpty() ? name : name + QLatin1Char(' ') + rest;
        };
        top = command(d->failed, QStandardPaths::findExecutable(program).isEmpty() ? tr("Not a command on this computer") : tr("Couldn't start it"), QStringLiteral("priority_high"), int(d->failed.size()));
        top.kind = Item::Top;
        top.id = QStringLiteral("failed:") + d->failed;
        top.destructive = true;
        if (!d->closest.isEmpty()) {
            const QString fix = fixed(d->closest.first());
            top.actions.append({tr("Run %1").arg(d->closest.first()), [this, fix] {
                                    runCommand(fix, false);
                                },
                QRectF()});
        }
        const QString failed = d->failed;
        top.actions.append({tr("Search the web"), [this, failed] {
                                searchWeb(failed);
                                emit closeRequested();
                            },
            QRectF()});
        top.run = top.actions.first().run;
        QList<Item> near;
        for (const QString& name : std::as_const(d->closest)) near.append(command(fixed(name), commands.value(name), QStringLiteral("terminal"), 0));
        groups.append({tr("Did you mean"), near});
    } else {
        // Completions: the history first, then the word being typed as a
        // program on PATH (the first word, or the one after "-e")
        QList<Item> found;
        for (const LauncherCommand& past : history) {
            if (past.command.startsWith(typed)) found.append(command(past.command, tr("ran %1").arg(ago(past.when)), QStringLiteral("history"), int(typed.size())));
        }
        const int space = typed.lastIndexOf(QLatin1Char(' '));
        const QString word = typed.mid(space + 1), head = typed.left(space + 1);
        if (!word.isEmpty() && (space < 0 || typed.left(space).endsWith(QLatin1String(" -e")))) {
            QStringList names;
            for (auto i = commands.lowerBound(word); i != commands.constEnd() && i.key().startsWith(word) && names.count() < 200; i++) names.append(i.key());
            std::stable_sort(names.begin(), names.end(), [](const QString& a, const QString& b) {
                return a.size() < b.size();
            });
            for (const QString& name : names.mid(0, 6)) {
                const QString line = head + name;
                if (std::none_of(found.begin(), found.end(), [&](const Item& item) { return item.title == line; })) found.append(command(line, commands.value(name), QStringLiteral("terminal"), int(typed.size())));
            }
        }
        for (const Item& item : std::as_const(found)) {
            if (item.title.size() > typed.size()) {
                d->runBar->ghost = item.title.mid(typed.size());
                break;
            }
        }

        // The top hit, which Enter runs: what's typed when it's already a
        // whole command, else the best completion of it
        const bool whole = std::any_of(history.begin(), history.end(), [&](const LauncherCommand& past) { return past.command == trimmed; }) || (!trimmed.contains(QLatin1Char(' ')) && commands.contains(trimmed));
        int from = -1;
        for (int i = 0; i < found.count(); i++) {
            if (whole ? found.at(i).title == trimmed : from < 0) from = i;
        }
        if (from >= 0) {
            top = found.takeAt(from);
            top.subtitle = tr("Command · %1").arg(top.subtitle);
        } else {
            const QString program = trimmed.section(QLatin1Char(' '), 0, 0);
            top = command(trimmed, commands.contains(program) ? tr("Command · %1").arg(commands.value(program)) : tr("Command"), QString(), int(trimmed.size()));
        }
        top.kind = Item::Top;
        top.symbol = QStringLiteral("terminal");
        const QString line = top.title;
        top.actions.append({tr("Run"), top.run, QRectF()});
        top.actions.append({tr("In terminal"), [this, line] {
                                runCommand(line, true);
                            },
            QRectF()});

        QList<Item> past, programs;
        for (const Item& item : std::as_const(found)) {
            if (item.symbol == QLatin1String("history")) {
                if (past.count() < 3) past.append(item);
            } else if (programs.count() < 4) {
                programs.append(item);
            }
        }
        groups.append({tr("History"), past});
        groups.append({tr("Commands"), programs});
    }

    if (top.run) {
        top.rect = QRectF(Pad, y, width - 2 * Pad, TopHeight);
        if (d->topId != top.id) {
            d->topId = top.id;
            d->topSince.start();
        }
        d->items.append(top);
        y += TopHeight + 12;
    } else {
        d->topId.clear();
    }

    // Under it, as many rows as fit above the key hints
    const double room = this->height() - y - 30 - 8;
    double used = 0;
    for (auto& [group, rows] : groups) {
        while (!rows.isEmpty() && used + 30 + rows.count() * (RowHeight + 2) + 8 > room) rows.removeLast();
        if (rows.isEmpty()) continue;
        d->headers.append({QPointF(Pad + 8, y + 16), group});
        y += 28;
        used += 28;
        for (int i = 0; i < rows.count(); i++) {
            Item item = rows.at(i);
            item.first = i == 0;
            item.last = i == rows.count() - 1;
            item.rect = QRectF(Pad, y, width - 2 * Pad, RowHeight);
            d->items.append(item);
            y += RowHeight + 2;
            used += RowHeight + 2;
        }
        y += 8;
        used += 8;
    }
    d->runBar->update();

    d->hints = QRectF(Pad + 6, y + 4, width - 2 * Pad, 20);
    y += 4 + 20 + 12;
}

void Launcher::frame() {
    // Hover and press are springs; the entrance and the top hit's turn run
    // off clocks. The timer rests when nothing moves
    const qint64 now = d->clock.elapsed();
    const double dt = qBound(0.001, (now - d->lastFrame) / 1000.0, 0.05);
    d->lastFrame = now;
    const M3Motion::Spring spring = M3Motion::spring(M3Motion::SpatialFast);
    bool moving = false;
    for (int i = 0; i < d->items.count(); i++) {
        Motion& m = d->moving(d->items.at(i).id);
        const double hover = i == d->hovered ? 1 : 0, press = i == d->pressed ? 1 : 0;
        const M3Motion::SpringState h = M3Motion::stepSpring(m.hover, m.hoverSpeed, hover, spring, dt);
        const M3Motion::SpringState p = M3Motion::stepSpring(m.press, m.pressSpeed, press, spring, dt);
        m.hover = h.value;
        m.hoverSpeed = h.velocity;
        m.press = p.value;
        m.pressSpeed = p.velocity;
        if (qAbs(m.hover - hover) > 0.002 || qAbs(m.hoverSpeed) > 0.02 || qAbs(m.press - press) > 0.002 || qAbs(m.pressSpeed) > 0.02) moving = true;
    }
    if (d->openedAt.isValid() && d->openedAt.elapsed() < 1400) moving = true;
    if (d->topSince.isValid() && d->topSince.elapsed() < 900) moving = true;
    if (!d->failed.isEmpty() && d->failedAt.isValid() && d->failedAt.elapsed() < 900) moving = true;
    if (!moving) d->frames.stop();
    this->update();
}

void Launcher::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event)
    const M3::Scheme s = M3::currentScheme();
    const bool expressive = M3Motion::expressive();
    const double since = d->openedAt.isValid() ? d->openedAt.elapsed() / 1000.0 : 10;
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);

    // The panel comes down from the bar as it opens
    const double entered = expressive ? springAt(since, 0.8, 380) : 1;
    p.translate(0, (entered - 1) * 18);
    p.setOpacity(qBound(0.0, since / 0.12, 1.0));
    p.setPen(Qt::NoPen);
    p.setBrush(s.surfaceContainer);
    p.drawRoundedRect(d->panel, 28, 28);

    for (const auto& [at, text] : std::as_const(d->headers)) {
        p.setFont(flex(13, 700, 112));
        p.setPen(s.primary);
        p.drawText(QPointF(at.x(), at.y()), text);
    }

    if (!d->toolbar.isNull()) {
        p.setPen(Qt::NoPen);
        p.setBrush(s.primaryContainer);
        p.drawRoundedRect(d->toolbar, ToolbarHeight / 2.0, ToolbarHeight / 2.0);
        // Between what acts on the session and where things are set
        p.setBrush(M3::layer(s.primaryContainer, s.onPrimaryContainer, 0.24));
        p.drawRoundedRect(QRectF(d->toolbar.left() + 12 + 3 * 56 + 7, d->toolbar.center().y() - 12, 2, 24), 1, 1);
    }

    // An app's picture in two tones of its container, kept once made
    auto glyph = [&](const LauncherApp* app, int side, const QColor& container, const QColor& content) {
        const QString key = QStringLiteral("%1-%2-%3-%4").arg(app->id).arg(side).arg(container.rgb()).arg(content.rgb());
        auto found = d->glyphs.constFind(key);
        if (found != d->glyphs.constEnd()) return *found;
        const qreal ratio = this->devicePixelRatioF();
        QImage image = app->app->icon(QSize(side, side) * ratio).toImage();
        if (image.isNull()) {
            // An app without a picture: a plain symbol, so its place isn't empty
            image = QImage(QSize(side, side) * ratio, QImage::Format_ARGB32_Premultiplied);
            image.fill(Qt::transparent);
            QPainter plain(&image);
            plain.setRenderHint(QPainter::TextAntialiasing);
            plain.setFont(symbols(side * ratio * 0.9));
            plain.setPen(content);
            plain.drawText(image.rect(), Qt::AlignCenter, QStringLiteral("widgets"));
        } else if (M3ThemedIcon::enabled()) {
            image = M3ThemedIcon::glyph(image, container, content);
        }
        d->glyphs.insert(key, image);
        return image;
    };
    auto drawSymbol = [&](const QRectF& box, const QString& name, double size, const QColor& colour) {
        p.setFont(symbols(size));
        p.setPen(colour);
        p.drawText(box, Qt::AlignCenter, name);
    };
    auto ring = [&](const QPainterPath& path) {
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(s.primary, 2.5));
        p.drawPath(path);
        p.setPen(Qt::NoPen);
    };

    for (int i = 0; i < d->items.count(); i++) {
        const Item& item = d->items.at(i);
        const Motion m = d->motion.value(item.id);
        const bool selected = i == d->selected;
        const double hover = qBound(0.0, m.hover, 1.2), press = qBound(0.0, m.press, 1.2);
        QRectF r = item.rect;
        p.setPen(Qt::NoPen);

        switch (item.kind) {
            case Item::Pin:
            case Item::Grid: {
                const bool pin = item.kind == Item::Pin;
                if (!pin) {
                    r.translate(0, -d->scroll);
                    if (r.bottom() < d->grid.top() || r.top() > d->grid.bottom()) break;
                    p.save();
                    p.setClipRect(d->grid);
                }
                const double radius = pin ? 30 : 26;
                const QPointF centre(r.center().x(), r.top() + radius + (pin ? 0 : 2));
                // Pinned apps arrive one after another, each popping into its shape
                const double pop = pin && expressive ? springAt(since - 0.05 - item.index * 0.035, 0.6, 500) : 1;
                const double scale = pop * (1 + 0.07 * hover - 0.12 * press);
                const QColor container = !pin ? s.primaryContainer : item.index % 3 == 0 ? s.primaryContainer : item.index % 3 == 1 ? s.tertiaryContainer : s.secondaryContainer;
                const QColor content = !pin ? s.onPrimaryContainer : item.index % 3 == 0 ? s.onPrimaryContainer : item.index % 3 == 1 ? s.onTertiaryContainer : s.onSecondaryContainer;
                QPainterPath shape;
                if (pin) shape = m3shapes::toPath(pinShape(item.index), QRectF(-radius, -radius, 2 * radius, 2 * radius));
                else shape.addEllipse(QPointF(0, 0), radius, radius);
                QTransform place;
                place.translate(centre.x(), centre.y());
                place.rotate(pin ? hover * 30 + (1 - qMin(1.0, pop)) * -50 : 0);
                place.scale(scale, scale);
                p.fillPath(place.map(shape), M3::layer(container, content, 0.08 * hover + 0.10 * press));
                if (selected) {
                    QTransform around = place;
                    around.scale(1.16, 1.16);
                    ring(around.map(shape));
                }
                const double side = (pin ? 30 : 26) * scale;
                if (side > 1) p.drawImage(QRectF(centre.x() - side / 2, centre.y() - side / 2, side, side), glyph(item.app, pin ? 30 : 26, container, content));
                p.setFont(flex(pin ? 12.5 : 12, pin ? 520 : 450));
                p.setPen(pin ? s.onSurface : s.onSurfaceVariant);
                p.setOpacity(p.opacity() * qBound(0.0, pop, 1.0));
                const QRectF label(r.left() + 4, centre.y() + radius + (pin ? 8 : 6), r.width() - 8, 18);
                p.drawText(label, Qt::AlignHCenter | Qt::AlignVCenter, p.fontMetrics().elidedText(item.title, Qt::ElideRight, int(label.width())));
                p.setOpacity(qBound(0.0, since / 0.12, 1.0));
                if (!pin) p.restore();
                break;
            }
            case Item::Row:
            case Item::Web: {
                const bool web = item.kind == Item::Web;
                const QPainterPath shape = web ? segment(r, 20, 20) : segment(r, item.first ? 20 : 4, item.last ? 20 : 4);
                const QColor base = selected ? s.secondaryContainer : web ? s.surfaceContainer : s.surfaceContainerHigh;
                const QColor content = selected ? s.onSecondaryContainer : s.onSurface;
                p.fillPath(shape, M3::layer(base, content, 0.08 * hover + 0.10 * press));
                if (web && !selected) {
                    p.setBrush(Qt::NoBrush);
                    p.setPen(QPen(s.outlineVariant, 1));
                    p.drawPath(segment(r.adjusted(0.5, 0.5, -0.5, -0.5), 20, 20));
                    p.setPen(Qt::NoPen);
                }
                const QRectF lead(r.left() + 14, r.center().y() - 18, 36, 36);
                if (!web) {
                    p.setBrush(selected ? M3::layer(s.secondaryContainer, s.onSecondaryContainer, 0.14) : s.secondaryContainer);
                    p.drawEllipse(lead);
                }
                if (item.app) {
                    p.drawImage(lead.adjusted(7, 7, -7, -7), glyph(item.app, 22, s.secondaryContainer, s.onSecondaryContainer));
                } else if (!item.icon.isNull()) {
                    QImage tinted = item.icon;
                    QPainter tint(&tinted);
                    tint.setCompositionMode(QPainter::CompositionMode_SourceIn);
                    tint.fillRect(tinted.rect(), s.onSecondaryContainer);
                    tint.end();
                    p.drawImage(lead.adjusted(8, 8, -8, -8), tinted);
                } else {
                    drawSymbol(lead, item.symbol, 20, web ? s.onSurfaceVariant : s.onSecondaryContainer);
                }
                const double textLeft = r.left() + 14 + 36 + 14;
                const double textWidth = r.right() - 16 - textLeft;
                if (item.subtitle.isEmpty()) {
                    p.setFont(flex(14, 450));
                    p.setPen(selected ? content : s.onSurfaceVariant);
                    p.drawText(QRectF(textLeft, r.top(), textWidth, r.height()), Qt::AlignLeft | Qt::AlignVCenter, p.fontMetrics().elidedText(item.title, Qt::ElideRight, int(textWidth)));
                } else if (item.prefix > 0) {
                    // A completion: what's typed, then what it adds in a stronger weight
                    const QString typed = item.title.left(item.prefix);
                    p.setFont(flex(14.5, 420));
                    p.setPen(selected ? content : s.onSurfaceVariant);
                    p.drawText(QRectF(textLeft, r.top() + 8, textWidth, 20), Qt::AlignLeft | Qt::AlignVCenter, typed);
                    const double after = p.fontMetrics().horizontalAdvance(typed);
                    p.setFont(flex(14.5, 560));
                    p.setPen(content);
                    p.drawText(QRectF(textLeft + after, r.top() + 8, qMax(0.0, textWidth - after), 20), Qt::AlignLeft | Qt::AlignVCenter, p.fontMetrics().elidedText(item.title.mid(item.prefix), Qt::ElideRight, int(textWidth - after)));
                    p.setFont(flex(12, 400));
                    p.setPen(selected ? content : s.onSurfaceVariant);
                    p.drawText(QRectF(textLeft, r.top() + 28, textWidth, 18), Qt::AlignLeft | Qt::AlignVCenter, p.fontMetrics().elidedText(item.subtitle, Qt::ElideMiddle, int(textWidth)));
                } else {
                    p.setFont(flex(14.5, 520));
                    p.setPen(content);
                    p.drawText(QRectF(textLeft, r.top() + 8, textWidth, 20), Qt::AlignLeft | Qt::AlignVCenter, p.fontMetrics().elidedText(item.title, Qt::ElideRight, int(textWidth)));
                    p.setFont(flex(12, 400));
                    p.setPen(selected ? content : s.onSurfaceVariant);
                    p.drawText(QRectF(textLeft, r.top() + 28, textWidth, 18), Qt::AlignLeft | Qt::AlignVCenter, p.fontMetrics().elidedText(item.subtitle, Qt::ElideMiddle, int(textWidth)));
                }
                break;
            }
            case Item::Top: {
                // A command that wouldn't start turns the error colours (an
                // effect: no overshoot) and is nudged sideways (a spatial kick)
                double error = 0, nudge = 0;
                if (item.destructive) {
                    const double t = d->failedAt.isValid() ? d->failedAt.elapsed() / 1000.0 : 10;
                    error = expressive ? 1 - std::exp(-40 * t) * (1 + 40 * t) : 1;
                    if (expressive) nudge = -420 / 22.6 * std::exp(-0.6 * 28.3 * t) * std::sin(22.6 * t);
                }
                r.translate(nudge, 0);
                const QColor container = M3::layer(s.primaryContainer, s.errorContainer, error);
                const QColor on = M3::layer(s.onPrimaryContainer, s.onErrorContainer, error);
                const QColor strong = M3::layer(s.primary, s.error, error), onStrong = M3::layer(s.onPrimary, s.onError, error);
                p.setBrush(M3::layer(container, on, 0.05 * hover));
                p.drawRoundedRect(r, 28, 28);
                // Its shape turns into place each time the top hit changes,
                // and bursts into a SoftBoom when it's an error
                static const m3shapes::Morph burst(MaterialShapes::cookie9Sided(), MaterialShapes::softBoom());
                const double settled = expressive && d->topSince.isValid() ? springAt(d->topSince.elapsed() / 1000.0, 0.6, 300) : 1;
                const QPointF centre(r.left() + 24 + 36, r.center().y());
                QTransform place;
                place.translate(centre.x(), centre.y());
                place.rotate((1 - settled) * -70);
                place.scale(0.6 + 0.4 * settled, 0.6 + 0.4 * settled);
                p.fillPath(place.map(m3shapes::toPath(burst, float(error), QRectF(-36, -36, 72, 72))), strong);
                if (item.app) {
                    p.drawImage(QRectF(centre.x() - 20, centre.y() - 20, 40, 40), glyph(item.app, 40, s.primary, s.onPrimary));
                } else {
                    drawSymbol(QRectF(centre.x() - 36, centre.y() - 36, 72, 72), item.symbol, 36, onStrong);
                }

                // Its actions, as buttons from the right: the one Enter
                // takes is filled and round (squarer while pressed), the
                // others wait squarer
                double right = r.right() - 20;
                for (int a = item.actions.count() - 1; a >= 0; a--) {
                    const Action& action = item.actions.at(a);
                    const bool focused = selected && a == d->selectedAction;
                    p.setFont(flex(14, 600));
                    const double w = p.fontMetrics().horizontalAdvance(action.label) + 32;
                    const QRectF button(right - w, r.center().y() - 20, w, 40);
                    d->items[i].actions[a].rect = button.translated(-nudge, 0);
                    right -= w + 8;
                    p.setPen(Qt::NoPen);
                    p.setBrush(focused ? strong : M3::layer(container, on, 0.14));
                    const double corner = focused ? 20 - 8 * qMin(1.0, press) : 12;
                    p.drawRoundedRect(button, corner, corner);
                    p.setPen(focused ? onStrong : on);
                    p.drawText(button, Qt::AlignCenter, action.label);
                }
                const double textLeft = r.left() + 24 + 72 + 20;
                const double textWidth = qMax(60.0, right - textLeft);
                const QRectF titleBox(textLeft, r.center().y() - 34, textWidth, 40);
                p.setPen(on);
                if (item.prefix > 0) {
                    // What's typed, lighter; then what the top hit adds to it
                    const QString typed = item.title.left(item.prefix);
                    p.save();
                    p.setClipRect(titleBox);
                    p.setFont(flex(30, 520, 110));
                    p.drawText(titleBox, Qt::AlignLeft | Qt::AlignVCenter, typed);
                    const double after = p.fontMetrics().horizontalAdvance(typed);
                    const double underline = p.fontMetrics().horizontalAdvance(item.title.section(QLatin1Char(' '), 0, 0));
                    const double baseline = titleBox.center().y() + (p.fontMetrics().ascent() - p.fontMetrics().descent()) / 2.0;
                    p.setFont(flex(30, 780, 118));
                    p.drawText(titleBox.adjusted(after, 0, 0, 0), Qt::AlignLeft | Qt::AlignVCenter, item.title.mid(item.prefix));
                    p.restore();
                    if (error > 0.02) {
                        p.save();
                        p.setOpacity(p.opacity() * qMin(1.0, error));
                        wavyUnderline(p, textLeft, textLeft + qMin(underline, textWidth), baseline + 7, s.error);
                        p.restore();
                    }
                } else {
                    p.setFont(flex(30, 780, 118));
                    p.drawText(titleBox, Qt::AlignLeft | Qt::AlignVCenter, p.fontMetrics().elidedText(item.title, Qt::ElideRight, int(textWidth)));
                }
                p.setPen(on);
                p.setFont(flex(13.5, 450));
                p.drawText(QRectF(textLeft, r.center().y() + 8, textWidth, 20), Qt::AlignLeft | Qt::AlignVCenter, p.fontMetrics().elidedText(item.subtitle, Qt::ElideRight, int(textWidth)));
                break;
            }
            case Item::Tool: {
                if (item.destructive) {
                    // What ends the session wears the error colour, and its corners
                    // round off under the pointer
                    const double corner = 16 + 8 * hover;
                    p.setBrush(M3::layer(s.error, s.onError, 0.10 * press));
                    p.drawRoundedRect(r, corner, corner);
                    if (selected) {
                        QPainterPath around;
                        around.addRoundedRect(r.adjusted(-4, -4, 4, 4), corner + 4, corner + 4);
                        ring(around);
                    }
                    drawSymbol(r, item.symbol, 24, s.onError);
                } else {
                    const QRectF spot(r.center().x() - 22, r.center().y() - 22, 44, 44);
                    p.setBrush(M3::layer(s.primaryContainer, s.onPrimaryContainer, 0.10 * hover + 0.10 * press));
                    p.drawEllipse(spot);
                    if (selected) {
                        QPainterPath around;
                        around.addEllipse(spot.adjusted(-2, -2, 2, 2));
                        ring(around);
                    }
                    drawSymbol(r, item.symbol, 24, s.onPrimaryContainer);
                }
                break;
            }
            case Item::Chip: {
                if (item.checked) {
                    p.setBrush(M3::layer(s.secondaryContainer, s.onSecondaryContainer, 0.08 * hover + 0.10 * press));
                    p.drawRoundedRect(r, 16, 16);
                    drawSymbol(QRectF(r.left() + 8, r.top(), 20, r.height()), QStringLiteral("check"), 18, s.onSecondaryContainer);
                    p.setFont(flex(13.5, 560));
                    p.setPen(s.onSecondaryContainer);
                    p.drawText(r.adjusted(32, 0, -12, 0), Qt::AlignLeft | Qt::AlignVCenter, item.title);
                } else {
                    p.setBrush(M3::layer(s.surfaceContainer, s.onSurface, 0.08 * hover + 0.10 * press));
                    p.setPen(QPen(s.outlineVariant, 1));
                    p.drawRoundedRect(r.adjusted(0.5, 0.5, -0.5, -0.5), 8, 8);
                    p.setFont(flex(13.5, 520));
                    p.setPen(s.onSurfaceVariant);
                    p.drawText(r, Qt::AlignCenter, item.title);
                }
                if (selected) {
                    QPainterPath around;
                    around.addRoundedRect(r.adjusted(-3, -3, 3, 3), 18, 18);
                    ring(around);
                }
                break;
            }
            case Item::Toggle: {
                p.setBrush(M3::layer(s.secondaryContainer, s.onSecondaryContainer, 0.08 * hover + 0.10 * press));
                p.drawRoundedRect(r, 16, 16);
                drawSymbol(QRectF(r.left() + 12, r.top(), 20, r.height()), item.symbol, 18, s.onSecondaryContainer);
                p.setFont(flex(13.5, 560));
                p.setPen(s.onSecondaryContainer);
                p.drawText(r.adjusted(38, 0, -12, 0), Qt::AlignLeft | Qt::AlignVCenter, item.title);
                if (selected) {
                    QPainterPath around;
                    around.addRoundedRect(r.adjusted(-3, -3, 3, 3), 19, 19);
                    ring(around);
                }
                break;
            }
        }
    }

    // More of the grid above or below: its edge fades
    if (!d->grid.isNull() && d->scrollMax > 0) {
        for (const bool top : {true, false}) {
            if (top ? d->scroll <= 0.5 : d->scroll >= d->scrollMax - 0.5) continue;
            QLinearGradient fade(0, top ? d->grid.top() : d->grid.bottom() - 28, 0, top ? d->grid.top() + 28 : d->grid.bottom());
            QColor clear = s.surfaceContainer;
            clear.setAlphaF(0);
            fade.setColorAt(top ? 0 : 1, s.surfaceContainer);
            fade.setColorAt(top ? 1 : 0, clear);
            p.fillRect(QRectF(d->grid.left(), top ? d->grid.top() : d->grid.bottom() - 28, d->grid.width(), 28), fade);
        }
    }

    // Which keys do what, while searching
    if (!d->hints.isNull()) {
        double x = d->hints.left();
        const QList<QPair<QString, QString>> keys = d->mode == Mode::Run ? QList<QPair<QString, QString>>{{tr("Enter"), tr("run")}, {tr("Tab"), tr("complete")}, {tr("Ctrl Enter"), tr("in a terminal")}, {tr("Esc"), tr("clear")}}
                                                                         : QList<QPair<QString, QString>>{{QStringLiteral("↑ ↓"), tr("move")}, {tr("Enter"), tr("open")}, {tr("Tab"), tr("actions")}, {tr("Esc"), tr("clear")}};
        for (const auto& [key, what] : keys) {
            p.setFont(flex(11.5, 600));
            const double w = p.fontMetrics().horizontalAdvance(key) + 14;
            p.setPen(Qt::NoPen);
            p.setBrush(s.surfaceContainerHighest);
            p.drawRoundedRect(QRectF(x, d->hints.top(), w, 20), 6, 6);
            p.setPen(s.onSurfaceVariant);
            p.drawText(QRectF(x, d->hints.top(), w, 20), Qt::AlignCenter, key);
            p.setFont(flex(11.5, 450));
            p.setPen(s.outline);
            const double whatWidth = p.fontMetrics().horizontalAdvance(what);
            p.drawText(QRectF(x + w + 6, d->hints.top(), whatWidth + 2, 20), Qt::AlignLeft | Qt::AlignVCenter, what);
            x += w + 6 + whatWidth + 18;
        }
    }
}

int Launcher::itemAt(const QPointF& point, int* action) const {
    for (int i = d->items.count() - 1; i >= 0; i--) {
        const Item& item = d->items.at(i);
        QRectF rect = item.rect;
        if (item.kind == Item::Grid) {
            if (!d->grid.contains(point)) continue;
            rect.translate(0, -d->scroll);
        }
        if (!rect.contains(point)) continue;
        if (action) {
            *action = 0;
            for (int a = 0; a < item.actions.count(); a++) {
                if (item.kind == Item::Top && item.actions.at(a).rect.contains(point)) *action = a;
            }
        }
        return i;
    }
    return -1;
}

void Launcher::activate(int item, int action) {
    if (item < 0 || item >= d->items.count()) return;
    // Copied: running it may lay everything out again
    const Item chosen = d->items.at(item);
    if (chosen.kind == Item::Top && action > 0 && action < chosen.actions.count()) {
        if (chosen.actions.at(action).run) chosen.actions.at(action).run();
        return;
    }
    if (chosen.run) chosen.run();
}

void Launcher::select(int item) {
    d->selected = item;
    d->selectedAction = 0;
    // The grid follows what's selected in it
    if (item >= 0 && item < d->items.count() && d->items.at(item).kind == Item::Grid) {
        const QRectF rect = d->items.at(item).rect;
        if (rect.top() - d->scroll < d->grid.top()) d->scroll = rect.top() - d->grid.top();
        if (rect.bottom() - d->scroll > d->grid.bottom()) d->scroll = rect.bottom() - d->grid.bottom();
        d->scroll = qBound(0.0, d->scroll, d->scrollMax);
    }
    this->update();
}

void Launcher::moveSelection(int dx, int dy) {
    if (d->items.isEmpty()) return;
    // Results are a list: up and down walk it
    if (d->mode == Mode::Search || d->mode == Mode::Run) {
        if (dy == 0) return;
        const int count = int(d->items.count());
        select(d->selected < 0 ? (dy > 0 ? 0 : count - 1) : (d->selected + dy + count) % count);
        return;
    }
    // Everything else is laid out in two directions: go to what's nearest that way
    if (d->selected < 0) {
        for (int i = 0; i < d->items.count(); i++) {
            const Item::Kind kind = d->items.at(i).kind;
            if (kind == Item::Pin || kind == Item::Grid) {
                select(i);
                return;
            }
        }
        select(0);
        return;
    }
    const QPointF from = d->items.at(d->selected).rect.center();
    int best = -1;
    double bestCost = 1e12;
    for (int i = 0; i < d->items.count(); i++) {
        if (i == d->selected) continue;
        const QPointF to = d->items.at(i).rect.center();
        const double along = dx != 0 ? (to.x() - from.x()) * dx : (to.y() - from.y()) * dy;
        const double across = dx != 0 ? qAbs(to.y() - from.y()) : qAbs(to.x() - from.x());
        if (along < 6) continue;
        // Sideways stays in its row
        if (dx != 0 && across > 30) continue;
        const double cost = along + across * 2.5;
        if (cost < bestCost) {
            bestCost = cost;
            best = i;
        }
    }
    if (best >= 0) select(best);
}

void Launcher::mouseMoveEvent(QMouseEvent* event) {
    const int item = itemAt(event->position());
    if (item != d->hovered) {
        d->hovered = item;
        if (!d->frames.isActive()) {
            d->lastFrame = d->clock.elapsed();
            d->frames.start();
        }
    }
    this->setCursor(item >= 0 ? Qt::PointingHandCursor : Qt::ArrowCursor);
}

void Launcher::leaveEvent(QEvent* event) {
    Q_UNUSED(event)
    d->hovered = -1;
    if (!d->frames.isActive()) {
        d->lastFrame = d->clock.elapsed();
        d->frames.start();
    }
}

void Launcher::mousePressEvent(QMouseEvent* event) {
    if (event->button() != Qt::LeftButton) return;
    d->pressed = itemAt(event->position(), &d->pressedAction);
    // Beside the bar and the panel is outside the Gateway
    if (d->pressed < 0 && !d->panel.contains(event->position()) && event->position().y() > BarHeight) {
        emit closeRequested();
        return;
    }
    if (!d->frames.isActive()) {
        d->lastFrame = d->clock.elapsed();
        d->frames.start();
    }
}

void Launcher::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() != Qt::LeftButton) return;
    int action = 0;
    const int item = itemAt(event->position(), &action);
    const int pressed = d->pressed;
    d->pressed = -1;
    if (!d->frames.isActive()) {
        d->lastFrame = d->clock.elapsed();
        d->frames.start();
    }
    if (item >= 0 && item == pressed) activate(item, action);
}

void Launcher::wheelEvent(QWheelEvent* event) {
    if (d->grid.isNull() || d->scrollMax <= 0) return;
    d->scroll = qBound(0.0, d->scroll - event->angleDelta().y() / 120.0 * GridRow, d->scrollMax);
    d->hovered = itemAt(event->position());
    this->update();
}

void Launcher::contextMenuEvent(QContextMenuEvent* event) {
    const int index = itemAt(event->pos());
    if (index < 0 || !d->items.at(index).app) return;
    const LauncherApp* app = d->items.at(index).app;
    LauncherData* data = LauncherData::instance();
    auto* menu = new QMenu(this);
    connect(menu, &QMenu::aboutToHide, menu, &QObject::deleteLater);
    const QString id = app->id;
    if (data->hasPins() && data->isPinned(id)) {
        menu->addAction(tr("Unpin"), this, [id] {
            LauncherData::instance()->setPinned(id, false);
        });
    } else {
        menu->addAction(tr("Pin"), this, [id] {
            LauncherData::instance()->setPinned(id, true);
        });
    }
    if (!app->actions.isEmpty()) menu->addSeparator();
    for (const QString& action : app->actions) {
        const QString label = app->app->getActionProperty(action, "Name").toString();
        if (label.isEmpty()) continue;
        menu->addAction(label, this, [this, id, action] {
            LauncherData::instance()->launch(LauncherData::instance()->app(id), action);
            emit closeRequested();
        });
    }
    menu->popup(event->globalPos());
}

bool Launcher::event(QEvent* event) {
    if (event->type() == QEvent::ToolTip) {
        auto* help = static_cast<QHelpEvent*>(event);
        const int item = itemAt(help->pos());
        if (item >= 0 && !d->items.at(item).tip.isEmpty()) {
            QToolTip::showText(help->globalPos(), d->items.at(item).tip, this);
        } else {
            QToolTip::hideText();
        }
        return true;
    }
    // The horizon moved the colours: pictures made in the old ones go
    if (event->type() == QEvent::ApplicationPaletteChange || event->type() == QEvent::PaletteChange) d->glyphs.clear();
    return QWidget::event(event);
}

bool Launcher::eventFilter(QObject* watched, QEvent* event) {
    if (watched != d->edit || event->type() != QEvent::KeyPress) return false;
    auto* key = static_cast<QKeyEvent*>(event);
    const bool empty = d->edit->text().isEmpty();
    switch (key->key()) {
        case Qt::Key_Escape:
            if (!empty) d->edit->clear();
            else if (d->mode == Mode::AllApps) {
                d->mode = Mode::Home;
                this->relayout();
            } else emit closeRequested();
            return true;
        case Qt::Key_Down:
            moveSelection(0, 1);
            return true;
        case Qt::Key_Up:
            moveSelection(0, -1);
            return true;
        case Qt::Key_Left:
        case Qt::Key_Right:
            // While there's text the arrows belong to it
            if (!empty) return false;
            moveSelection(key->key() == Qt::Key_Right ? 1 : -1, 0);
            return true;
        case Qt::Key_Backspace:
            // The Run chip goes like an input chip: backspace at the start
            if (d->mode == Mode::Run && empty) {
                leaveRun();
                return true;
            }
            return false;
        case Qt::Key_Tab:
        case Qt::Key_Backtab: {
            // In Run mode Tab takes the completion
            if (d->mode == Mode::Run && key->key() == Qt::Key_Tab && !d->runBar->ghost.isEmpty() && d->edit->cursorPosition() == d->edit->text().size()) {
                d->edit->setText(d->edit->text() + d->runBar->ghost);
                return true;
            }
            // Through the selected result's actions; never out of the field
            if (d->selected >= 0 && d->selected < d->items.count()) {
                const int count = int(d->items.at(d->selected).actions.count());
                if (d->items.at(d->selected).kind == Item::Top && count > 1) {
                    d->selectedAction = (d->selectedAction + (key->key() == Qt::Key_Tab ? 1 : count - 1)) % count;
                    this->update();
                }
            }
            return true;
        }
        case Qt::Key_Return:
        case Qt::Key_Enter:
            if (d->mode == Mode::Run && key->modifiers() & Qt::ControlModifier) {
                if (d->selected >= 0) runCommand(d->items.at(d->selected).title, true);
                return true;
            }
            if (d->selected >= 0) activate(d->selected, d->selectedAction);
            return true;
        case Qt::Key_PageDown:
        case Qt::Key_PageUp:
            if (!d->grid.isNull()) {
                d->scroll = qBound(0.0, d->scroll + (key->key() == Qt::Key_PageDown ? 1 : -1) * d->grid.height(), d->scrollMax);
                this->update();
                return true;
            }
            return false;
        default:
            return false;
    }
}
