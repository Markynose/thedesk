#ifndef ASSISTANTBRAIN_H
#define ASSISTANTBRAIN_H

#include <QImage>
#include <QList>
#include <QObject>
#include <QStringList>

// Something the assistant changed or made, shown as a card in the thread
struct AssistantCard {
        enum Kind {
            Level,  // a slider: icon, title, value 0..1
            Switch, // icon, title, subtitle ("Until 18:00"), value 0/1
            Code,   // path (or a language) + lines, each starting with ' ', '+' or '-'
            File,   // path + subtitle
            Ask,    // a permission ask: title, lines (what it wants to do); value 0 asking, 1 allowed, -1 not
            Image,  // path: a file or a URL; title: a caption
            Storage, // a disk: title, subtitle ("41 GB free of 238 GB"), value = used 0..1,
                     // path = where it's mounted, lines = its biggest folders ("name\tbytes\tpath")
            Battery,       // value 0..1, title: its state, subtitle: time left, icon
            Media,         // path: the player (it's read live), title/subtitle as it was asked
            Notifications, // lines: "app\ticon\tsummary\tbody\tHH:mm", one tile each
            Files,         // lines: paths, one tile each with a preview
            Timer,         // title: what for, value: when it's due (ms since the epoch), lines: [when it was set]
            Weather        // title: the place, subtitle: the sky, value: °C now, icon: the sky's kind,
                           // lines: the next hours ("HH\t°C\tkind")
        };
        QString id;
        Kind kind = Level;
        QString title, subtitle;
        double value = 0;
        QString path;
        QStringList lines;
        QString icon;
};

// A follow-up suggestion under an answer
struct AssistantChip {
        QString id, label, icon;
};

// What answers: the panel only talks to this
class AssistantBrain : public QObject {
        Q_OBJECT
    public:
        using QObject::QObject;

        virtual void warm() {} // the panel opened: get ready for a message
        // context: what the panel may show it ("Active window: …", "Clipboard: …");
        // harder: the user asked it to think harder about this one
        virtual void send(QString text, QStringList context, bool harder) = 0;
        virtual bool chip(QString chipId) = 0; // true: handled in place; false: the panel sends the label
        virtual void setCardValue(QString cardId, double value) = 0; // moved by hand
        virtual void answer(QString askId, bool allow) { Q_UNUSED(askId) Q_UNUSED(allow) }
        virtual void stop() = 0;
        virtual void forget() {} // a new thread
        virtual void setScreen(const QImage& screen) { Q_UNUSED(screen) } // what was on screen when the panel opened (null: not allowed)

    signals:
        void stepStarted(QString id, QString text);
        void stepFinished(QString id, QString text, bool shownAsCard);
        // more of the current answer; it may hold markup the panel shows as
        // tone ({done} {unsure} {oops}), key words (**30%**) and chips ([[file:…]] [[app:…]])
        void textArrived(QString text);
        void cardAdded(AssistantCard card);
        void cardChanged(AssistantCard card);
        void chipsOffered(QList<AssistantChip> chips);
        void deepChanged(bool deep); // thinking harder: the face turns tertiary
        void turnFinished();
};

#endif // ASSISTANTBRAIN_H
