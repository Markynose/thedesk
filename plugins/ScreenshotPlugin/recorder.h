#ifndef RECORDER_H
#define RECORDER_H

#include <QWidget>
#include <chunk.h>

// Record in the screenshot window hands a region here: a 3-2-1 over it, then
// ffmpeg records it to ~/Videos/Recordings while a red pill in the bar counts.
// The pill or Super+Shift+R stops it
class Recorder : public QObject {
        Q_OBJECT
    public:
        explicit Recorder(QObject* parent = nullptr);
        ~Recorder() override;

        void start(QRect region);
        void stop();
        bool isRecording() const;

    private:
        struct Private;
        Private* d;
};

// The bar's red pill while recording
class RecordingChunk : public Chunk {
        Q_OBJECT
    public:
        RecordingChunk();
        ~RecordingChunk() override;

        void begin();
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

#endif // RECORDER_H
