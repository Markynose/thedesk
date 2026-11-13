#ifndef DEMOBRAIN_H
#define DEMOBRAIN_H

#include "assistantbrain.h"

// A scripted stand-in for tests (TD_ASSISTANT_DEMO=1): it answers a couple of
// requests with cards and changes nothing on the machine
class DemoBrain : public AssistantBrain {
        Q_OBJECT
    public:
        using AssistantBrain::AssistantBrain;

        void send(QString text, QStringList context, bool harder) override;
        bool chip(QString chipId) override;
        void setCardValue(QString cardId, double value) override;
        void stop() override;

    private:
        int turn = 0;
        void later(int ms, std::function<void()> what);
        void say(int at, QString text);
};

#endif // DEMOBRAIN_H
