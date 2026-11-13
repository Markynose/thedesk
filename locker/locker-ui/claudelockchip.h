/****************************************
 *
 *   theDesk - Claude on the lock screen
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 * *************************************/
#ifndef CLAUDELOCKCHIP_H
#define CLAUDELOCKCHIP_H

#include <QWidget>

// Under the lock screen's date, while a Claude Code session is open: its
// logo and "Claude is working" (the rays move), "Claude needs you" (filled:
// a permission, a question or a plan waits) or "Claude is done" (a turn
// that ended while the screen was locked). Nothing otherwise, and never the
// project's name: other people see the lock screen
class ClaudeLockChip : public QWidget {
        Q_OBJECT
    public:
        explicit ClaudeLockChip(QWidget* parent = nullptr);
        ~ClaudeLockChip() override;

        QSize sizeHint() const override;

    protected:
        void paintEvent(QPaintEvent* event) override;

    private:
        struct Private;
        Private* d;
        void follow();
};

#endif // CLAUDELOCKCHIP_H
