#ifndef CUSTOMSHORTCUTS_H
#define CUSTOMSHORTCUTS_H

#include <QObject>

struct CustomShortcutsPrivate;

/**
 * User-defined global shortcuts, read from the [CustomShortcuts] group of the
 * theDesk settings file (~/.config/theSuite/theDesk.conf):
 *
 *   [CustomShortcuts]
 *   Meta+W=firefox
 *   Meta+Return=kitty
 *   Meta+C=@close-window
 *
 * Keys are key sequences (QKeySequence portable text). Values are either a
 * command line to launch, or one of the built-in actions:
 *   @close-window   ask the active window to close
 *   @kill-window    force stop the active window
 *
 * The group is reloaded whenever it changes.
 */
class CustomShortcuts : public QObject {
        Q_OBJECT
    public:
        static CustomShortcuts* instance();

    private:
        explicit CustomShortcuts(QObject* parent = nullptr);
        ~CustomShortcuts();
        CustomShortcutsPrivate* d;

        void reload();
        void trigger(const QString& action);
};

#endif // CUSTOMSHORTCUTS_H
