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

// GLib first: Qt's "signals" keyword would break its headers
#include <lightdm.h>

#include "lightdmclient.h"

struct LightDMClient::Private {
        LightDMGreeter* greeter = nullptr;
};

namespace {
    QString str(const gchar* s) {
        return s ? QString::fromUtf8(s) : QString();
    }

    void onShowPrompt(LightDMGreeter*, const gchar* text, LightDMPromptType type, gpointer self) {
        emit static_cast<LightDMClient*>(self)->showPrompt(str(text), type == LIGHTDM_PROMPT_TYPE_SECRET);
    }

    void onShowMessage(LightDMGreeter*, const gchar* text, LightDMMessageType type, gpointer self) {
        emit static_cast<LightDMClient*>(self)->showMessage(str(text), type == LIGHTDM_MESSAGE_TYPE_ERROR);
    }

    void onAuthenticationComplete(LightDMGreeter*, gpointer self) {
        emit static_cast<LightDMClient*>(self)->authenticationComplete();
    }

    bool ok(gboolean result, GError* error) {
        if (error) {
            qWarning("thedesk-greeter: %s", error->message);
            g_error_free(error);
        }
        return result;
    }
} // namespace

LightDMClient::LightDMClient(QObject* parent) :
    QObject(parent) {
    d = new Private();
    d->greeter = lightdm_greeter_new();
    g_signal_connect(d->greeter, LIGHTDM_GREETER_SIGNAL_SHOW_PROMPT, G_CALLBACK(onShowPrompt), this);
    g_signal_connect(d->greeter, LIGHTDM_GREETER_SIGNAL_SHOW_MESSAGE, G_CALLBACK(onShowMessage), this);
    g_signal_connect(d->greeter, LIGHTDM_GREETER_SIGNAL_AUTHENTICATION_COMPLETE, G_CALLBACK(onAuthenticationComplete), this);
}

LightDMClient::~LightDMClient() {
    g_signal_handlers_disconnect_by_data(d->greeter, this);
    g_object_unref(d->greeter);
    delete d;
}

bool LightDMClient::connectToDaemon() {
    // Not resettable: LightDM would keep this greeter idling on its X server
    // and start the session on a second one, switching consoles (a flash of
    // a text console) and losing the hand-off to the loading screen. Without
    // it the session takes over this X server. (Must be set before connecting.)
    lightdm_greeter_set_resettable(d->greeter, FALSE);
    GError* error = nullptr;
    return ok(lightdm_greeter_connect_to_daemon_sync(d->greeter, &error), error);
}

QString LightDMClient::selectUserHint() const {
    return str(lightdm_greeter_get_select_user_hint(d->greeter));
}

QString LightDMClient::defaultSessionHint() const {
    return str(lightdm_greeter_get_default_session_hint(d->greeter));
}

QList<LightDMClient::User> LightDMClient::users() const {
    QList<User> out;
    for (GList* item = lightdm_user_list_get_users(lightdm_user_list_get_instance()); item; item = item->next) {
        auto* user = static_cast<LightDMUser*>(item->data);
        out.append({str(lightdm_user_get_name(user)), str(lightdm_user_get_display_name(user)), str(lightdm_user_get_image(user)), str(lightdm_user_get_session(user))});
    }
    return out;
}

QList<LightDMClient::Session> LightDMClient::sessions() const {
    QList<Session> out;
    for (GList* item = lightdm_get_sessions(); item; item = item->next) {
        auto* session = static_cast<LightDMSession*>(item->data);
        out.append({str(lightdm_session_get_key(session)), str(lightdm_session_get_name(session))});
    }
    return out;
}

void LightDMClient::authenticate(const QString& user) {
    GError* error = nullptr;
    ok(lightdm_greeter_authenticate(d->greeter, user.toUtf8().constData(), &error), error);
}

void LightDMClient::respond(const QString& response) {
    GError* error = nullptr;
    ok(lightdm_greeter_respond(d->greeter, response.toUtf8().constData(), &error), error);
}

bool LightDMClient::isAuthenticated() const {
    return lightdm_greeter_get_is_authenticated(d->greeter);
}

bool LightDMClient::startSession(const QString& session) {
    GError* error = nullptr;
    return ok(lightdm_greeter_start_session_sync(d->greeter, session.isEmpty() ? nullptr : session.toUtf8().constData(), &error), error);
}

QString LightDMClient::sharedDataDir(const QString& user) {
    GError* error = nullptr;
    gchar* dir = lightdm_greeter_ensure_shared_data_dir_sync(d->greeter, user.toUtf8().constData(), &error);
    ok(dir != nullptr, error);
    QString path = str(dir);
    g_free(dir);
    return path;
}

bool LightDMClient::canSuspend() const {
    return lightdm_get_can_suspend();
}

bool LightDMClient::canRestart() const {
    return lightdm_get_can_restart();
}

bool LightDMClient::canShutdown() const {
    return lightdm_get_can_shutdown();
}

void LightDMClient::suspend() {
    GError* error = nullptr;
    ok(lightdm_suspend(&error), error);
}

void LightDMClient::restart() {
    GError* error = nullptr;
    ok(lightdm_restart(&error), error);
}

void LightDMClient::shutdown() {
    GError* error = nullptr;
    ok(lightdm_shutdown(&error), error);
}
