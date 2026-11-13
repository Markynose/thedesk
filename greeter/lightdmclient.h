/****************************************
 *
 *   theDesk - LightDM greeter
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 * *************************************/
#ifndef LIGHTDMCLIENT_H
#define LIGHTDMCLIENT_H

#include <QList>
#include <QObject>
#include <QString>

/**
 * liblightdm-gobject as Qt signals and plain Qt types. (Artix's
 * liblightdm-qt6 can't be linked against: its symbols never resolve.) Qt
 * runs on GLib's event loop here, so LightDM's GObject signals arrive as
 * usual. The GLib headers stay in the .cpp: they clash with Qt's keywords.
 */
class LightDMClient : public QObject {
        Q_OBJECT
    public:
        struct User {
                QString name, displayName, image, session;
        };
        struct Session {
                QString key, name;
        };

        explicit LightDMClient(QObject* parent = nullptr);
        ~LightDMClient() override;

        bool connectToDaemon();
        QString selectUserHint() const;
        QString defaultSessionHint() const;
        QList<User> users() const;
        QList<Session> sessions() const;

        void authenticate(const QString& user);
        void respond(const QString& response);
        bool isAuthenticated() const;
        bool startSession(const QString& session);
        QString sharedDataDir(const QString& user); // what the user's session left for us

        bool canSuspend() const;
        bool canRestart() const;
        bool canShutdown() const;
        void suspend();
        void restart();
        void shutdown();

    Q_SIGNALS:
        void showPrompt(QString text, bool secret);
        void showMessage(QString text, bool error);
        void authenticationComplete();

    private:
        struct Private;
        Private* d;
};

#endif // LIGHTDMCLIENT_H
