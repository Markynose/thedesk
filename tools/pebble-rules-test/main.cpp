// What Pebble may do without asking: run with
//   g++ -fPIC -I../../desktop/assistant main.cpp $(pkg-config --cflags --libs Qt6Core) -o /tmp/t && /tmp/t
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QFile>
#include <cstdio>
#include "pebblerules.h"

static int failures = 0;
static void expect(bool got, bool want, const char* what) {
    if (got != want) {
        std::printf("FAIL %s: got %d\n", what, got);
        failures++;
    }
}

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    // looking only
    expect(lookingOnly("ls -la ~/Downloads"), true, "ls");
    expect(lookingOnly("du -sh ~/Videos | sort -h | head"), true, "du pipe");
    expect(lookingOnly("df -h"), true, "df");
    expect(lookingOnly("git log --oneline -5"), true, "git log");
    expect(lookingOnly("pacman -Qi firefox"), true, "pacman -Q");
    expect(lookingOnly("find ~ -name '*.iso'"), true, "find");
    expect(lookingOnly("ip addr"), true, "ip addr");
    expect(lookingOnly("rm -rf ~/x"), false, "rm");
    expect(lookingOnly("ls > out.txt"), false, "redirect");
    expect(lookingOnly("ls; rm x"), false, "semicolon");
    expect(lookingOnly("ls && rm x"), false, "and");
    expect(lookingOnly("ls || rm x"), false, "or");
    expect(lookingOnly("cat $(echo x)"), false, "subshell");
    expect(lookingOnly("cat `x`"), false, "backticks");
    expect(lookingOnly("find ~ -name x -delete"), false, "find -delete");
    expect(lookingOnly("find ~ -exec rm {} +"), false, "find -exec");
    expect(lookingOnly("git push"), false, "git push");
    expect(lookingOnly("git log --output=x"), false, "git log --output");
    expect(lookingOnly("pacman -S foo"), false, "pacman -S");
    expect(lookingOnly("systemctl stop x"), false, "systemctl stop");
    expect(lookingOnly("ip link set eth0 down"), false, "ip link set");
    expect(lookingOnly("cat ~/.ssh/id_ed25519"), false, "ssh key");
    expect(lookingOnly("cat project/.env"), false, ".env");
    expect(lookingOnly("printenv"), false, "printenv all");
    expect(lookingOnly("sed -i s/a/b/ f"), false, "sed");
    expect(lookingOnly("awk 'BEGIN{system(\"x\")}'"), false, "awk");
    expect(lookingOnly("ls\nrm x"), false, "newline");
    // free paths
    const QString home = QDir::homePath();
    expect(freePath(home + "/Documents/notes.txt"), true, "documents");
    expect(freePath(home + "/Downloads/a/b.iso"), true, "downloads");
    expect(freePath(home + "/.zshrc"), false, "dotfile");
    expect(freePath(home + "/.config/x"), false, "config");
    expect(freePath(home + "/a/../.ssh/k"), false, "dotdot");
    expect(freePath("/etc/passwd"), false, "outside");
    expect(freePath(home), false, "home itself");
    // a link in home into a dotfolder
    QTemporaryDir scratch(home + "/pebble-rules-XXXXXX");
    QFile::link(home + "/.config", scratch.path() + "/cfg");
    expect(freePath(scratch.path() + "/cfg/zed/settings.json"), false, "link into .config");
    expect(freePath(scratch.path() + "/new.txt"), true, "new file in a plain folder");
    std::printf(failures ? "%d failed\n" : "all passed\n", failures);
    return failures ? 1 : 0;
}
