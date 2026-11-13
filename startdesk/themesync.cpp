/****************************************
 *
 *   theDesk - session colours for other programs
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 * *************************************/
#include "themesync.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTextStream>
#include <csignal>
#include <m3scheme.h>
#include <m3usercard.h>
#include <QImage>
#include <sys/stat.h>
#include <tsettings.h>
#include <unistd.h>

namespace {
    QString hex(const QColor& c) {
        return c.name();
    }

    QString configHome() {
        QString config = qEnvironmentVariable("XDG_CONFIG_HOME");
        return config.isEmpty() ? QDir::homePath() + QStringLiteral("/.config") : config;
    }

    // Every process of ours with this command name
    QList<pid_t> processesNamed(const QByteArray& name) {
        QList<pid_t> pids;
        const uid_t me = getuid();
        for (const QString& entry : QDir(QStringLiteral("/proc")).entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
            bool ok;
            const pid_t pid = entry.toInt(&ok);
            if (!ok) continue;
            struct stat info;
            if (stat(QStringLiteral("/proc/%1").arg(pid).toLocal8Bit().constData(), &info) != 0 || info.st_uid != me) continue;
            QFile comm(QStringLiteral("/proc/%1/comm").arg(pid));
            if (comm.open(QFile::ReadOnly) && comm.readAll().trimmed() == name) pids.append(pid);
        }
        return pids;
    }

    void writeKitty(const M3::Scheme& s, const QList<QColor>& ansi) {
        const QString dir = configHome() + QStringLiteral("/kitty");
        if (!QDir(dir).exists()) return; // no kitty here
        QSaveFile file(dir + QStringLiteral("/thedesk-m3.conf"));
        if (!file.open(QFile::WriteOnly)) return;
        QTextStream t(&file);
        t << "# Written by theDesk from the session's Material 3 colours; it changes\n"
             "# with the living horizon. kitty.conf includes this file last.\n"
          << "foreground " << hex(s.onSurface) << '\n'
          << "background " << hex(s.surface) << '\n'
          << "selection_foreground " << hex(s.onPrimaryContainer) << '\n'
          << "selection_background " << hex(s.primaryContainer) << '\n'
          << "cursor " << hex(s.primary) << '\n'
          << "cursor_text_color " << hex(s.onPrimary) << '\n'
          << "url_color " << hex(s.tertiary) << '\n'
          << "active_border_color " << hex(s.primary) << '\n'
          << "inactive_border_color " << hex(s.outlineVariant) << '\n'
          << "bell_border_color " << hex(s.error) << '\n'
          << "tab_bar_background " << hex(s.surfaceContainerLowest) << '\n'
          << "active_tab_foreground " << hex(s.onSecondaryContainer) << '\n'
          << "active_tab_background " << hex(s.secondaryContainer) << '\n'
          << "inactive_tab_foreground " << hex(s.onSurfaceVariant) << '\n'
          << "inactive_tab_background " << hex(s.surfaceContainer) << '\n'
          << "mark1_foreground " << hex(s.onTertiaryContainer) << '\n'
          << "mark1_background " << hex(s.tertiaryContainer) << '\n';
        for (int i = 0; i < ansi.size(); i++) t << "color" << i << ' ' << hex(ansi.at(i)) << '\n';
        t.flush();
        if (!file.commit()) return;

        // Running kittys reread their config on SIGUSR1
        for (pid_t pid : processesNamed("kitty")) kill(pid, SIGUSR1);
    }

    // libadwaita's named colours (adw-gtk3 uses the same names), from the scheme
    QList<QPair<QString, QColor>> gtkColours(const M3::Scheme& s) {
        const QList<QPair<const char*, QColor>> colours = {
            {"accent_color", s.primary}, {"accent_bg_color", s.primary}, {"accent_fg_color", s.onPrimary},
            {"destructive_color", s.error}, {"destructive_bg_color", s.error}, {"destructive_fg_color", s.onError},
            {"error_color", s.error}, {"error_bg_color", s.error}, {"error_fg_color", s.onError},
            {"window_bg_color", s.surface}, {"window_fg_color", s.onSurface},
            {"view_bg_color", s.surfaceContainerLow}, {"view_fg_color", s.onSurface},
            {"headerbar_bg_color", s.surfaceContainer}, {"headerbar_fg_color", s.onSurface},
            {"headerbar_border_color", s.outlineVariant}, {"headerbar_backdrop_color", s.surface},
            {"sidebar_bg_color", s.surfaceContainerLow}, {"sidebar_fg_color", s.onSurface},
            {"sidebar_backdrop_color", s.surfaceContainerLow}, {"sidebar_border_color", s.outlineVariant},
            {"card_bg_color", s.surfaceContainer}, {"card_fg_color", s.onSurface},
            {"dialog_bg_color", s.surfaceContainerHigh}, {"dialog_fg_color", s.onSurface},
            {"popover_bg_color", s.surfaceContainer}, {"popover_fg_color", s.onSurface},
            {"thumbnail_bg_color", s.surfaceContainerHigh}, {"thumbnail_fg_color", s.onSurface},
        };
        QList<QPair<QString, QColor>> out;
        for (const auto& [name, colour] : colours) out.append({QLatin1String(name), colour});
        return out;
    }

    void writeFile(const QString& path, const QString& content) {
        QSaveFile file(path);
        if (!file.open(QFile::WriteOnly)) return;
        file.write(content.toUtf8());
        file.commit();
    }

    QString dataHome() {
        QString data = qEnvironmentVariable("XDG_DATA_HOME");
        return data.isEmpty() ? QDir::homePath() + QStringLiteral("/.local/share") : data;
    }

    // GTK 3: the theme "theDesk-M3", made from adw-gtk3-dark with its colour
    // definitions rewritten. (GTK 3 resolves named colours inside the theme's
    // own stylesheet, so gtk.css can't change them.) Firefox's system theme
    // and other GTK 3 apps follow it from their next start.
    void writeGtk3Theme(const M3::Scheme& s) {
        const QString source = QStringLiteral("/usr/share/themes/adw-gtk3-dark/gtk-3.0");
        QFile base(source + QStringLiteral("/gtk.css"));
        if (!base.open(QFile::ReadOnly)) return; // adw-gtk3 isn't installed
        QString css = QString::fromUtf8(base.readAll());
        for (const auto& [name, colour] : gtkColours(s)) {
            css.replace(QRegularExpression(QStringLiteral("@define-color %1 [^;]*;").arg(name)), QStringLiteral("@define-color %1 %2;").arg(name, colour.name()));
        }
        const QString theme = dataHome() + QStringLiteral("/themes/theDesk-M3");
        const QString dir = theme + QStringLiteral("/gtk-3.0");
        if (!QDir().mkpath(dir)) return;
        css.prepend(QStringLiteral("/* Written by theDesk: adw-gtk3-dark in the session's Material 3 colours. */\n"));
        writeFile(dir + QStringLiteral("/gtk.css"), css);
        writeFile(dir + QStringLiteral("/gtk-dark.css"), css);
        if (!QFileInfo::exists(dir + QStringLiteral("/assets"))) QFile::link(source + QStringLiteral("/assets"), dir + QStringLiteral("/assets"));
        writeFile(theme + QStringLiteral("/index.theme"), QStringLiteral("[Desktop Entry]\nType=X-GNOME-Metatheme\nName=theDesk-M3\nComment=adw-gtk3 in theDesk's Material 3 colours\n\n[X-GNOME-Metatheme]\nGtkTheme=theDesk-M3\n"));
    }

    // GTK 4 (libadwaita): a user stylesheet can set the named colours, and
    // newer libadwaita reads CSS variables of the same names
    void writeGtk4(const M3::Scheme& s) {
        const QString dir = configHome() + QStringLiteral("/gtk-4.0");
        if (!QDir().mkpath(dir)) return;
        QString css = QStringLiteral("/* Written by theDesk from the session's Material 3 colours. */\n");
        QString variables = QStringLiteral(":root {\n");
        for (const auto& [name, colour] : gtkColours(s)) {
            css += QStringLiteral("@define-color %1 %2;\n").arg(name, colour.name());
            variables += QStringLiteral("    --%1: %2;\n").arg(QString(name).replace(QLatin1Char('_'), QLatin1Char('-')), colour.name());
        }
        writeFile(dir + QStringLiteral("/thedesk-m3.css"), css + variables + QStringLiteral("}\n"));

        // gtk.css imports it first. It was a whole Gruvbox palette that would
        // win over it: that's kept as gtk.css.gruvbox
        const QString import = QStringLiteral("@import url(\"thedesk-m3.css\");");
        QFile gtk(dir + QStringLiteral("/gtk.css"));
        QString existing;
        if (gtk.open(QFile::ReadOnly)) {
            existing = QString::fromUtf8(gtk.readAll());
            gtk.close();
        }
        if (existing.contains(import)) return;
        if (existing.contains(QStringLiteral("@define-color"))) {
            if (!QFile::exists(gtk.fileName() + QStringLiteral(".gruvbox"))) QFile::copy(gtk.fileName(), gtk.fileName() + QStringLiteral(".gruvbox"));
            existing.clear();
        }
        if (QFileInfo(gtk.fileName()).isSymLink()) QFile::remove(gtk.fileName());
        writeFile(gtk.fileName(), import + QStringLiteral("\n") + existing);
    }

    void writeGtk(const M3::Scheme& s) {
        writeGtk3Theme(s);
        writeGtk4(s);
    }

    // The starship prompt (~/.config/starship.toml) with its "thedesk"
    // palette in the session's colours, for zsh to point STARSHIP_CONFIG at.
    // starship reads its config at every prompt, so the next prompt has them.
    void writeStarship(const M3::Scheme& s) {
        QFile templateFile(configHome() + QStringLiteral("/starship.toml"));
        if (!templateFile.open(QFile::ReadOnly)) return;
        const QStringList lines = QString::fromUtf8(templateFile.readAll()).split(QLatin1Char('\n'));
        if (!lines.contains(QStringLiteral("palette = \"thedesk\""))) return; // not ours

        // Everything but the template's own (fallback) palette
        QString out;
        bool inPalette = false;
        for (const QString& line : lines) {
            const QString trimmed = line.trimmed();
            if (trimmed.startsWith(QLatin1Char('['))) inPalette = trimmed == QStringLiteral("[palettes.thedesk]");
            if (!inPalette) out += line + QLatin1Char('\n');
        }
        while (out.endsWith(QStringLiteral("\n\n"))) out.chop(1);

        const QList<QPair<const char*, QColor>> colours = {
            {"primary", s.primary},
            {"on_primary", s.onPrimary},
            {"primary_container", s.primaryContainer},
            {"on_primary_container", s.onPrimaryContainer},
            {"secondary_container", s.secondaryContainer},
            {"on_secondary_container", s.onSecondaryContainer},
            {"tertiary", s.tertiary},
            {"error", s.error},
            {"on_surface_variant", s.onSurfaceVariant},
            {"outline", s.outline},
        };
        out += QStringLiteral("\n# Written by theDesk from the session's Material 3 colours\n[palettes.thedesk]\n");
        for (const auto& [name, colour] : colours) out += QStringLiteral("%1 = \"%2\"\n").arg(QLatin1String(name), hex(colour));

        QString cache = qEnvironmentVariable("XDG_CACHE_HOME");
        if (cache.isEmpty()) cache = QDir::homePath() + QStringLiteral("/.cache");
        const QString dir = cache + QStringLiteral("/thedesk");
        if (!QDir().mkpath(dir)) return;
        writeFile(dir + QStringLiteral("/starship.toml"), out);
    }

    void writeBspwm(const M3::Scheme& s) {
        if (QStandardPaths::findExecutable(QStringLiteral("bspc")).isEmpty() || processesNamed("bspwm").isEmpty()) return;
        const QList<QPair<QString, QColor>> settings = {
            {QStringLiteral("focused_border_color"), s.primary},
            {QStringLiteral("active_border_color"), s.outline},
            {QStringLiteral("normal_border_color"), s.surfaceContainerHighest},
            {QStringLiteral("presel_feedback_color"), s.primaryContainer},
        };
        for (const auto& [key, colour] : settings) {
            QProcess::startDetached(QStringLiteral("bspc"), {QStringLiteral("config"), key, hex(colour)});
        }
    }
} // namespace

bool ThemeSync::enabled() {
    tSettings settings(QStringLiteral("theSuite"), QStringLiteral("theDesk.platform"));
    return settings.value(QStringLiteral("Palette/base")).toString() == QStringLiteral("m3");
}

void ThemeSync::writeGreeterPalette() {
    // LightDM gives each session a folder its greeter can read
    // ($XDG_GREETER_DATA_DIR): leave the colour choice there, so the login
    // screen wears the same colours and its hand-off into theDesk doesn't jump
    const QString dir = qEnvironmentVariable("XDG_GREETER_DATA_DIR");
    if (dir.isEmpty() || !QDir(dir).exists()) return;
    tSettings settings(QStringLiteral("theSuite"), QStringLiteral("theDesk.platform"));
    QSaveFile file(dir + QStringLiteral("/thedesk-palette.conf"));
    if (!file.open(QFile::WriteOnly)) return;
    QTextStream t(&file);
    t << "[Palette]\n";
    for (const QString& key : {QStringLiteral("base"), QStringLiteral("m3.seed"), QStringLiteral("m3.variant"), QStringLiteral("m3.mode"), QStringLiteral("m3.contrast")}) {
        t << key << '=' << settings.value(QStringLiteral("Palette/") + key).toString() << '\n';
    }
    // The shape the user's picture wears (M3UserCard), so the login screen's
    // picture is the same shape as the lock screen's
    tSettings desk(QStringLiteral("theSuite"), QStringLiteral("theDesk"));
    if (desk.keysInGroup(QStringLiteral("UserCard")).contains(QStringLiteral("shapes"))) {
        t << "\n[UserCard]\nshapes=" << desk.value(QStringLiteral("UserCard/shapes")).toString() << '\n';
    }
    t.flush();
    file.commit();
}

void ThemeSync::writeGreeterPicture() {
    // The login screen runs as LightDM's user, which can't read a private
    // home folder: leave a copy of the picture where it can
    const QString dir = qEnvironmentVariable("XDG_GREETER_DATA_DIR");
    if (dir.isEmpty() || !QDir(dir).exists()) return;
    const QString source = M3UserCard::currentUserPicture();
    QImage picture = source.isEmpty() ? QImage() : QImage(source);
    const QString target = dir + QStringLiteral("/picture.png");
    if (picture.isNull()) {
        QFile::remove(target);
        return;
    }
    if (picture.width() > 512 || picture.height() > 512) picture = picture.scaled(512, 512, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
    QSaveFile file(target);
    if (file.open(QFile::WriteOnly) && picture.save(&file, "PNG")) file.commit();
}

void ThemeSync::apply() {
    // Not from a nested test session: it would leave its stand-in picture
    // and colours for the real login screen
    if (!qEnvironmentVariableIsSet("TD_NESTED_TEST")) {
        writeGreeterPalette();
        writeGreeterPicture();
    }
    if (!enabled()) return;
    const M3::Config config = M3::Config::fromSettings();
    const M3::Scheme scheme = M3::schemeFor(config);
    const bool dark = qGray(scheme.surface.rgb()) < 128;
    const QList<QColor> ansi = M3::terminalColours(scheme, M3::seedFor(config), dark);
    writeKitty(scheme, ansi);
    writeStarship(scheme);
    writeBspwm(scheme);
    writeGtk(scheme);
}
