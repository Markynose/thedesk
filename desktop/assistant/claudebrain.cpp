#include "claudebrain.h"

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

    // Never read, changed or written, whatever a page or a message says
    const QStringList PrivateFolders{".ssh", ".gnupg", ".password-store", ".local/share/keyrings", ".mozilla", ".librewolf", ".config/google-chrome",
        ".config/chromium", ".config/BraveSoftware", ".config/vivaldi", ".claude", ".aws", ".kube", ".docker", ".netrc", ".pki",
        ".git-credentials", ".config/gh", ".local/share/thedesk/assistant"};

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
            {"undo_last", "Undo the desk changes you made in your last answer (brightness, do not disturb, volume, layout).", AssistantTools::schemaOf({}), false},
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
            if (!offered.isEmpty()) emit chipsOffered(offered);
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
        } else if (AssistantTools::instance()->has(name)) {
            const AssistantToolResult done = AssistantTools::instance()->tool(name).run(args);
            say(done.text);
            if (done.undo) d->undos.append(done.undo);
            if (!done.card.isEmpty()) {
                AssistantCard shown;
                shown.id = QStringLiteral("desk%1").arg(d->nextRequest++);
                shown.kind = done.card == "image" ? AssistantCard::Image : done.card == "switch" ? AssistantCard::Switch : AssistantCard::Level;
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
