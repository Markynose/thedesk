#ifndef PEBBLENUDGES_H
#define PEBBLENUDGES_H

#include <QString>

// Pebble speaking first: now and then (at most one at a time, each kind once
// in a few days) a snackbar about something it could sort out, whose button
// opens Pebble with the job asked. Local checks only: no AI until tapped.
namespace PebbleNudges {
    void setUp();
    // a snackbar action id that opens Pebble and asks `prompt`
    QString askAction(const QString& prompt);
} // namespace PebbleNudges

#endif // PEBBLENUDGES_H
