#include "clipboardhistory.h"

#include <QClipboard>
#include <QDir>
#include <QGuiApplication>
#include <QMimeData>
#include <QRegularExpression>
#include <QStandardPaths>

namespace {
    constexpr int MaxEntries = 40;
    constexpr int MaxImages = 10; // a screenshot is a few MB

    QString pinsDir() {
        return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/thedesk/clipboard");
    }
} // namespace

struct ClipboardHistory::Private {
        QList<Entry> entries;
        quint64 nextId = 1;
        bool setting = false; // our own write to the clipboard
        QString quiet;        // put there to be typed, not remembered
};

ClipboardHistory* ClipboardHistory::instance() {
    static auto* history = new ClipboardHistory();
    return history;
}

ClipboardHistory::ClipboardHistory() :
    QObject(nullptr) {
    d = new Private();

    // Pins from earlier sessions, in their saved order
    QDir dir(pinsDir());
    for (const QFileInfo& file : dir.entryInfoList(QDir::Files, QDir::Name)) {
        Entry entry{d->nextId++, {}, {}, file.lastModified(), true};
        if (file.suffix() == QStringLiteral("png")) {
            entry.image.load(file.filePath());
        } else {
            QFile f(file.filePath());
            if (f.open(QFile::ReadOnly)) entry.text = QString::fromUtf8(f.readAll());
        }
        entry.secret = looksLikePassword(entry.text);
        if (!entry.text.isEmpty() || !entry.image.isNull()) d->entries.append(entry);
    }

    connect(QGuiApplication::clipboard(), &QClipboard::dataChanged, this, &ClipboardHistory::capture);
    capture();
}

bool ClipboardHistory::looksLikePassword(const QString& text) {
    // One word, password-length, mixing at least three kinds of character,
    // and not something with a shape of its own (a link, a path, an address)
    const QString t = text.trimmed();
    if (t.size() < 8 || t.size() > 64) return false;
    if (t.contains(QRegularExpression(QStringLiteral("\\s")))) return false;
    if (t.contains(QStringLiteral("://")) || t.startsWith('/') || t.startsWith('~') || t.startsWith(QStringLiteral("./"))) return false;
    if (t.contains(QRegularExpression(QStringLiteral("^[\\w-]+\\.[A-Za-z]{1,4}$")))) return false; // a file name
    if (t.contains(QRegularExpression(QStringLiteral("^[^@]+@[^@]+\\.[a-z]{2,}$"), QRegularExpression::CaseInsensitiveOption))) return false;
    bool lower = false, upper = false, digit = false, symbol = false;
    for (const QChar c : t) {
        if (c.isLower()) lower = true;
        else if (c.isUpper()) upper = true;
        else if (c.isDigit()) digit = true;
        else symbol = true;
    }
    return lower + upper + digit + symbol >= 3;
}

QList<ClipboardHistory::Entry> ClipboardHistory::entries() const {
    return d->entries;
}

void ClipboardHistory::capture() {
    if (d->setting) return;
    const QMimeData* mime = QGuiApplication::clipboard()->mimeData();
    if (!mime) return;
    // Password managers mark what they copy as secret
    if (mime->data(QStringLiteral("x-kde-passwordManagerHint")) == "secret") return;
    if (!d->quiet.isEmpty() && !mime->hasImage() && mime->text() == d->quiet) return;

    Entry entry{0, {}, {}, QDateTime::currentDateTime(), false};
    if (mime->hasImage()) {
        entry.image = qvariant_cast<QImage>(mime->imageData());
    } else if (mime->hasText()) {
        entry.text = mime->text();
        entry.secret = looksLikePassword(entry.text);
    }
    if (entry.image.isNull() && entry.text.trimmed().isEmpty()) return;

    // Copied again: it moves to the top and keeps its pin
    for (int i = 0; i < d->entries.size(); i++) {
        const Entry& old = d->entries.at(i);
        const bool same = entry.image.isNull() ? old.text == entry.text : old.image == entry.image;
        if (!same) continue;
        entry.pinned = old.pinned;
        d->entries.removeAt(i);
        break;
    }
    entry.id = d->nextId++;
    d->entries.prepend(entry);

    // The oldest unpinned go first
    int images = 0, count = 0;
    for (int i = 0; i < d->entries.size(); i++) {
        const Entry& e = d->entries.at(i);
        if (e.pinned) continue;
        count++;
        if (!e.image.isNull()) images++;
        if (count > MaxEntries || (!e.image.isNull() && images > MaxImages)) {
            d->entries.removeAt(i--);
            count--;
            if (!e.image.isNull()) images--;
        }
    }
    emit changed();
}

void ClipboardHistory::use(quint64 id) {
    for (int i = 0; i < d->entries.size(); i++) {
        Entry entry = d->entries.at(i);
        if (entry.id != id) continue;
        d->entries.move(i, 0);
        d->entries[0].when = QDateTime::currentDateTime();

        auto* mime = new QMimeData();
        if (entry.image.isNull()) mime->setText(entry.text);
        else mime->setImageData(entry.image);
        d->setting = true;
        QGuiApplication::clipboard()->setMimeData(mime);
        d->setting = false;
        emit changed();
        return;
    }
}

void ClipboardHistory::putQuietly(const QString& text) {
    // The change can come back later than setText (X11 tells owners apart
    // asynchronously), so it's recognised by its text
    d->quiet = text;
    QGuiApplication::clipboard()->setText(text);
}

void ClipboardHistory::remove(quint64 id) {
    for (int i = 0; i < d->entries.size(); i++) {
        if (d->entries.at(i).id != id) continue;
        const bool pinned = d->entries.at(i).pinned;
        d->entries.removeAt(i);
        if (pinned) savePins();
        emit changed();
        return;
    }
}

void ClipboardHistory::togglePin(quint64 id) {
    for (Entry& entry : d->entries) {
        if (entry.id != id) continue;
        entry.pinned = !entry.pinned;
        savePins();
        emit changed();
        return;
    }
}

void ClipboardHistory::clear() {
    d->entries.removeIf([](const Entry& e) {
        return !e.pinned;
    });
    emit changed();
}

void ClipboardHistory::savePins() {
    // Written whole each time: there are only ever a few
    QDir dir(pinsDir());
    dir.mkpath(QStringLiteral("."));
    QFile::setPermissions(dir.path(), QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
    for (const QString& file : dir.entryList(QDir::Files)) dir.remove(file);
    int n = 0;
    for (const Entry& entry : std::as_const(d->entries)) {
        if (!entry.pinned) continue;
        const QString base = dir.filePath(QStringLiteral("%1").arg(n++, 4, 10, QChar('0')));
        if (!entry.image.isNull()) {
            entry.image.save(base + QStringLiteral(".png"));
        } else {
            QFile f(base + QStringLiteral(".txt"));
            if (f.open(QFile::WriteOnly)) f.write(entry.text.toUtf8());
        }
    }
}
