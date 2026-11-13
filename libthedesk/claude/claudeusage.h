/****************************************
 *
 *   theDesk - Claude's usage limits
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 * *************************************/
#ifndef CLAUDEUSAGE_H
#define CLAUDEUSAGE_H

#include <QDateTime>
#include <QObject>
#include "libthedesk_global.h"

class QPainter;
struct ClaudeUsagePrivate;

/**
 * The 5-hour and weekly limits of a Claude subscription, as Claude Code
 * last measured them. The thedesk-claude-usage Claude Code mod writes them
 * to ~/.cache/thedesk/claude-usage.json whenever they move; this reads that
 * file (and watches it). Only as fresh as the last Claude Code session.
 *
 * A limit crossing 80, 90 or 100% shows a snackbar.
 *
 * The mod also says what each open session is doing
 * (claude-session-<id>.json beside the limits): activity() is the most
 * pressing of them, and a session that starts waiting on the person (a
 * permission, a question, a plan to approve) shows a snackbar.
 */
class LIBTHEDESK_EXPORT ClaudeUsage : public QObject {
        Q_OBJECT
    public:
        struct Limit {
                bool known = false;
                double percent = 0; // 0-100
                QDateTime resetsAt;
        };

        enum Activity {
            Idle,    // no session, or none in a turn
            Working, // a session is in a turn
            Waiting  // a session waits on the person
        };

        static ClaudeUsage* instance();

        // The most pressing thing an open session is doing
        Activity activity() const;
        // "thedesk wants to run a command", of the session that waits
        QString waitingText() const;
        // When a session last answered and has been resting since (the lock
        // screen says so, of a turn that ended while it was up)
        QDateTime finished() const;

        // Snackbars (a limit crossed, a session that starts waiting) are the
        // desktop's to show: off until it turns them on
        void setAnnounces(bool announces);

        // There's a reading to show
        bool available() const;
        Limit session() const; // the 5-hour window
        Limit week() const;
        QDateTime updated() const;

        // "Resets in 2 h 10 min", "Resets Friday at 14:00"
        static QString resetText(const Limit& limit);
        // "Updated just now", "Updated 5 min ago"
        QString updatedText() const;

        // The Claude logo, in its own colour. While Claude works its rays
        // grow from the middle and draw back in: phase runs 0-1 through one
        // beat; below 0 the logo rests, whole
        static void paintLogo(QPainter* painter, const QRectF& rect, double phase = -1);

    signals:
        void changed();

    private:
        explicit ClaudeUsage();
        ClaudeUsagePrivate* d;
        void read();
        void readSessions();
};

#endif // CLAUDEUSAGE_H
