#ifndef ASSISTANTCHUNK_H
#define ASSISTANTCHUNK_H

#include <chunk.h>

// The assistant in the bar while it works with its sheet put away:
// the loading indicator and what it's doing; tonal with a check once done
class AssistantChunk : public Chunk {
        Q_OBJECT
    public:
        explicit AssistantChunk();
        ~AssistantChunk();

        void setState(bool done, QString text);

        QSize sizeHint() const override;
        QString name() override;
        int expandedHeight() override;
        int statusBarHeight() override;

    protected:
        void paintEvent(QPaintEvent* event) override;

    private:
        struct Private;
        Private* d;
};

#endif // ASSISTANTCHUNK_H
