#ifndef ONBOARDINGPEBBLE_H
#define ONBOARDINGPEBBLE_H

#include "../libthedesk_global.h"
#include "onboardingpage.h"

// Pebble's four faces, thinking; a click picks its colour (Assistant/face).
// The onboarding's Pebble page and System Settings both use it
class LIBTHEDESK_EXPORT PebbleFacePicker : public QWidget {
        Q_OBJECT
    public:
        explicit PebbleFacePicker(QWidget* parent = nullptr);
        ~PebbleFacePicker() override;
        QSize sizeHint() const override;

    protected:
        void paintEvent(QPaintEvent* event) override;
        void mousePressEvent(QMouseEvent* event) override;
        void showEvent(QShowEvent* event) override;
        void hideEvent(QHideEvent* event) override;

    private:
        struct Private;
        Private* d;
};

// "Meet Pebble": the assistant in a page, and its colour
class OnboardingPebble : public OnboardingPage {
        Q_OBJECT
    public:
        explicit OnboardingPebble(QWidget* parent = nullptr);

        QString name() override;
        QString displayName() override;
};

#endif // ONBOARDINGPEBBLE_H
