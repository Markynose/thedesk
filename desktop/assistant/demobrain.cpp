#include "demobrain.h"

#include <QTimer>

void DemoBrain::later(int ms, std::function<void()> what) {
    const int forTurn = turn;
    QTimer::singleShot(ms, Qt::PreciseTimer, this, [this, forTurn, what] { // precise: a long answer's parts land in order
        if (forTurn == turn) what(); // stopped turns say nothing more
    });
}

void DemoBrain::say(int at, QString text) {
    // streams in a few words at a time, like the real thing
    const QStringList words = text.split(' ');
    for (int i = 0; i < words.size(); i += 3) {
        const QString part = words.mid(i, 3).join(' ') + (i + 3 < words.size() ? " " : "");
        later(at + i * 40, [this, part] {
            emit textArrived(part);
        });
    }
}

void DemoBrain::send(QString text, QStringList context, bool harder) {
    Q_UNUSED(harder)
    Q_UNUSED(context)
    turn++;
    const QString t = text.toLower();
    if (t.contains("dim") || t.contains("dnd")) {
        later(300, [this] { emit stepStarted("b", tr("Setting brightness…")); });
        later(1100, [this] {
            emit stepFinished("b", tr("Brightness 30%"), true);
            emit stepStarted("d", tr("Turning on do not disturb…"));
        });
        later(1800, [this] { emit stepFinished("d", tr("Do not disturb till 18:00"), true); });
        say(2000, tr("Done. Screen's at 30% and do not disturb is on till 6."));
        later(2400, [this] {
            emit cardAdded({"brightness", AssistantCard::Level, tr("Brightness"), {}, 0.3, {}, {}, "brightness_6"});
            emit cardAdded({"dnd", AssistantCard::Switch, tr("Do not disturb"), tr("Until 18:00 · calls still ring"), 1, {}, {}, "do_not_disturb_on"});
        });
        later(2700, [this] {
            emit chipsOffered({{"undo", tr("Undo"), "undo"}, {"till7", tr("Till 7 instead"), {}}, {"night", tr("Night light too"), "nightlight"}});
            emit turnFinished();
        });
    } else if (t.contains("css")) {
        later(300, [this] { emit stepStarted("r1", tr("Reading index.html…")); });
        later(1000, [this] {
            emit stepFinished("r1", tr("Read index.html"), false);
            emit stepStarted("r2", tr("Reading style.css…"));
        });
        later(1600, [this] {
            emit stepFinished("r2", tr("Read style.css"), false);
            emit stepStarted("e", tr("Editing style.css…"));
        });
        later(6000, [this] { emit stepFinished("e", tr("Edited style.css"), true); });
        say(6200, tr("Tidied style.css: the cards use flexbox now and 3 unused rules are gone."));
        later(6600, [this] {
            emit cardAdded({"code", AssistantCard::Code, "style.css", {}, 0, QStringLiteral("~/site/style.css"), {" .cards {", "-  float: left;", "-  width: 31%;", "+  display: flex;", "+  gap: 16px;", " }"}});
            emit cardAdded({"file", AssistantCard::File, "style.css", tr("~/site · edited just now · 3 rules removed"), 0, QStringLiteral("~/site/style.css"), {}});
        });
        later(6900, [this] {
            emit chipsOffered({{"undo", tr("Undo"), "undo"}, {"diff", tr("Show the whole change"), {}}});
            emit turnFinished();
        });
    } else if (t.contains("style")) {
        say(600, QStringLiteral("{done} Words can go <t wght=100-1000>thin to heavy</t>, <t wdth=151 fx=stretch>wide</t>, <t slnt=-10 color=tertiary>leaning</t>, <t size=1.8 wght=900 fx=pop>big</t> or <t fx=wave color=primary wght=700>do a little wave</t>."));
        later(1600, [this] { emit turnFinished(); });
    } else if (t.contains("disk") || t.contains("space")) {
        // a long answer: a list and a code block (as the real one sends it: the block as a card mid-text)
        later(300, [this] { emit stepStarted("l", tr("Looking at your disk…")); });
        later(1100, [this] {
            emit stepFinished("l", tr("Looked at your disk"), false);
            emit stepStarted("w", tr("Writing it up…"));
        });
        if (t.contains("full")) later(900, [this] { // "how full": with another card, it waits rolled up
            emit cardAdded({"disk", AssistantCard::Storage, tr("Home"), tr("41 GB free of 238 GB"), 0.83, QStringLiteral("/home"), {}});
        });
        say(1500, QStringLiteral("{done} Most of it hides in **caches,** old **packages** and **logs,** so clear those first.\nFirst, see where the space went:\n"));
        later(2300, [this] {
            emit cardAdded({"cmds", AssistantCard::Code, "bash", {}, 0, "bash", {" du -sh ~/.cache ~/Downloads /var/log", " sudo journalctl --vacuum-size=200M"}});
        });
        say(2400, QStringLiteral("1. **Package cache:** your package manager keeps every download it ever made, and paccache -rk1 keeps only the newest one of each.\n"
            "2. **Logs:** the system journal grows until you trim it; the second command above keeps it under **200** MB.\n"
            "3. **~/.cache:** safe to empty, since browsers and apps rebuild only what they actually use.\n"
            "4. **Orphans:** packages nothing depends on any more; list them first and remove the ones you don't need.\n"
            "5. **Downloads and Trash:** often the biggest win of all, so sort by size and empty the Trash when you're done.\n"
            "Then run df -h again: you should have a few **GB** back :)"));
        later(6200, [this] {
            emit stepFinished("w", tr("Wrote it up"), false);
            emit chipsOffered({{"clean", tr("Clean the cache"), "cleaning_services"}, {"big", tr("Find big files"), {}}});
            emit turnFinished();
        });
    } else {
        say(900, tr("I'm a stand-in for now, so I can only do the screen and dnd one, the css one or how to free up disk space. The real me comes next."));
        later(1500, [this] { emit turnFinished(); });
    }
}

bool DemoBrain::chip(QString chipId) {
    if (chipId == "till7") {
        emit cardChanged({"dnd", AssistantCard::Switch, tr("Do not disturb"), tr("Until 19:00 · calls still ring"), 1, {}, {}, "do_not_disturb_on"});
        emit chipsOffered({{"undo", tr("Undo"), "undo"}, {"till6", tr("Till 6 again"), {}}, {"night", tr("Night light too"), "nightlight"}});
    } else if (chipId == "till6") {
        emit cardChanged({"dnd", AssistantCard::Switch, tr("Do not disturb"), tr("Until 18:00 · calls still ring"), 1, {}, {}, "do_not_disturb_on"});
        emit chipsOffered({{"undo", tr("Undo"), "undo"}, {"till7", tr("Till 7 instead"), {}}, {"night", tr("Night light too"), "nightlight"}});
    }
    return true;
}

void DemoBrain::setCardValue(QString cardId, double value) {
    Q_UNUSED(cardId)
    Q_UNUSED(value)
}

void DemoBrain::stop() {
    turn++;
    emit turnFinished();
}
