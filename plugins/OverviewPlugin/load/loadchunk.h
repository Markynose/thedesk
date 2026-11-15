/****************************************
 *
 *   theDesk - the bar's CPU and memory
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 * *************************************/
#ifndef LOADCHUNK_H
#define LOADCHUNK_H

#include <chunk.h>

/**
 * The bar's CPU and memory: the load is a MaterialShape that spikes (circle,
 * Cookie9, SoftBurst) and turns as the CPU gets busier, filling from the
 * bottom with the memory in use, the figure beside it. Its panel grows out of
 * it: the CPU in big type, a shape per thread, memory as a wavy bar and the
 * programs using the most. /proc is read every 2 s (1 s with the panel open)
 * while the bar can be seen.
 */
class LoadChunk : public Chunk {
        Q_OBJECT
    public:
        LoadChunk();
        ~LoadChunk() override;

        QString name() override;
        int expandedHeight() override;
        int statusBarHeight() override;

    protected:
        QWidget* quickWidget() override;
        void paintEvent(QPaintEvent* event) override;
        void changeEvent(QEvent* event) override;

    private:
        struct Private;
        Private* d;

        void sample();
        void frame();
        void startFrames();
        void updatePolling();
        void updateCovered();
        void fit();
        void recolour();
        QRect shapeRect() const;
};

#endif // LOADCHUNK_H
