#ifndef PEBBLERULES_H
#define PEBBLERULES_H

// What Pebble may touch, and what it may do without asking (claudebrain.cpp;
// checked by tools/pebble-rules-test)

#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSet>
#include <QStringList>

namespace {
    // Never read, changed or written, whatever a page or a message says
    const QStringList PrivateFolders{".ssh", ".gnupg", ".password-store", ".local/share/keyrings", ".mozilla", ".librewolf", ".config/google-chrome",
        ".config/chromium", ".config/BraveSoftware", ".config/vivaldi", ".claude", ".aws", ".kube", ".docker", ".netrc", ".pki",
        ".git-credentials", ".config/gh", ".local/share/thedesk/assistant"};

    // ---- What Pebble may do without asking --------------------------------

    // A path it may change on its own: in the home folder, nothing hidden on
    // the way (dotfiles and ~/.config can run code later), nothing private
    bool freePathText(const QString& path) {
        const QString home = QDir::homePath() + '/';
        const QString clean = QDir::cleanPath(path);
        if (!clean.startsWith(home)) return false;
        for (const QString& part : clean.mid(home.size()).split('/')) {
            if (part.isEmpty() || part.startsWith('.')) return false;
        }
        return true;
    }
    bool freePath(const QString& path) {
        // the path as written, and where it really leads (a link may point into a dotfolder)
        if (!freePathText(path)) return false;
        QFileInfo info(QDir::cleanPath(path));
        while (!info.exists() && info.absolutePath() != info.absoluteFilePath()) info = QFileInfo(info.absolutePath());
        const QString real = info.canonicalFilePath();
        return real == QDir(QDir::homePath()).canonicalPath() || freePathText(QString(real).replace(QDir(QDir::homePath()).canonicalPath(), QDir::homePath()));
    }

    // A command that only looks: every part of a pipe one of these, no
    // redirects, no subshells, nothing private named
    bool lookingOnly(const QString& command) {
        static const QRegularExpression shellish(QStringLiteral("[<>;&`$()\\\\{}\\n]|\\|\\|"));
        if (command.contains(shellish)) return false;
        for (const QString& folder : PrivateFolders) {
            if (command.contains(folder)) return false;
        }
        if (command.contains(QStringLiteral(".env"))) return false;
        static const QSet<QString> looks{"ls", "cat", "head", "tail", "wc", "df", "du", "free", "uptime", "uname", "whoami", "date", "cal", "pwd",
            "echo", "grep", "rg", "stat", "file", "which", "whereis", "ps", "pgrep", "lsblk", "lscpu", "lsusb", "lspci", "sensors", "tree", "sort",
            "uniq", "cut", "column", "nproc", "id", "groups", "jq", "basename", "dirname", "realpath", "readlink", "md5sum", "sha256sum", "fc-list",
            "fc-match", "lsmod", "rc-status", "uptime", "w", "last", "locale", "tty", "printenv"};
        for (const QString& part : command.split('|')) {
            const QStringList words = part.simplified().split(' ', Qt::SkipEmptyParts);
            if (words.isEmpty()) return false;
            const QString& name = words.first();
            if (name == "printenv" && words.size() == 1) return false; // all of it can hold secrets
            if (looks.contains(name)) continue;
            if (name == "find" && !part.contains(QRegularExpression(QStringLiteral("-(delete|exec|execdir|ok|okdir|fprint|fprintf|fls)\\b")))) continue;
            if (name == "git" && words.size() > 1 && QSet<QString>{"log", "status", "diff", "show", "branch", "remote", "tag", "shortlog"}.contains(words.at(1)) && !part.contains(QStringLiteral("--output"))) continue;
            if (name == "pacman" && words.size() > 1 && words.at(1).startsWith(QStringLiteral("-Q"))) continue;
            if (name == "systemctl" && words.size() > 1 && QSet<QString>{"status", "list-units", "is-active", "is-enabled"}.contains(words.at(1))) continue;
            if (name == "upower" && words.size() > 1 && QSet<QString>{"-i", "-d", "-e", "--dump", "--enumerate"}.contains(words.at(1))) continue;
            if (name == "ip" && words.size() > 1 && QSet<QString>{"a", "addr", "address", "r", "route", "link"}.contains(words.at(1)) && !part.contains(QRegularExpression(QStringLiteral("\\b(set|add|del|delete|flush|change|replace)\\b")))) continue;
            return false;
        }
        return true;
    }

} // namespace

#endif // PEBBLERULES_H
