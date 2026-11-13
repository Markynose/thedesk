#ifndef CLAUDEBRAIN_H
#define CLAUDEBRAIN_H

#include "assistantbrain.h"

class QJsonObject;

// The real brain: a headless Claude Code session (the user's own login) that
// stays open while the thread lives. theDesk hosts its tools in-process (an
// "sdk" MCP server over the same pipe) and answers its permission asks with
// cards in the panel. A quick model by default; a bigger one, with more
// reasoning, for heavy jobs. It never names the models in the panel.
class ClaudeBrain : public AssistantBrain {
        Q_OBJECT
    public:
        explicit ClaudeBrain(QObject* parent = nullptr);
        ~ClaudeBrain();

        void warm() override;
        void send(QString text, QStringList context, bool harder) override;
        bool chip(QString chipId) override;
        void setCardValue(QString cardId, double value) override;
        void answer(QString askId, bool allow) override;
        void stop() override;
        void forget() override;
        void setScreen(const QImage& screen) override;

        // Is this message a heavy job? (code, files, long writing)
        static bool looksHeavy(const QString& text, const QStringList& context);

        // For the settings page: what it remembers, and its prompt file
        static QStringList remembered();
        static void setRemembered(const QStringList& facts);
        static QString promptPath();
        static void resetPrompt();

    private:
        struct Private;
        Private* d;

        void start();
        void write(const QJsonObject& object);
        void handleLine(const QByteArray& line);
        void handleControl(const QJsonObject& message);
        QJsonObject handleMcp(const QJsonObject& message);
        void setDepth(int depth);
        void deepen();
        void takeText(const QString& text, bool final);
        void fail(const QString& why);
        QString stepText(const QString& tool, const QJsonObject& input, bool done) const;
};

#endif // CLAUDEBRAIN_H
