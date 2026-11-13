/****************************************
 *
 *   theDesk - the desktop's sounds
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 * *************************************/
#ifndef DESKSOUND_H
#define DESKSOUND_H

#include "libthedesk_global.h"
#include <QObject>

struct DeskSoundPrivate;

/**
 * The desktop's sounds, by freedesktop sound name ("message-new-instant",
 * "power-plug", "phone-incoming-call"...), from the sound theme in
 * theDesk.conf [Sounds] theme (a folder in ~/.local/share/sounds or
 * /usr/share/sounds; anything it lacks comes from freedesktop).
 *
 * Each sound is decoded once and kept, so it starts the moment it's asked
 * for (in step with the motion it goes with). Quiet modes apply: Mute
 * silences everything; No notifications and Critical only silence
 * notifications and calls (the caller passes Critical for those that may
 * still be heard). [Sounds] enabled=false turns them all off, and
 * [Sounds] <name>=false one of them.
 */
class LIBTHEDESK_EXPORT DeskSound : public QObject {
        Q_OBJECT
    public:
        enum Kind {
            System,      // charging, devices, screenshots, volume
            Notification,
            Critical,    // a notification that gets through Critical only
            Ringtone     // calls; loops until stopped
        };

        // nullptr when it's silenced or there's no such sound
        static DeskSound* play(const QString& name, Kind kind = System);
        // An app's own sound file (a notification's sound-file hint)
        static DeskSound* playFile(const QString& path, Kind kind);
        // Decode ahead of time, so the first play is instant too
        static void preload(const QStringList& names);
        // The file a name resolves to in the current theme ("" if none)
        static QString location(const QString& name);

        void stop();

        ~DeskSound();

    signals:
        void finished();

    private:
        explicit DeskSound(QObject* parent = nullptr);
        DeskSoundPrivate* d;

        Q_INVOKABLE void start();
        static bool allowed(Kind kind);
        static DeskSound* playPath(const QString& path, Kind kind);
};

#endif // DESKSOUND_H
