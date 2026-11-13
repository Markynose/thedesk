/****************************************
 *
 *   theDesk - System Settings: About, redesigned
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 * *************************************/
#ifndef ABOUTCARDS_H
#define ABOUTCARDS_H

#include <QWidget>
#include <functional>

class QPushButton;
struct AboutPagePrivate;

/**
 * About, as one page: "theDesk" playing the onboarding's opening and coming
 * to rest as the title (a click plays it again), the version and a row of
 * chips, then cards: the system (the distribution's logo and name, what
 * it's based on, its release), this device (its model, its name, Rename)
 * and the specs (each with a Material symbol).
 */
class AboutPage : public QWidget {
        Q_OBJECT
    public:
        struct Spec {
                QString symbol, label, value;
        };

        explicit AboutPage(QWidget* parent = nullptr);
        ~AboutPage() override;

        void setSpecs(const QList<Spec>& specs);
        void setHostname(const QString& hostname);
        // The chips under the title
        QPushButton* addChip(const QString& symbolName, const QString& text, std::function<void()> clicked);
        // Rename on the device card
        void onRename(std::function<void()> rename);

        // Opening the page: the first time in a session the title plays
        void opened();

    protected:
        void resizeEvent(QResizeEvent* event) override;

    private:
        AboutPagePrivate* d;
};

#endif // ABOUTCARDS_H
