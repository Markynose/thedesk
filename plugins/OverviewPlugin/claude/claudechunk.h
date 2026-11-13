/****************************************
 *
 *   theDesk - Claude's usage limits in the bar
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 * *************************************/
#ifndef CLAUDECHUNK_H
#define CLAUDECHUNK_H

#include <chunk.h>

// The Claude logo, a ring for this session and one for this week, and the
// session's figure; clicking opens the Overview (its Claude card).
// While a session works the logo's rays move; while one waits on the person
// the chunk fills and says so
class ClaudeChunk : public Chunk {
        Q_OBJECT
    public:
        ClaudeChunk();
        ~ClaudeChunk() override;

        QString name() override;
        int expandedHeight() override;
        int statusBarHeight() override;
        QSize sizeHint() const override;

        // Shows it in the bar while there's a reading
        void follow();

    protected:
        void paintEvent(QPaintEvent* event) override;

    private:
        struct Private;
        Private* d;
};

#endif // CLAUDECHUNK_H
