/****************************************
 *
 *   theDesk - Material 3 Expressive Status Center overview
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 * *************************************/
#ifndef M3OVERVIEW_H
#define M3OVERVIEW_H

#include <QWidget>

struct M3OverviewPagePrivate;

/**
 * The Status Center overview as a profile page: the user card, then cards
 * for the session, battery, storage and memory, today's date, "your day" on
 * the living horizon's clock, what's playing (with the M3 Expressive wavy
 * seek bar) and live system readings. Cards rise in on springs, numbers count
 * up, and the live readings only tick while the page is visible.
 */
class M3OverviewPage : public QWidget {
        Q_OBJECT
    public:
        explicit M3OverviewPage(QWidget* parent = nullptr);
        ~M3OverviewPage() override;

        static constexpr int ColumnWidth = 1000;

    protected:
        void showEvent(QShowEvent* event) override;
        void hideEvent(QHideEvent* event) override;

    private:
        M3OverviewPagePrivate* d;
        void refresh();
};

#endif // M3OVERVIEW_H
