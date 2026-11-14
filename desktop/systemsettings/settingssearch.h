/****************************************
 *
 *   theDesk - System Settings search
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 * *************************************/
#ifndef SETTINGSSEARCH_H
#define SETTINGSSEARCH_H

#include <QWidget>
#include <functional>

class StatusCenterPane;
struct SettingsSearchPrivate;

/**
 * Android's settings search as a docked search view: it grows out of the
 * search button over the pane list, and every setting it finds says where
 * it lives ("Power › Buttons · Suspend"). The index is the panes' settings
 * groups (the style publishes their rows), so any pane with groups is
 * searchable without registering anything.
 */
class SettingsSearch : public QWidget {
        Q_OBJECT
    public:
        // `panes` lists the panes to search, in the list's order
        SettingsSearch(QWidget* window, std::function<QList<StatusCenterPane*>()> panes);
        ~SettingsSearch() override;

        void open(QWidget* from); // grows out of `from`
        void close();

    signals:
        // `target` is a widget in the setting's row, or null for the pane itself
        void chosen(StatusCenterPane* pane, QWidget* target);

    protected:
        void paintEvent(QPaintEvent* event) override;
        bool eventFilter(QObject* watched, QEvent* event) override;

    private:
        SettingsSearchPrivate* d;
        void search(const QString& query);
        void choose(int row);
        void follow();
};

#endif // SETTINGSSEARCH_H
