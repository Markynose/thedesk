#include "desktools.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QPointer>
#include <QTimer>
#include <QUrl>
#include <Applications/application.h>
#include <Wm/desktopwm.h>
#include <assistanttools.h>
#include <quietmodemanager.h>
#include <statemanager.h>
#include <tsettings.h>

namespace {
    QString tr(const char* text) {
        return QCoreApplication::translate("DeskTools", text);
    }

    QuietModeManagerTd* quiet() {
        return StateManager::quietModeManager();
    }

    // shortcut: the "until" timer lives in memory; a restart of theDesk forgets it (store it if that bites)
    QPointer<QTimer> dndTimer;

    QString modeWord(QuietModeManagerTd::QuietMode mode) {
        switch (mode) {
            case QuietModeManagerTd::Sound: return QStringLiteral("off");
            case QuietModeManagerTd::CriticalOnly: return QStringLiteral("critical_only");
            case QuietModeManagerTd::NoNotifications: return QStringLiteral("on");
            case QuietModeManagerTd::Mute: return QStringLiteral("mute");
            default: return QStringLiteral("off");
        }
    }

    DesktopWmWindowPtr findWindow(const QString& what) {
        for (const DesktopWmWindowPtr& window : DesktopWm::openWindows()) {
            if (!window->shouldShowInTaskbar()) continue;
            QString app;
            if (ApplicationPointer application = window->application()) app = application->getProperty(QStringLiteral("Name")).toString();
            if (window->title().contains(what, Qt::CaseInsensitive) || app.contains(what, Qt::CaseInsensitive)) return window;
        }
        return {};
    }

    AssistantToolResult setDnd(const QString& mode, const QString& until) {
        AssistantToolResult result;
        const QuietModeManagerTd::QuietMode before = quiet()->currentMode();
        QuietModeManagerTd::QuietMode next = QuietModeManagerTd::NoNotifications;
        if (mode == "off") next = QuietModeManagerTd::Sound;
        else if (mode == "critical_only") next = QuietModeManagerTd::CriticalOnly;
        else if (mode == "mute") next = QuietModeManagerTd::Mute;
        quiet()->setQuietMode(next);
        if (dndTimer) dndTimer->deleteLater();

        QString subtitle = QuietModeManagerTd::name(next);
        QTime at = QTime::fromString(until, QStringLiteral("HH:mm"));
        if (next != QuietModeManagerTd::Sound && at.isValid()) {
            QDateTime when(QDate::currentDate(), at);
            if (when <= QDateTime::currentDateTime()) when = when.addDays(1);
            dndTimer = new QTimer();
            dndTimer->setSingleShot(true);
            QObject::connect(dndTimer, &QTimer::timeout, [] {
                quiet()->setQuietMode(QuietModeManagerTd::Sound);
                dndTimer->deleteLater();
            });
            dndTimer->start(int(QDateTime::currentDateTime().msecsTo(when)));
            subtitle = tr("Until %1").arg(QLocale().toString(at, QLocale::ShortFormat));
            if (next == QuietModeManagerTd::NoNotifications) subtitle += tr(" · critical ones still show");
        }
        result.text = QStringLiteral("Do not disturb is now %1%2 (was %3).").arg(modeWord(next), at.isValid() ? QStringLiteral(" until ") + until : QString(), modeWord(before));
        result.card = QStringLiteral("switch");
        result.icon = QStringLiteral("do_not_disturb_on");
        result.title = tr("Do not disturb");
        result.subtitle = subtitle;
        result.value = next == QuietModeManagerTd::Sound ? 0 : 1;
        result.setValue = [next](double on) {
            quiet()->setQuietMode(on > 0.5 ? (next == QuietModeManagerTd::Sound ? QuietModeManagerTd::NoNotifications : next) : QuietModeManagerTd::Sound);
        };
        result.undo = [before] {
            if (dndTimer) dndTimer->deleteLater();
            quiet()->setQuietMode(before);
        };
        return result;
    }
} // namespace

void DeskTools::setUp() {
    AssistantTools* tools = AssistantTools::instance();

    tools->add({QStringLiteral("desk_status"),
        QStringLiteral("What the desktop is doing now: time, do not disturb, keyboard layouts, the open windows."),
        AssistantTools::schemaOf({}), false, [](QJsonObject) {
            QStringList windows;
            for (const DesktopWmWindowPtr& window : DesktopWm::openWindows()) {
                if (window->shouldShowInTaskbar()) windows.append(QStringLiteral("- ") + window->title());
            }
            tSettings settings;
            AssistantToolResult result;
            result.text = QStringLiteral("Time: %1\nDo not disturb: %2\nKeyboard layout: %3 (available: %4)\nActive window: %5\nOpen windows:\n%6")
                              .arg(QDateTime::currentDateTime().toString(Qt::ISODate), modeWord(quiet()->currentMode()), DesktopWm::currentKeyboardLayout(),
                                  settings.delimitedList(QStringLiteral("Input/keyboard.layouts")).join(QStringLiteral(", ")),
                                  DesktopWm::activeWindow() ? DesktopWm::activeWindow()->title() : QStringLiteral("none"), windows.join('\n'));
            return result;
        }});

    tools->add({QStringLiteral("set_do_not_disturb"),
        QStringLiteral("Do not disturb. mode: on (no notifications), off, critical_only, or mute (no sound at all). until: optional HH:MM (24 hour) when it turns off by itself."),
        AssistantTools::schemaOf({{"mode", "string"}, {"until", "string: HH:MM"}}, {"until"}), false, [](QJsonObject args) {
            return setDnd(args.value("mode").toString(), args.value("until").toString());
        }});

    tools->add({QStringLiteral("set_keyboard_layout"),
        QStringLiteral("Switch the keyboard layout to one of the user's layouts (desk_status lists them)."),
        AssistantTools::schemaOf({{"layout", "string"}}), false, [](QJsonObject args) {
            AssistantToolResult result;
            const QString before = DesktopWm::currentKeyboardLayout();
            const QString layout = args.value("layout").toString();
            DesktopWm::setCurrentKeyboardLayout(layout);
            result.text = QStringLiteral("Layout is now %1.").arg(layout);
            result.undo = [before] {
                DesktopWm::setCurrentKeyboardLayout(before);
            };
            return result;
        }});

    tools->add({QStringLiteral("open_app"),
        QStringLiteral("Open an installed app by its name (\"Firefox\", \"Files\", \"Zed\")."),
        AssistantTools::schemaOf({{"name", "string"}}), false, [](QJsonObject args) {
            AssistantToolResult result;
            const QString wanted = args.value("name").toString().trimmed();
            ApplicationPointer best;
            for (const QString& entry : Application::allApplications()) {
                ApplicationPointer app(new Application(entry));
                if (!app->isValid() || app->getProperty(QStringLiteral("NoDisplay"), false).toBool()) continue;
                const QString name = app->getProperty(QStringLiteral("Name")).toString();
                if (name.compare(wanted, Qt::CaseInsensitive) == 0) {
                    best = app;
                    break;
                }
                if (!best && (name.contains(wanted, Qt::CaseInsensitive) || app->getProperty(QStringLiteral("GenericName")).toString().contains(wanted, Qt::CaseInsensitive))) best = app;
            }
            if (!best) {
                result.text = QStringLiteral("No app called %1 is installed.").arg(wanted);
                return result;
            }
            best->launch();
            result.text = QStringLiteral("Opened %1.").arg(best->getProperty(QStringLiteral("Name")).toString());
            return result;
        }});

    tools->add({QStringLiteral("focus_window"),
        QStringLiteral("Bring an open window to the front, found by part of its title or its app's name."),
        AssistantTools::schemaOf({{"window", "string"}}), false, [](QJsonObject args) {
            AssistantToolResult result;
            DesktopWmWindowPtr window = findWindow(args.value("window").toString());
            if (!window) {
                result.text = QStringLiteral("No open window matches that.");
                return result;
            }
            // after the panel has gone, or it takes the focus straight back
            QTimer::singleShot(400, [window] {
                window->activate();
            });
            result.text = QStringLiteral("Brought %1 to the front (once the panel closes).").arg(window->title());
            return result;
        }});

    tools->add({QStringLiteral("close_window"),
        QStringLiteral("Close an open window, found by part of its title or its app's name. Unsaved work may be lost, so the user is asked first."),
        AssistantTools::schemaOf({{"window", "string"}}), true, [](QJsonObject args) {
            AssistantToolResult result;
            DesktopWmWindowPtr window = findWindow(args.value("window").toString());
            if (!window) {
                result.text = QStringLiteral("No open window matches that.");
                return result;
            }
            result.text = QStringLiteral("Closed %1.").arg(window->title());
            window->close();
            return result;
        }});

    tools->add({QStringLiteral("show_image"),
        QStringLiteral("Show an image in the panel: a file path or an https URL, with a short caption. Use it whenever a picture helps (a photo, a diagram, a screenshot, an image file you found)."),
        AssistantTools::schemaOf({{"source", "string: a file path or an https URL"}, {"caption", "string"}}, {"caption"}), false, [](QJsonObject args) {
            AssistantToolResult result;
            QString source = args.value("source").toString().trimmed();
            if (source.startsWith('~')) source.replace(0, 1, QDir::homePath());
            const QUrl url(source);
            if (!(url.scheme() == "https" || url.scheme() == "http" || QFileInfo(source).isFile())) {
                result.text = QStringLiteral("That's not a file or a web address I can show.");
                return result;
            }
            result.card = QStringLiteral("image");
            result.path = source;
            result.title = args.value("caption").toString();
            result.text = QStringLiteral("Shown in the panel.");
            return result;
        }});
}
