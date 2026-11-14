#include "claudebrain.h"
#include "pebblerules.h"
#include "docsearch.h"
#include <QThread>
#include <QDirIterator>
#include <QElapsedTimer>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTimeZone>
#include <QUrlQuery>
#include <tsettings.h>
#include <QLocale>
#include <QStorageInfo>

#include <QBuffer>
#include <QDateTime>
#include <QFile>
#include <Wm/desktopwm.h>
#include <assistanttools.h>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QRandomGenerator>
#include <QSet>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>

namespace {
    constexpr qint64 StayDeepMs = 5 * 60 * 1000; // once a thread went deep it stays a while

    // depth 0: quick; 1: bigger model; 2: bigger model, thinking hardest
    const char* modelFor(int depth) { return depth == 0 ? "haiku" : "sonnet"; }
    const char* effortFor(int depth) { return depth == 0 ? "low" : depth == 1 ? "medium" : "high"; }

    QString shortPath(QString path) {
        const QString home = QDir::homePath();
        if (path.startsWith(home)) path.replace(0, home.size(), QStringLiteral("~"));
        return path;
    }
    QString fileName(const QString& path) { return QFileInfo(path).fileName(); }
    QString clip(QString s, int n) {
        s = s.simplified();
        return s.size() > n ? s.left(n).trimmed() : s; // step lines add their own "…"
    }

    // Where a file goes before Pebble changes it (Pebble can't read this folder)
    QString backupOf(const QString& path) {
        static int n = 0;
        const QString dir = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/thedesk/assistant/backups/%1-%2").arg(QDateTime::currentMSecsSinceEpoch()).arg(n++);
        QDir().mkpath(dir);
        const QString copy = dir + '/' + QFileInfo(path).fileName();
        QFile::copy(path, copy);
        return copy;
    }
    void pruneBackups() {
        // shortcut: backups go after a week by age alone, not by size; cap the size if they ever grow big
        QDir dir(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/thedesk/assistant/backups"));
        for (const QFileInfo& old : dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot)) {
            if (old.birthTime().isValid() ? old.birthTime().daysTo(QDateTime::currentDateTime()) > 7 : old.lastModified().daysTo(QDateTime::currentDateTime()) > 7) QDir(old.absoluteFilePath()).removeRecursively();
        }
    }

    QString memoryPath() {
        return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/thedesk/assistant-memory.md");
    }
    QStringList memories() {
        QFile file(memoryPath());
        if (!file.open(QFile::ReadOnly)) return {};
        QStringList lines;
        for (const QString& line : QString::fromUtf8(file.readAll()).split('\n')) {
            if (line.startsWith(QStringLiteral("- "))) lines.append(line.mid(2));
        }
        return lines;
    }
    void saveMemories(const QStringList& lines) {
        QFile file(memoryPath());
        if (!file.open(QFile::WriteOnly | QFile::Truncate)) return;
        QString out = QStringLiteral("# What Pebble remembers (delete a line to make it forget)\n");
        for (const QString& line : lines) out += QStringLiteral("- ") + line + '\n';
        file.write(out.toUtf8());
    }

    // The brain's own tools (the rest come from AssistantTools)
    struct Inner {
            QString name, description;
            QJsonObject schema;
            bool asks;
    };
    QList<Inner> innerTools() {
        return {
            {"think_harder", "Call once when the job needs careful thinking, code changes, several files or long writing. A deeper mode takes over and you carry on.", AssistantTools::schemaOf({}), false},
            {"run_command", "Run a shell command (bash) in the user's home. The user is asked first every time. Prefer the desk tools when one fits.", AssistantTools::schemaOf({{"command", "string"}, {"why", "string: a few words, shown to the user"}}), true},
            {"look_at_screen", "See what was on the user's screen when they opened you. Use it when they say \"this\", \"here\" or ask about something on screen.", AssistantTools::schemaOf({}), false},
            {"remember", "Remember one short fact about the user for later chats (they can see and delete these).", AssistantTools::schemaOf({{"fact", "string"}}), false},
            {"forget", "Forget remembered facts containing these words.", AssistantTools::schemaOf({{"words", "string"}}), false},
            {"undo_last", "Undo what you changed in your last answer: desk settings, file edits, files moved or put in the trash.", AssistantTools::schemaOf({}), false},
            {"trash", "Put files or folders in the trash (never delete them for good). They can be restored with undo_last.", AssistantTools::schemaOf({{"paths", "array: full paths"}}), true},
            {"search_documents", "Find documents by what's inside them (PDFs, Word and LibreOffice files, text), newest first. words: what must appear, space separated. folder: optional, else Documents, Downloads and Desktop.", AssistantTools::schemaOf({{"words", "string"}, {"folder", "string: full path"}}, {"folder"}), false},
            {"weather", "The weather now and for the next hours, shown as a card. place: optional, a city name; without it, where the user is.", AssistantTools::schemaOf({{"place", "string"}}, {"place"}), false},
            {"disk_space", "How full the disk is and which folders in the home folder take the most space, shown as cards. Use it for any question about disk space or storage instead of df or du.", AssistantTools::schemaOf({}), false},
            {"move", "Move or rename a file or folder. Won't replace an existing one. Can be undone with undo_last.", AssistantTools::schemaOf({{"from", "string: full path"}, {"to", "string: full path"}}), true},
        };
    }

    // Pebble's system prompt lives in a file the user can edit; the first run
    // writes the default there. {name} {home} {date} {memories} get filled in
    QString systemPrompt() {
        const QString path = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + QStringLiteral("/theSuite/pebble-prompt.md");
        if (!QFile::exists(path)) {
            QDir().mkpath(QFileInfo(path).absolutePath());
            QFile::copy(QStringLiteral(":/thedesk/desktop/assistant/pebble-prompt.md"), path);
            QFile::setPermissions(path, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ReadGroup | QFileDevice::ReadOther);
        }
        QFile file(path);
        if (!file.open(QFile::ReadOnly)) file.setFileName(QStringLiteral(":/thedesk/desktop/assistant/pebble-prompt.md"));
        if (!file.isOpen() && !file.open(QFile::ReadOnly)) return {};
        QString prompt = QString::fromUtf8(file.readAll());
        const QString full = qEnvironmentVariableIsSet("TD_DISPLAY_NAME") ? qEnvironmentVariable("TD_DISPLAY_NAME") : DesktopWm::userDisplayName();
        prompt.replace(QStringLiteral("{name}"), full.section(' ', 0, 0).isEmpty() ? QStringLiteral("unknown") : full.section(' ', 0, 0));
        prompt.replace(QStringLiteral("{home}"), QDir::homePath());
        prompt.replace(QStringLiteral("{date}"), QDate::currentDate().toString(Qt::ISODate));
        prompt.replace(QStringLiteral("{memories}"), memories().isEmpty() ? QString() : QStringLiteral("What you remember about the user:\n- ") + memories().join(QStringLiteral("\n- ")));
        return prompt;
    }
} // namespace

struct ClaudeBrain::Private {
        QProcess* process = nullptr;
        QByteArray buffer;
        int nextRequest = 0;
        int depth = 0;
        qint64 deepUntil = 0;
        bool busy = false, sawText = false;
        QImage screen;
        QHash<QString, QList<bool>> mcpCards;                 // desk tool -> made a card, per call, in order
        QHash<QString, std::function<void(double)>> setters;  // card id -> its control
        QList<std::function<void()>> undos, lastUndos;        // this answer's, the one before
        QString pending;                         // text held back until its markup closes
        QHash<QString, QPair<QString, QJsonObject>> tools; // tool use id -> tool, input
        QHash<QString, QJsonObject> asks;        // permission request id -> input
        QSet<QString> asked;                     // tool use ids the user saw as an ask card
        // Once the chat has read a web page, a download or a long paste,
        // everything asks again: text there can carry instructions
        bool tainted = false;
        bool chipsThisTurn = false;
        QString deepStep;
};

ClaudeBrain::ClaudeBrain(QObject* parent) :
    AssistantBrain(parent) {
    d = new Private();
}

ClaudeBrain::~ClaudeBrain() {
    if (d->process) {
        d->process->kill();
        d->process->waitForFinished(500);
    }
    delete d;
}

bool ClaudeBrain::looksHeavy(const QString& text, const QStringList& context) {
    static const QRegularExpression heavy(QStringLiteral(
        "\\b(code|css|html|js|javascript|typescript|python|script|function|bug|compile|build|refactor|fix|debug|commit|git|zed|"
        "write|draft|essay|email|letter|summari[sz]e|translate|explain in detail|files|folder|rename|organi[sz]e|tidy|clean up)\\b"),
        QRegularExpression::CaseInsensitiveOption);
    if (text.size() > 400 || heavy.match(text).hasMatch()) return true;
    for (const QString& line : context) {
        if (line.startsWith(QStringLiteral("Active window:")) && line.contains(QStringLiteral("Zed"))) return true;
    }
    return false;
}

void ClaudeBrain::start() {
    if (d->process && d->process->state() != QProcess::NotRunning) return;
    pruneBackups();
    delete d->process;
    d->process = new QProcess(this);
    d->buffer.clear();
    d->depth = 0;

    const QString home = QDir::homePath();
    QString claude = QStandardPaths::findExecutable(QStringLiteral("claude"));
    if (claude.isEmpty()) claude = QStandardPaths::findExecutable(QStringLiteral("claude"), {home + "/.local/bin", home + "/.claude/local"});
    if (claude.isEmpty()) {
        fail(tr("I can't find Claude Code on this computer, so I can't think yet."));
        return;
    }
    // Its own folder, so no project's notes leak in; the home folder is reachable
    const QString place = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/thedesk/assistant");
    QDir().mkpath(place);
    d->process->setWorkingDirectory(place);
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.remove(QStringLiteral("CLAUDECODE"));
    env.remove(QStringLiteral("CLAUDE_CODE_ENTRYPOINT"));
    d->process->setProcessEnvironment(env);

    const QString mcp = QString::fromUtf8(QJsonDocument(QJsonObject{{"mcpServers", QJsonObject{{"desk", QJsonObject{{"type", "sdk"}, {"name", "desk"}}}}}}).toJson(QJsonDocument::Compact));
    QStringList tools{"Read", "Glob", "Grep", "Edit", "Write", "WebSearch", "WebFetch"};
    QStringList allowed{"Read", "Glob", "Grep", "WebSearch", "WebFetch"}; // the web never asks (the user chose that)
    for (const Inner& tool : innerTools()) {
        tools.append(QStringLiteral("mcp__desk__") + tool.name);
        if (!tool.asks) allowed.append(QStringLiteral("mcp__desk__") + tool.name);
    }
    for (const AssistantTool& tool : AssistantTools::instance()->tools()) {
        tools.append(QStringLiteral("mcp__desk__") + tool.name);
        if (!tool.asks) allowed.append(QStringLiteral("mcp__desk__") + tool.name);
    }
    QStringList denied;
    for (const QString& folder : PrivateFolders) {
        for (const char* tool : {"Read", "Edit", "Write"}) denied.append(QStringLiteral("%1(%2/%3/**)").arg(tool, home, folder));
    }
    for (const char* tool : {"Read", "Edit", "Write"}) denied.append(QStringLiteral("%1(**/.env*)").arg(tool));
    QStringList args{
        "-p", "--input-format", "stream-json", "--output-format", "stream-json", "--verbose", "--include-partial-messages",
        "--model", modelFor(0), "--effort", effortFor(0),
        "--setting-sources", "", "--strict-mcp-config", "--mcp-config", mcp,
        "--disable-slash-commands", "--no-session-persistence", "--permission-prompt-tool", "stdio",
        "--add-dir", home,
        "--system-prompt", systemPrompt(), // Pebble's alone, instead of Claude Code's coding one
    };
    args << "--tools" << tools << "--allowedTools" << allowed << "--disallowedTools" << denied;

    connect(d->process, &QProcess::readyReadStandardOutput, this, [this] {
        d->buffer += d->process->readAllStandardOutput();
        int nl;
        while ((nl = int(d->buffer.indexOf('\n'))) >= 0) {
            const QByteArray line = d->buffer.left(nl);
            d->buffer.remove(0, nl + 1);
            if (!line.trimmed().isEmpty()) handleLine(line);
        }
    });
    connect(d->process, &QProcess::finished, this, [this] {
        if (d->busy) fail(tr("Lost my train of thought. Try again?"));
    });
    connect(d->process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) fail(tr("I couldn't start my brain (Claude Code). Is it installed and logged in?"));
    });
    d->process->start(claude, args);
    write(QJsonObject{{"type", "control_request"}, {"request_id", "init"}, {"request", QJsonObject{{"subtype", "initialize"}, {"hooks", QJsonValue::Null}}}});
}

void ClaudeBrain::write(const QJsonObject& object) {
    if (!d->process || d->process->state() == QProcess::NotRunning) return;
    d->process->write(QJsonDocument(object).toJson(QJsonDocument::Compact) + '\n');
}

void ClaudeBrain::warm() {
    start();
}

void ClaudeBrain::setDepth(int depth) {
    if (depth == d->depth) return;
    const bool wasDeep = d->depth > 0;
    d->depth = depth;
    write(QJsonObject{{"type", "control_request"}, {"request_id", QStringLiteral("m%1").arg(d->nextRequest++)}, {"request", QJsonObject{{"subtype", "set_model"}, {"model", modelFor(depth)}}}});
    write(QJsonObject{{"type", "control_request"}, {"request_id", QStringLiteral("e%1").arg(d->nextRequest++)}, {"request", QJsonObject{{"subtype", "apply_flag_settings"}, {"settings", QJsonObject{{"effortLevel", effortFor(depth)}}}}}});
    if ((depth > 0) != wasDeep) emit deepChanged(depth > 0);
}

void ClaudeBrain::deepen() {
    // what the panel says when it switches: never which model
    static const QStringList phrases{tr("Thinking harder…"), tr("Taking a closer look…"), tr("Digging in…")};
    d->deepStep = QStringLiteral("deep%1").arg(d->nextRequest++);
    emit stepStarted(d->deepStep, phrases.at(QRandomGenerator::global()->bounded(int(phrases.size()))));
}

void ClaudeBrain::send(QString text, QStringList context, bool harder) {
    start();
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    int depth = 0;
    if (harder) depth = 2;
    else if (looksHeavy(text, context) || now < d->deepUntil) depth = 1;
    const bool deeper = depth > d->depth;
    setDepth(depth);
    if (depth > 0) d->deepUntil = now + StayDeepMs;
    d->busy = true;
    d->sawText = false;
    d->pending.clear();
    if (!d->undos.isEmpty()) d->lastUndos = d->undos;
    d->undos.clear();
    if (deeper) deepen();

    d->chipsThisTurn = false;
    // a long paste (or long copied/selected text) may come from a web page
    if (text.size() > 1500) d->tainted = true;
    for (const QString& line : context) {
        if ((line.startsWith(QStringLiteral("Clipboard:")) || line.startsWith(QStringLiteral("Selected text:"))) && line.size() > 300) d->tainted = true;
    }

    QString content = text;
    if (!context.isEmpty()) content = QStringLiteral("<context>\n") + context.join(QStringLiteral("\n\n")) + QStringLiteral("\n</context>\n\n") + text;
    write(QJsonObject{{"type", "user"}, {"message", QJsonObject{{"role", "user"}, {"content", content}}}});
}

bool ClaudeBrain::chip(QString chipId) {
    Q_UNUSED(chipId)
    return false; // the panel sends the chip's words as a message
}

void ClaudeBrain::setCardValue(QString cardId, double value) {
    if (d->setters.contains(cardId)) d->setters.value(cardId)(value);
}

void ClaudeBrain::setScreen(const QImage& screen) {
    d->screen = screen;
}

void ClaudeBrain::answer(QString askId, bool allow) {
    if (!d->asks.contains(askId)) return;
    const QJsonObject input = d->asks.take(askId);
    const QJsonObject response = allow ? QJsonObject{{"behavior", "allow"}, {"updatedInput", input}} : QJsonObject{{"behavior", "deny"}, {"message", "The user said no. Don't try again unless they ask."}};
    write(QJsonObject{{"type", "control_response"}, {"response", QJsonObject{{"subtype", "success"}, {"request_id", askId}, {"response", response}}}});
}

void ClaudeBrain::stop() {
    if (!d->busy) return;
    write(QJsonObject{{"type", "control_request"}, {"request_id", QStringLiteral("x%1").arg(d->nextRequest++)}, {"request", QJsonObject{{"subtype", "interrupt"}}}});
}

void ClaudeBrain::forget() {
    if (d->process) d->process->kill();
    d->busy = false;
    d->deepUntil = 0;
    d->tools.clear();
    d->asks.clear();
    d->asked.clear();
    d->setters.clear();
    d->undos.clear();
    d->lastUndos.clear();
    d->tainted = false;
}

void ClaudeBrain::fail(const QString& why) {
    takeText(QString(), true);
    emit textArrived(QStringLiteral("{oops} ") + why);
    d->busy = false;
    emit turnFinished();
}

void ClaudeBrain::takeText(const QString& text, bool final) {
    // Code blocks become code cards and the chips line becomes chips; text
    // waits while one of those is still open
    d->pending += text;
    auto emitText = [this](const QString& part) {
        if (part.isEmpty()) return;
        if (!d->sawText) {
            d->sawText = true;
            if (!d->deepStep.isEmpty()) emit stepFinished(d->deepStep, QString(), true);
            d->deepStep.clear();
        }
        emit textArrived(part);
    };
    while (true) {
        const int fence = int(d->pending.indexOf(QStringLiteral("```")));
        const int chips = int(d->pending.indexOf(QStringLiteral("[[chips:")));
        const int first = fence < 0 ? chips : chips < 0 ? fence : qMin(fence, chips);
        if (first < 0) break;
        emitText(d->pending.left(first));
        d->pending.remove(0, first);
        if (first == fence) {
            const int close = int(d->pending.indexOf(QStringLiteral("```"), 3));
            if (close < 0) {
                if (!final) return;
                d->pending.append(QStringLiteral("\n```"));
                continue;
            }
            QStringList lines = d->pending.mid(3, close - 3).split('\n');
            const QString language = lines.isEmpty() ? QString() : lines.takeFirst().trimmed();
            while (!lines.isEmpty() && lines.last().trimmed().isEmpty()) lines.removeLast();
            QStringList marked;
            for (const QString& line : std::as_const(lines)) marked.append(QStringLiteral(" ") + line);
            emit cardAdded({QStringLiteral("code%1").arg(d->nextRequest++), AssistantCard::Code, language, {}, 0, language, marked});
            d->pending.remove(0, close + 3);
        } else {
            const int close = int(d->pending.indexOf(QStringLiteral("]]")));
            if (close < 0) {
                if (!final) return;
                d->pending.clear();
                break;
            }
            QList<AssistantChip> offered;
            for (const QString& label : d->pending.mid(8, close - 8).split('|')) {
                if (!label.trimmed().isEmpty()) offered.append({label.trimmed(), label.trimmed(), {}});
            }
            if (!offered.isEmpty()) {
                d->chipsThisTurn = true;
                emit chipsOffered(offered);
            }
            d->pending.remove(0, close + 2);
        }
    }
    if (final) {
        emitText(d->pending.trimmed().isEmpty() ? QString() : d->pending);
        d->pending.clear();
        return;
    }
    // hold back an end that could be the start of a fence or the chips line
    int hold = 0;
    for (const QString& marker : {QStringLiteral("```"), QStringLiteral("[[chips:")}) {
        for (int n = qMin(int(marker.size()) - 1, int(d->pending.size())); n > hold; n--) {
            if (d->pending.endsWith(marker.left(n))) {
                hold = n;
                break;
            }
        }
    }
    emitText(d->pending.left(d->pending.size() - hold));
    d->pending = d->pending.right(hold);
}

QString ClaudeBrain::stepText(const QString& tool, const QJsonObject& input, bool done) const {
    const QString path = input.value("file_path").toString(input.value("path").toString());
    if (tool == "Read") return done ? tr("Read %1").arg(fileName(path)) : tr("Reading %1…").arg(fileName(path));
    if (tool == "Glob" || tool == "Grep") {
        const QString what = clip(input.value("pattern").toString(), 28);
        return done ? tr("Looked for %1").arg(what) : tr("Looking for %1…").arg(what);
    }
    if (tool == "Bash") {
        QString what = clip(input.value("description").toString(input.value("command").toString()), 34);
        while (what.endsWith('.') || what.endsWith(QStringLiteral("…"))) what.chop(1);
        return done ? what : what + QStringLiteral("…");
    }
    if (tool == "Edit") return done ? tr("Edited %1").arg(fileName(path)) : tr("Editing %1…").arg(fileName(path));
    if (tool == "Write") return done ? tr("Wrote %1").arg(fileName(path)) : tr("Writing %1…").arg(fileName(path));
    if (tool == "WebSearch") {
        const QString what = clip(input.value("query").toString(), 30);
        return done ? tr("Searched %1").arg(what) : tr("Searching %1…").arg(what);
    }
    if (tool == "WebFetch") {
        const QString host = QUrl(input.value("url").toString()).host();
        return done ? tr("Read %1").arg(host) : tr("Reading %1…").arg(host);
    }
    if (tool.startsWith(QStringLiteral("mcp__desk__"))) {
        const QString name = tool.mid(11);
        static const QHash<QString, QPair<const char*, const char*>> words{
            {"set_brightness", {QT_TR_NOOP("Setting brightness"), QT_TR_NOOP("Brightness")}},
            {"set_volume", {QT_TR_NOOP("Setting the volume"), QT_TR_NOOP("Volume")}},
            {"set_muted", {QT_TR_NOOP("Muting"), QT_TR_NOOP("Sound")}},
            {"set_do_not_disturb", {QT_TR_NOOP("Setting do not disturb"), QT_TR_NOOP("Do not disturb")}},
            {"set_keyboard_layout", {QT_TR_NOOP("Switching layout"), QT_TR_NOOP("Switched layout")}},
            {"open_app", {QT_TR_NOOP("Opening %1"), QT_TR_NOOP("Opened %1")}},
            {"focus_window", {QT_TR_NOOP("Finding %1"), QT_TR_NOOP("Found %1")}},
            {"close_window", {QT_TR_NOOP("Closing %1"), QT_TR_NOOP("Closed %1")}},
            {"show_image", {QT_TR_NOOP("Getting the picture"), QT_TR_NOOP("Picture")}},
            {"run_command", {QT_TR_NOOP("Running a command"), QT_TR_NOOP("Ran a command")}},
            {"look_at_screen", {QT_TR_NOOP("Looking at your screen"), QT_TR_NOOP("Looked at your screen")}},
            {"remember", {QT_TR_NOOP("Remembering"), QT_TR_NOOP("Remembered")}},
            {"forget", {QT_TR_NOOP("Forgetting"), QT_TR_NOOP("Forgot")}},
            {"undo_last", {QT_TR_NOOP("Undoing"), QT_TR_NOOP("Undone")}},
            {"desk_status", {QT_TR_NOOP("Checking the desk"), QT_TR_NOOP("Checked the desk")}},
            {"battery_status", {QT_TR_NOOP("Checking the battery"), QT_TR_NOOP("Checked the battery")}},
            {"recent_notifications", {QT_TR_NOOP("Checking notifications"), QT_TR_NOOP("Checked notifications")}},
            {"trash", {QT_TR_NOOP("Putting it in the trash"), QT_TR_NOOP("In the trash")}},
            {"move", {QT_TR_NOOP("Moving it"), QT_TR_NOOP("Moved it")}},
            {"search_documents", {QT_TR_NOOP("Searching your documents"), QT_TR_NOOP("Searched your documents")}},
            {"disk_space", {QT_TR_NOOP("Measuring your disk"), QT_TR_NOOP("Measured your disk")}},
            {"weather", {QT_TR_NOOP("Checking the weather"), QT_TR_NOOP("Checked the weather")}},
            {"clipboard_history", {QT_TR_NOOP("Looking through what you copied"), QT_TR_NOOP("Looked through what you copied")}},
            {"quick_switch", {QT_TR_NOOP("Flipping %1"), QT_TR_NOOP("Flipped %1")}},
            {"set_theme", {QT_TR_NOOP("Changing the theme"), QT_TR_NOOP("Changed the theme")}},
            {"copy_to_clipboard", {QT_TR_NOOP("Copying"), QT_TR_NOOP("Copied")}},
            {"remind_me", {QT_TR_NOOP("Setting a reminder"), QT_TR_NOOP("Reminder set")}},
            {"media", {QT_TR_NOOP("Talking to the player"), QT_TR_NOOP("Player")}},
            {"window_layout", {QT_TR_NOOP("Arranging windows"), QT_TR_NOOP("Arranged windows")}},
            {"watch", {QT_TR_NOOP("Setting up a watch"), QT_TR_NOOP("Watching")}},
            {"whats_slow", {QT_TR_NOOP("Seeing what's busy"), QT_TR_NOOP("Saw what's busy")}},
        };
        const QString subject = input.value("name").toString(input.value("window").toString());
        if (words.contains(name)) {
            QString line = tr(done ? words.value(name).second : words.value(name).first);
            if (line.contains(QStringLiteral("%1"))) line = line.arg(clip(subject, 24));
            return done ? line : line + QStringLiteral("…");
        }
    }
    return done ? tr("Done") : tr("Working…");
}

void ClaudeBrain::handleLine(const QByteArray& line) {
    // TD_ASSISTANT_LOG=<file>: everything the brain says, for debugging
    static const QString log = qEnvironmentVariable("TD_ASSISTANT_LOG");
    if (!log.isEmpty()) {
        QFile file(log);
        if (file.open(QFile::Append)) file.write(line + '\n');
    }
    const QJsonObject message = QJsonDocument::fromJson(line).object();
    const QString type = message.value("type").toString();
    if (type == "stream_event") {
        const QJsonObject event = message.value("event").toObject();
        if (event.value("type") == "content_block_delta") {
            const QJsonObject delta = event.value("delta").toObject();
            if (delta.value("type") == "text_delta") takeText(delta.value("text").toString(), false);
        }
    } else if (type == "assistant") {
        for (const QJsonValue& block : message.value("message").toObject().value("content").toArray()) {
            const QJsonObject b = block.toObject();
            if (b.value("type") != "tool_use") continue;
            const QString tool = b.value("name").toString();
            if (tool.startsWith(QStringLiteral("mcp__desk__think_harder"))) continue; // shows as "Thinking harder…"
            // text before a tool ends that part of the answer
            takeText(QString(), true);
            d->tools.insert(b.value("id").toString(), {tool, b.value("input").toObject()});
            // reading the web or a downloaded file: from here on, everything asks
            const QString path = b.value("input").toObject().value("file_path").toString() + b.value("input").toObject().value("path").toString();
            const bool downloaded = path.startsWith(QDir::homePath() + QStringLiteral("/Downloads")) || path.startsWith(QStringLiteral("/tmp"));
            if (tool == "WebFetch" || tool == "WebSearch" || tool == "mcp__desk__search_documents" || tool == "mcp__desk__clipboard_history" || ((tool == "Read" || tool == "Grep") && downloaded)) d->tainted = true;
            emit stepStarted(b.value("id").toString(), stepText(tool, b.value("input").toObject(), false));
        }
    } else if (type == "user") {
        const QJsonValue content = message.value("message").toObject().value("content");
        for (const QJsonValue& block : content.toArray()) {
            const QJsonObject b = block.toObject();
            if (b.value("type") != "tool_result") continue;
            const QString id = b.value("tool_use_id").toString();
            if (!d->tools.contains(id)) continue;
            const auto [tool, input] = d->tools.take(id);
            const bool failed = b.value("is_error").toBool();
            bool card = false;
            if (!failed && !d->asked.contains(id) && tool == "Edit") {
                // what changed, as a code card
                QStringList lines;
                for (const QString& l : input.value("old_string").toString().split('\n')) lines.append(QStringLiteral("-") + l);
                for (const QString& l : input.value("new_string").toString().split('\n')) lines.append(QStringLiteral("+") + l);
                emit cardAdded({id, AssistantCard::Code, fileName(input.value("file_path").toString()), {}, 0, input.value("file_path").toString(), lines});
                card = true;
            } else if (!failed && tool == "Write") {
                emit cardAdded({id, AssistantCard::File, fileName(input.value("file_path").toString()), tr("%1 · written just now").arg(shortPath(QFileInfo(input.value("file_path").toString()).absolutePath())), 0, input.value("file_path").toString(), {}});
                card = true;
            }
            if (tool.startsWith(QStringLiteral("mcp__desk__"))) {
                QList<bool>& made = d->mcpCards[tool.mid(11)];
                if (!made.isEmpty()) card = made.takeFirst();
            }
            emit stepFinished(id, failed ? tr("Skipped: %1").arg(stepText(tool, input, true)) : stepText(tool, input, true), card);
        }
    } else if (type == "result") {
        takeText(QString(), true);
        // it changed things on its own: one tap puts them back
        if (!d->undos.isEmpty() && !d->chipsThisTurn) emit chipsOffered({{QStringLiteral("undo"), tr("Undo"), QStringLiteral("undo")}});
        if (message.value("is_error").toBool() && !d->sawText) emit textArrived(QStringLiteral("{oops} ") + tr("Something went wrong on my side. Try again?"));
        d->busy = false;
        d->deepStep.clear();
        emit turnFinished();
    } else if (type == "control_request") {
        handleControl(message);
    }
}

void ClaudeBrain::handleControl(const QJsonObject& message) {
    const QString requestId = message.value("request_id").toString();
    const QJsonObject request = message.value("request").toObject();
    const QString subtype = request.value("subtype").toString();
    if (subtype == "mcp_message") {
        const QJsonObject mcp = request.value("message").toObject();
        const QJsonObject params = mcp.value("params").toObject();
        if (mcp.value("method") == "tools/call" && params.value("name") == "search_documents") {
            // reading every document takes a while: off the desktop's thread
            const QJsonObject args = params.value("arguments").toObject();
            const QStringList words = args.value("words").toString().split(' ', Qt::SkipEmptyParts);
            QStringList folders;
            if (!args.value("folder").toString().isEmpty() && !args.value("folder").toString().contains(QStringLiteral("/."))) folders.append(args.value("folder").toString());
            else for (const char* folder : {"Documents", "Downloads", "Desktop"}) folders.append(QDir::homePath() + '/' + folder);
            auto* found = new QString();
            QThread* worker = QThread::create([found, words, folders] {
                *found = DocSearch::find(words, folders);
            });
            connect(worker, &QThread::finished, this, [this, worker, found, mcp, requestId] {
                const QJsonObject reply{{"jsonrpc", "2.0"}, {"id", mcp.value("id")}, {"result", QJsonObject{{"content", QJsonArray{QJsonObject{{"type", "text"}, {"text", *found}}}}}}};
                // what it found, as tiles with a look inside
                QStringList paths;
                for (const QString& line : found->split('\n')) {
                    const QString path = line.section(QStringLiteral(" · "), 0, 0);
                    if (path.startsWith('/') && QFileInfo::exists(path) && paths.size() < 6) paths.append(path);
                }
                if (!paths.isEmpty()) {
                    takeText(QString(), true);
                    emit cardAdded({QStringLiteral("files%1").arg(d->nextRequest++), AssistantCard::Files, tr("Documents"), {}, 0, {}, paths});
                }
                d->mcpCards["search_documents"].append(!paths.isEmpty());
                write(QJsonObject{{"type", "control_response"}, {"response", QJsonObject{{"subtype", "success"}, {"request_id", requestId}, {"response", QJsonObject{{"mcp_response", reply}}}}}});
                delete found;
                worker->deleteLater();
            });
            worker->start();
            return;
        }
        if (mcp.value("method") == "tools/call" && params.value("name") == "weather") {
            // Open-Meteo (no key): where the user is comes from their time zone's city, unless a place is named
            // a place named, else the home place in Pebble's settings, else the time zone's city
            QString place = params.value("arguments").toObject().value("place").toString().trimmed();
            if (place.isEmpty()) place = tSettings().value(QStringLiteral("Assistant/weatherPlace")).toString().trimmed();
            auto* network = new QNetworkAccessManager(this);
            auto answer = [this, network, mcp, requestId](const QString& text) {
                const QJsonObject reply{{"jsonrpc", "2.0"}, {"id", mcp.value("id")}, {"result", QJsonObject{{"content", QJsonArray{QJsonObject{{"type", "text"}, {"text", text}}}}}}};
                d->mcpCards["weather"].append(text.startsWith(QStringLiteral("Now")));
                write(QJsonObject{{"type", "control_response"}, {"response", QJsonObject{{"subtype", "success"}, {"request_id", requestId}, {"response", QJsonObject{{"mcp_response", reply}}}}}});
                network->deleteLater();
            };
            auto get = [network](const QUrl& url) {
                QNetworkRequest request(url);
                request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("theDesk/3.0 (desktop assistant)"));
                request.setTransferTimeout(8000);
                return network->get(request);
            };
            auto forecast = [this, get, answer](double latitude, double longitude, const QString& name) {
                QUrl url(QStringLiteral("https://api.open-meteo.com/v1/forecast"));
                url.setQuery(QStringLiteral("latitude=%1&longitude=%2&current=temperature_2m,weather_code,is_day&hourly=temperature_2m,weather_code,is_day&forecast_hours=7&timezone=auto").arg(latitude, 0, 'f', 2).arg(longitude, 0, 'f', 2));
                QNetworkReply* reply = get(url);
                connect(reply, &QNetworkReply::finished, this, [this, reply, answer, name] {
                    reply->deleteLater();
                    const QJsonObject json = QJsonDocument::fromJson(reply->readAll()).object();
                    const QJsonObject now = json.value("current").toObject(), hourly = json.value("hourly").toObject();
                    if (reply->error() != QNetworkReply::NoError || now.isEmpty()) {
                        answer(QStringLiteral("Couldn't reach the weather service."));
                        return;
                    }
                    // the WMO weather code as a kind of sky
                    auto kindOf = [](int code, bool day) -> QString {
                        if (code == 0) return day ? QStringLiteral("clear") : QStringLiteral("night");
                        if (code <= 2) return day ? QStringLiteral("partly") : QStringLiteral("partlynight");
                        if (code == 3) return QStringLiteral("cloudy");
                        if (code == 45 || code == 48) return QStringLiteral("fog");
                        if ((code >= 71 && code <= 77) || code == 85 || code == 86) return QStringLiteral("snow");
                        if (code >= 95) return QStringLiteral("storm");
                        if (code >= 51 && code <= 57) return QStringLiteral("drizzle");
                        return QStringLiteral("rain");
                    };
                    static const QHash<QString, const char*> skies{{"clear", QT_TR_NOOP("Clear")}, {"night", QT_TR_NOOP("Clear night")}, {"partly", QT_TR_NOOP("Partly cloudy")}, {"partlynight", QT_TR_NOOP("Partly cloudy")}, {"cloudy", QT_TR_NOOP("Cloudy")}, {"fog", QT_TR_NOOP("Fog")}, {"drizzle", QT_TR_NOOP("Drizzle")}, {"rain", QT_TR_NOOP("Rain")}, {"snow", QT_TR_NOOP("Snow")}, {"storm", QT_TR_NOOP("Thunderstorm")}};
                    const QString kind = kindOf(now.value("weather_code").toInt(), now.value("is_day").toInt() == 1);
                    AssistantCard card;
                    card.id = QStringLiteral("weather%1").arg(d->nextRequest++);
                    card.kind = AssistantCard::Weather;
                    card.title = name;
                    card.subtitle = tr(skies.value(kind));
                    card.value = now.value("temperature_2m").toDouble();
                    card.icon = kind;
                    const QJsonArray times = hourly.value("time").toArray(), temps = hourly.value("temperature_2m").toArray(), codes = hourly.value("weather_code").toArray(), days = hourly.value("is_day").toArray();
                    QStringList told{QStringLiteral("Now in %1: %2 °C, %3.").arg(name).arg(qRound(card.value)).arg(skies.value(kind))};
                    for (int i = 1; i < times.size() && i < 7; i++) {
                        const QString hour = times.at(i).toString().mid(11, 2), hourKind = kindOf(codes.at(i).toInt(), days.at(i).toInt() == 1);
                        card.lines.append(hour + '\t' + QString::number(qRound(temps.at(i).toDouble())) + '\t' + hourKind);
                        told.append(QStringLiteral("%1:00 %2 °C %3").arg(hour).arg(qRound(temps.at(i).toDouble())).arg(skies.value(hourKind)));
                    }
                    told.append(QStringLiteral("The card shows this; say the gist in a sentence."));
                    takeText(QString(), true);
                    emit cardAdded(card);
                    answer(told.join('\n'));
                });
            };
            if (place.isEmpty()) {
                // the time zone's main city (zone1970.tab: "+5026+03031" = 50°26' N, 30°31' E)
                const QString zone = QString::fromUtf8(QTimeZone::systemTimeZoneId());
                QFile table(QStringLiteral("/usr/share/zoneinfo/zone1970.tab"));
                static const QRegularExpression at(QStringLiteral("^([+-])(\\d{2})(\\d{2})(\\d{2})?([+-])(\\d{3})(\\d{2})(\\d{2})?$"));
                if (table.open(QFile::ReadOnly)) {
                    for (const QByteArray& line : table.readAll().split('\n')) {
                        const QList<QByteArray> fields = line.split('\t');
                        if (fields.size() < 3 || QString::fromUtf8(fields.at(2)) != zone) continue;
                        const QRegularExpressionMatch m = at.match(QString::fromUtf8(fields.at(1)));
                        if (!m.hasMatch()) break;
                        const double latitude = (m.captured(2).toDouble() + m.captured(3).toDouble() / 60) * (m.captured(1) == "-" ? -1 : 1);
                        const double longitude = (m.captured(6).toDouble() + m.captured(7).toDouble() / 60) * (m.captured(5) == "-" ? -1 : 1);
                        forecast(latitude, longitude, zone.section('/', -1).replace('_', ' '));
                        return;
                    }
                }
                answer(QStringLiteral("I don't know where the user is; ask which city."));
                return;
            }
            QUrl search(QStringLiteral("https://geocoding-api.open-meteo.com/v1/search"));
            QUrlQuery query;
            query.addQueryItem(QStringLiteral("name"), place);
            query.addQueryItem(QStringLiteral("count"), QStringLiteral("1"));
            search.setQuery(query);
            QNetworkReply* found = get(search);
            connect(found, &QNetworkReply::finished, this, [found, forecast, answer, place] {
                found->deleteLater();
                const QJsonObject first = QJsonDocument::fromJson(found->readAll()).object().value("results").toArray().first().toObject();
                if (first.isEmpty()) answer(QStringLiteral("No place called %1 found.").arg(place));
                else forecast(first.value("latitude").toDouble(), first.value("longitude").toDouble(), first.value("name").toString());
            });
            return;
        }
        if (mcp.value("method") == "tools/call" && params.value("name") == "disk_space") {
            // adding up folders takes a moment: off the desktop's thread
            struct Folder {
                    QString name, path;
                    qint64 bytes = 0;
            };
            auto* folders = new QList<Folder>();
            QThread* worker = QThread::create([folders] {
                // the home folder's own folders, and the two hidden ones that grow big; a few seconds at most
                QElapsedTimer spent;
                spent.start();
                QStringList paths;
                for (const QFileInfo& info : QDir::home().entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks)) paths.append(info.absoluteFilePath());
                for (const char* hidden : {".cache", ".local/share"}) paths.append(QDir::homePath() + '/' + hidden);
                for (const QString& path : std::as_const(paths)) {
                    Folder folder{QFileInfo(path).fileName(), path};
                    if (path.endsWith(QStringLiteral("/.local/share"))) folder.name = QStringLiteral(".local/share");
                    QDirIterator it(path, QDir::Files | QDir::Hidden | QDir::NoSymLinks, QDirIterator::Subdirectories);
                    while (it.hasNext() && spent.elapsed() < 6000) folder.bytes += it.nextFileInfo().size();
                    folders->append(folder);
                }
                std::sort(folders->begin(), folders->end(), [](const Folder& a, const Folder& b) { return a.bytes > b.bytes; });
            });
            connect(worker, &QThread::finished, this, [this, worker, folders, mcp, requestId] {
                const QStorageInfo disk(QDir::homePath());
                const qint64 total = disk.bytesTotal(), free = disk.bytesAvailable();
                const QLocale locale;
                AssistantCard card;
                card.id = QStringLiteral("disk%1").arg(d->nextRequest++);
                card.kind = AssistantCard::Storage;
                card.title = tr("Disk");
                card.subtitle = tr("%1 free of %2").arg(locale.formattedDataSize(free, 0, QLocale::DataSizeTraditionalFormat), locale.formattedDataSize(total, 0, QLocale::DataSizeTraditionalFormat));
                card.value = total > 0 ? double(total - free) / total : 0;
                card.path = disk.rootPath();
                QStringList told{QStringLiteral("Disk (%1): %2 free of %3, %4% used.").arg(disk.rootPath(), locale.formattedDataSize(free, 1, QLocale::DataSizeTraditionalFormat), locale.formattedDataSize(total, 1, QLocale::DataSizeTraditionalFormat)).arg(qRound(card.value * 100)), QStringLiteral("Biggest folders in the home folder:")};
                for (int i = 0; i < folders->size() && i < 6; i++) {
                    const Folder& folder = folders->at(i);
                    if (i < 3) card.lines.append(folder.name + '\t' + QString::number(folder.bytes) + '\t' + folder.path);
                    told.append(QStringLiteral("- %1: %2").arg(folder.path, locale.formattedDataSize(folder.bytes, 1, QLocale::DataSizeTraditionalFormat)));
                }
                told.append(QStringLiteral("The card already shows these numbers; say the gist in a sentence."));
                takeText(QString(), true);
                emit cardAdded(card);
                d->mcpCards["disk_space"].append(true);
                const QJsonObject reply{{"jsonrpc", "2.0"}, {"id", mcp.value("id")}, {"result", QJsonObject{{"content", QJsonArray{QJsonObject{{"type", "text"}, {"text", told.join('\n')}}}}}}};
                write(QJsonObject{{"type", "control_response"}, {"response", QJsonObject{{"subtype", "success"}, {"request_id", requestId}, {"response", QJsonObject{{"mcp_response", reply}}}}}});
                delete folders;
                worker->deleteLater();
            });
            worker->start();
            return;
        }
        if (mcp.value("method") == "tools/call" && params.value("name") == "run_command") {
            // the user allowed it already (it always asks); it runs without holding up the desktop
            auto* shell = new QProcess(this);
            shell->setWorkingDirectory(QDir::homePath());
            shell->setProcessChannelMode(QProcess::MergedChannels);
            auto* limit = new QTimer(shell);
            limit->setSingleShot(true);
            connect(limit, &QTimer::timeout, shell, &QProcess::kill);
            connect(shell, &QProcess::finished, this, [this, shell, mcp, requestId, limit](int code, QProcess::ExitStatus status) {
                QString out = QString::fromUtf8(shell->readAll());
                if (out.size() > 8000) out = out.left(8000) + QStringLiteral("\n[cut]");
                const QString tail = status == QProcess::CrashExit && !limit->isActive() ? QStringLiteral("stopped after 2 minutes") : QString::number(code);
                const QJsonObject reply{{"jsonrpc", "2.0"}, {"id", mcp.value("id")}, {"result", QJsonObject{{"content", QJsonArray{QJsonObject{{"type", "text"}, {"text", out + QStringLiteral("\nexit code: ") + tail}}}}}}};
                d->mcpCards["run_command"].append(false);
                write(QJsonObject{{"type", "control_response"}, {"response", QJsonObject{{"subtype", "success"}, {"request_id", requestId}, {"response", QJsonObject{{"mcp_response", reply}}}}}});
                shell->deleteLater();
            });
            shell->start(QStringLiteral("bash"), {"-lc", params.value("arguments").toObject().value("command").toString()});
            limit->start(120000);
            return;
        }
        const QJsonObject reply = handleMcp(mcp);
        write(QJsonObject{{"type", "control_response"}, {"response", QJsonObject{{"subtype", "success"}, {"request_id", requestId}, {"response", QJsonObject{{"mcp_response", reply}}}}}});
    } else if (subtype == "can_use_tool") {
        // A card asks the user; the answer goes back with answer()
        const QString tool = request.value("tool_name").toString();
        const QJsonObject input = request.value("input").toObject();
        if (mayGoAhead(tool, input)) {
            write(QJsonObject{{"type", "control_response"}, {"response", QJsonObject{{"subtype", "success"}, {"request_id", requestId}, {"response", QJsonObject{{"behavior", "allow"}, {"updatedInput", input}}}}}});
            return;
        }
        d->asks.insert(requestId, input);
        const QString toolUseId = request.value("tool_use_id").toString();
        if (!toolUseId.isEmpty()) d->asked.insert(toolUseId);
        AssistantCard card{requestId, AssistantCard::Ask};
        const QString path = input.value("file_path").toString();
        if (tool == "mcp__desk__run_command") {
            card.title = tr("Run this?");
            card.subtitle = input.value("why").toString();
            card.lines = input.value("command").toString().split('\n');
        } else if (tool == "mcp__desk__close_window") {
            card.title = tr("Close %1?").arg(clip(input.value("window").toString(), 30));
            card.subtitle = tr("Unsaved work in it may be lost");
        } else if (tool == "Edit") {
            card.title = tr("Change %1?").arg(fileName(path));
            card.subtitle = shortPath(path);
            for (const QString& l : input.value("old_string").toString().split('\n')) card.lines.append(QStringLiteral("-") + l);
            for (const QString& l : input.value("new_string").toString().split('\n')) card.lines.append(QStringLiteral("+") + l);
        } else if (tool == "Write") {
            card.title = QFileInfo::exists(path) ? tr("Replace %1?").arg(fileName(path)) : tr("Create %1?").arg(fileName(path));
            card.subtitle = shortPath(path);
            card.lines = input.value("content").toString().split('\n').mid(0, 12);
        } else {
            card.title = tr("Use %1?").arg(tool);
            card.lines = QString::fromUtf8(QJsonDocument(input).toJson(QJsonDocument::Indented)).split('\n').mid(0, 12);
        }
        card.path = path;
        emit cardAdded(card);
    }
}

bool ClaudeBrain::mayGoAhead(const QString& tool, const QJsonObject& input) {
    // Without asking: looking-only commands, and changes to files it can put
    // back (a backup first, then undo). Never once the chat is tainted
    if (d->tainted) return false;
    if (tool == "mcp__desk__run_command") return lookingOnly(input.value("command").toString());
    if (tool == "Edit" || tool == "Write") {
        const QString path = input.value("file_path").toString();
        if (!freePath(path)) return false;
        if (QFileInfo::exists(path)) {
            const QString copy = backupOf(path);
            d->undos.append([path, copy] {
                QFile::remove(path);
                QFile::copy(copy, path);
            });
        } else {
            d->undos.append([path] {
                QFile::remove(path);
            });
        }
        return true;
    }
    if (tool == "mcp__desk__trash") {
        for (const QJsonValue& path : input.value("paths").toArray()) {
            if (!freePath(path.toString())) return false;
        }
        return true;
    }
    if (tool == "mcp__desk__move") return freePath(input.value("from").toString()) && freePath(input.value("to").toString());
    return false;
}

QJsonObject ClaudeBrain::handleMcp(const QJsonObject& message) {
    const QString method = message.value("method").toString();
    if (!message.contains("id")) return {}; // a notification
    QJsonObject result;
    if (method == "initialize") {
        result = QJsonObject{{"protocolVersion", message.value("params").toObject().value("protocolVersion")},
            {"capabilities", QJsonObject{{"tools", QJsonObject{}}}}, {"serverInfo", QJsonObject{{"name", "desk"}, {"version", "1"}}}};
    } else if (method == "tools/list") {
        QJsonArray list;
        for (const Inner& tool : innerTools()) list.append(QJsonObject{{"name", tool.name}, {"description", tool.description}, {"inputSchema", tool.schema}});
        for (const AssistantTool& tool : AssistantTools::instance()->tools()) list.append(QJsonObject{{"name", tool.name}, {"description", tool.description}, {"inputSchema", tool.schema}});
        result = QJsonObject{{"tools", list}};
    } else if (method == "tools/call") {
        const QJsonObject params = message.value("params").toObject();
        const QString name = params.value("name").toString();
        const QJsonObject args = params.value("arguments").toObject();
        QJsonArray content;
        bool card = false;
        auto say = [&content](const QString& text) {
            content.append(QJsonObject{{"type", "text"}, {"text", text}});
        };
        if (name == "think_harder") {
            if (d->depth == 0) {
                setDepth(1);
                d->deepUntil = QDateTime::currentMSecsSinceEpoch() + StayDeepMs;
                deepen();
            }
            say(QStringLiteral("Deeper mode is on. Carry on with the job."));
        } else if (name == "look_at_screen") {
            if (d->screen.isNull()) {
                say(QStringLiteral("The user didn't share the screen this time."));
            } else {
                QByteArray jpeg;
                QBuffer buffer(&jpeg);
                buffer.open(QIODevice::WriteOnly);
                d->screen.scaledToWidth(qMin(1280, d->screen.width()), Qt::SmoothTransformation).save(&buffer, "JPEG", 80);
                content.append(QJsonObject{{"type", "image"}, {"data", QString::fromLatin1(jpeg.toBase64())}, {"mimeType", "image/jpeg"}});
            }
        } else if (name == "remember") {
            QStringList lines = memories();
            lines.append(args.value("fact").toString().simplified());
            saveMemories(lines);
            say(QStringLiteral("Remembered."));
        } else if (name == "forget") {
            const QString words = args.value("words").toString();
            QStringList lines = memories();
            const qsizetype before = lines.size();
            lines.removeIf([&words](const QString& line) {
                return line.contains(words, Qt::CaseInsensitive);
            });
            saveMemories(lines);
            say(QStringLiteral("Forgot %1 thing(s).").arg(before - lines.size()));
        } else if (name == "undo_last") {
            const QList<std::function<void()>> undos = d->undos.isEmpty() ? d->lastUndos : d->undos;
            for (auto it = undos.crbegin(); it != undos.crend(); ++it) (*it)();
            if (d->undos.isEmpty()) d->lastUndos.clear();
            else d->undos.clear();
            say(undos.isEmpty() ? QStringLiteral("There was nothing to undo.") : QStringLiteral("Put %1 change(s) back.").arg(undos.size()));
        } else if (name == "trash") {
            // the trash, not gone: each one can come back with undo_last
            QStringList done, failed;
            for (const QJsonValue& value : args.value("paths").toArray()) {
                const QString path = value.toString();
                QString inTrash;
                if (QFile::moveToTrash(path, &inTrash)) {
                    done.append(path);
                    d->undos.append([path, inTrash] {
                        QFile::rename(inTrash, path);
                    });
                } else {
                    failed.append(path);
                }
            }
            say(QStringLiteral("In the trash: %1.%2").arg(done.isEmpty() ? QStringLiteral("nothing") : done.join(QStringLiteral(", ")), failed.isEmpty() ? QString() : QStringLiteral(" Couldn't: ") + failed.join(QStringLiteral(", "))));
        } else if (name == "move") {
            const QString from = args.value("from").toString(), to = args.value("to").toString();
            if (QFileInfo::exists(to)) {
                say(QStringLiteral("%1 is already there; nothing moved.").arg(to));
            } else {
                QDir().mkpath(QFileInfo(to).absolutePath());
                if (QFile::rename(from, to)) {
                    d->undos.append([from, to] {
                        QFile::rename(to, from);
                    });
                    say(QStringLiteral("Moved to %1.").arg(to));
                } else {
                    say(QStringLiteral("Couldn't move %1.").arg(from));
                }
            }
        } else if (AssistantTools::instance()->has(name)) {
            const AssistantToolResult done = AssistantTools::instance()->tool(name).run(args);
            say(done.text);
            if (done.undo) d->undos.append(done.undo);
            if (!done.card.isEmpty()) {
                AssistantCard shown;
                shown.id = QStringLiteral("desk%1").arg(d->nextRequest++);
                static const QHash<QString, AssistantCard::Kind> kinds{{"image", AssistantCard::Image}, {"switch", AssistantCard::Switch}, {"battery", AssistantCard::Battery}, {"media", AssistantCard::Media}, {"notifications", AssistantCard::Notifications}, {"timer", AssistantCard::Timer}, {"processes", AssistantCard::Processes}};
                shown.kind = kinds.value(done.card, AssistantCard::Level);
                shown.lines = done.lines;
                shown.title = done.title;
                shown.subtitle = done.subtitle;
                shown.value = done.value;
                shown.path = done.path;
                shown.icon = done.icon;
                if (done.setValue) d->setters.insert(shown.id, done.setValue);
                takeText(QString(), true);
                emit cardAdded(shown);
                card = true;
            }
        } else {
            return QJsonObject{{"jsonrpc", "2.0"}, {"id", message.value("id")}, {"error", QJsonObject{{"code", -32602}, {"message", "Unknown tool"}}}};
        }
        d->mcpCards[name].append(card);
        result = QJsonObject{{"content", content}};
    } else {
        return QJsonObject{{"jsonrpc", "2.0"}, {"id", message.value("id")}, {"error", QJsonObject{{"code", -32601}, {"message", "Method not found"}}}};
    }
    return QJsonObject{{"jsonrpc", "2.0"}, {"id", message.value("id")}, {"result", result}};
}

QStringList ClaudeBrain::remembered() {
    return memories();
}

void ClaudeBrain::setRemembered(const QStringList& facts) {
    saveMemories(facts);
}

QString ClaudeBrain::promptPath() {
    systemPrompt(); // writes the default when there's none yet
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + QStringLiteral("/theSuite/pebble-prompt.md");
}

void ClaudeBrain::resetPrompt() {
    QFile::remove(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + QStringLiteral("/theSuite/pebble-prompt.md"));
    systemPrompt();
}
