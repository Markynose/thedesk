#ifndef BATTERYHEALTH_H
#define BATTERYHEALTH_H

#include <QWidget>

// Power settings: a card per battery, its charge, which one is draining, and
// how much of its design capacity it still holds
class BatteryHealth : public QWidget {
        Q_OBJECT
    public:
        explicit BatteryHealth(QWidget* parent = nullptr);
        ~BatteryHealth() override;

        QSize sizeHint() const override;

    protected:
        void paintEvent(QPaintEvent* event) override;
        void showEvent(QShowEvent* event) override;
        void hideEvent(QHideEvent* event) override;

    private:
        struct Private;
        Private* d;

        void load();
        void read(const QString& path);
};

#endif // BATTERYHEALTH_H
