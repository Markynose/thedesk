/****************************************
 *
 *   theDesk - how to use a newly connected display
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 * *************************************/
#ifndef DISPLAYCHOOSER_H
#define DISPLAYCHOOSER_H

#include <QAbstractButton>
#include <QWidget>

struct DisplayChoiceTilePrivate;

// One way to use the displays: a drawing of the two screens and its name
class DisplayChoiceTile : public QAbstractButton {
        Q_OBJECT
    public:
        enum Kind {
            Extend,
            Mirror,
            OnlyMain,
            OnlyOther
        };

        DisplayChoiceTile(Kind kind, bool laptop, QString label, bool recommended, QWidget* parent = nullptr);
        ~DisplayChoiceTile() override;

        QSize sizeHint() const override;

    protected:
        void paintEvent(QPaintEvent* event) override;
        bool event(QEvent* event) override;
        void keyPressEvent(QKeyEvent* event) override;

    private:
        DisplayChoiceTilePrivate* d;
};

struct DisplayChooserPrivate;

/**
 * The sheet on the main screen when a display is connected: extend (the
 * recommended one), mirror, only the main screen, only the new display.
 * Emits chosen() with a NewDisplayController::Choice.
 */
class DisplayChooser : public QWidget {
        Q_OBJECT
    public:
        DisplayChooser(QString otherName, bool laptop, QWidget* parent = nullptr);
        ~DisplayChooser() override;

        int preferredHeight() const; // the sheet's height
        QRect tileRect(int choice) const; // a choice's tile, on the screen

    signals:
        void chosen(int choice);

    protected:
        void showEvent(QShowEvent* event) override;

    private:
        DisplayChooserPrivate* d;
};

#endif // DISPLAYCHOOSER_H
