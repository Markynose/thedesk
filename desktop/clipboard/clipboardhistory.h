#ifndef CLIPBOARDHISTORY_H
#define CLIPBOARDHISTORY_H

#include <QDateTime>
#include <QImage>
#include <QObject>

// What was copied this session, newest first. Kept in memory only; pinned
// entries are saved to ~/.local/share/thedesk/clipboard
class ClipboardHistory : public QObject {
        Q_OBJECT
    public:
        struct Entry {
                quint64 id;
                QString text;
                QImage image; // set for images, text empty
                QDateTime when;
                bool pinned = false;
                bool secret = false; // looks like a password: shown as dots
        };

        static ClipboardHistory* instance();
        static bool looksLikePassword(const QString& text);

        QList<Entry> entries() const;
        // Put an entry back on the clipboard (and to the top)
        void use(quint64 id);
        void remove(quint64 id);
        void togglePin(quint64 id);
        void clear(); // everything but the pins
        // Onto the clipboard without joining the history (a typed emoji)
        void putQuietly(const QString& text);

    signals:
        void changed();

    private:
        explicit ClipboardHistory();
        struct Private;
        Private* d;

        void capture();
        void savePins();
};

#endif // CLIPBOARDHISTORY_H
