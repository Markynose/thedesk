/****************************************
 *
 *   theDesk - Screen locker password checker
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   Authenticates the *calling* user through PAM. The password is read as a
 *   single line from stdin.
 *
 *   This binary is intentionally NOT setuid: pam_unix verifies the caller's
 *   own password through its setuid helper (unix_chkpwd), which refuses to
 *   check passwords of other users. That avoids a local password oracle and
 *   works with every hash scheme the system supports (yescrypt, sha512, ...).
 *
 *   Exit codes: 0 = authenticated, 1 = wrong password, 2 = error.
 *
 * *************************************/

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <pwd.h>
#include <security/pam_appl.h>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

namespace {
    // PAM service used for authentication. A dedicated service file is
    // installed to /etc/pam.d; fall back to common stacks if it is missing,
    // because a missing service falls through to "other", which usually denies.
    const char* pamService() {
        static const char* candidates[] = {"thedesk-locker", "system-auth", "login"};
        static const char* dirs[] = {"/etc/pam.d/", "/usr/lib/pam.d/"};
        for (const char* candidate : candidates) {
            for (const char* dir : dirs) {
                struct stat st;
                if (stat((std::string(dir) + candidate).c_str(), &st) == 0) return candidate;
            }
        }
        return "login";
    }

    void secureClear(std::string& str) {
        if (!str.empty()) explicit_bzero(str.data(), str.size());
        str.clear();
    }

    int conversation(int numMessages, const struct pam_message** messages, struct pam_response** responses, void* appData) {
        if (numMessages <= 0 || numMessages > PAM_MAX_NUM_MSG) return PAM_CONV_ERR;

        auto* password = static_cast<const std::string*>(appData);
        auto* reply = static_cast<struct pam_response*>(calloc(numMessages, sizeof(struct pam_response)));
        if (!reply) return PAM_BUF_ERR;

        for (int i = 0; i < numMessages; i++) {
            switch (messages[i]->msg_style) {
                case PAM_PROMPT_ECHO_OFF:
                case PAM_PROMPT_ECHO_ON:
                    reply[i].resp = strdup(password->c_str());
                    if (!reply[i].resp) {
                        for (int j = 0; j < i; j++) {
                            if (reply[j].resp) {
                                explicit_bzero(reply[j].resp, strlen(reply[j].resp));
                                free(reply[j].resp);
                            }
                        }
                        free(reply);
                        return PAM_BUF_ERR;
                    }
                    break;
                case PAM_ERROR_MSG:
                case PAM_TEXT_INFO:
                    if (messages[i]->msg) std::cerr << messages[i]->msg << "\n";
                    break;
                default:
                    break;
            }
        }

        *responses = reply;
        return PAM_SUCCESS;
    }
} // namespace

int main(int argc, char* argv[]) {
    struct passwd* pw = getpwuid(getuid());
    if (!pw || !pw->pw_name) {
        std::cerr << "Unable to determine the current user\n";
        return 2;
    }
    const std::string username = pw->pw_name;

    // For compatibility the username may still be passed as the first argument,
    // but only the calling user can be authenticated.
    if (argc > 1) {
        std::string arg = argv[1];
        if (arg == "-h" || arg == "--help") {
            std::cout << "Usage: " << argv[0] << " [username]\n"
                      << "Reads a password from stdin and authenticates the calling user via PAM.\n";
            return 0;
        }
        if (arg != username) {
            std::cerr << "Can only check the password of the calling user\n";
            return 2;
        }
    }

    std::string password;
    std::getline(std::cin, password);

    struct pam_conv conv = {conversation, &password};
    pam_handle_t* handle = nullptr;
    int result = pam_start(pamService(), username.c_str(), &conv, &handle);
    if (result != PAM_SUCCESS) {
        std::cerr << "pam_start failed: " << pam_strerror(handle, result) << "\n";
        secureClear(password);
        return 2;
    }

    result = pam_authenticate(handle, 0);
    if (result == PAM_SUCCESS) {
        // Refresh credentials (e.g. Kerberos tickets, keyrings); failure is not fatal.
        pam_setcred(handle, PAM_REFRESH_CRED);
    }

    secureClear(password);

    int exitCode;
    switch (result) {
        case PAM_SUCCESS:
            std::cout << "Password correct\n";
            exitCode = 0;
            break;
        case PAM_AUTH_ERR:
        case PAM_USER_UNKNOWN:
        case PAM_MAXTRIES:
        case PAM_PERM_DENIED:
            std::cerr << "Authentication failed: " << pam_strerror(handle, result) << "\n";
            exitCode = 1;
            break;
        default:
            std::cerr << "PAM error: " << pam_strerror(handle, result) << "\n";
            exitCode = 2;
            break;
    }

    pam_end(handle, result);
    return exitCode;
}
