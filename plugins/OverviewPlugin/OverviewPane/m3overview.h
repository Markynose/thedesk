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

// M3 Expressive wavy progress (WavyProgressIndicator); `amplitude` 0..1 (0 flat)
void paintWave(QPainter* p, const QRectF& track, double value, double amplitude, double phase, QColor active, QColor inactive, double stroke = 4);

/**
 * The Status Center overview: your day as a ribbon on the living horizon's
 * clock with the big clock riding the "now" line, world clocks as pins at
 * their own hour and the greeting; then what's playing (with the M3
 * Expressive wavy seek bar), usage, live system readings and the session,
 * battery, storage and memory as pills. Cards rise in on springs, numbers
 * count up, and the live readings only tick while the page is visible.
 */
class M3OverviewPage : public QWidget {
        Q_OBJECT
    public:
        explicit M3OverviewPage(QWidget* parent = nullptr);
        ~M3OverviewPage() override;

        static constexpr int ColumnWidth = 1160;

        // A short line under the date (the clocks are about to change) and its full text on hover
        void setNotice(const QString& notice, const QString& details);

    protected:
        void showEvent(QShowEvent* event) override;
        void hideEvent(QHideEvent* event) override;

    private:
        M3OverviewPagePrivate* d;
        void refresh();
};

#endif // M3OVERVIEW_H
