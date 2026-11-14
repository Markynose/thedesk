#ifndef PEBBLELIVE_H
#define PEBBLELIVE_H

#include <QString>
#include <functional>

// Live updates: things Pebble set going that are still running (a reminder
// counting down, a download it's watching) show as a chip in the bar, like
// Android's live updates. Clicking it opens Pebble.
namespace PebbleLive {
    void setUp(std::function<void()> open);
    // due: when it ends (ms since the epoch) for a countdown, or 0 while it simply runs
    int add(const QString& text, qint64 due = 0, qint64 since = 0);
    void remove(int id);
} // namespace PebbleLive

#endif // PEBBLELIVE_H
