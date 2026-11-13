#ifndef PEBBLESETTINGS_H
#define PEBBLESETTINGS_H

#include <statuscenterpane.h>

// System Settings > Pebble: its colour, the tour, nudges, the selection
// button, what it remembers and its prompt
class PebbleSettings : public StatusCenterPane {
        Q_OBJECT
    public:
        explicit PebbleSettings();
        ~PebbleSettings();

        QString name() override;
        QString displayName() override;
        QIcon icon() override;
        QWidget* leftPane() override;

    protected:
        void showEvent(QShowEvent* event) override;

    private:
        struct Private;
        Private* d;
        void refreshMemories();
};

#endif // PEBBLESETTINGS_H
