/****************************************
 *
 *   theDesk - the Gateway: a launcher in the middle of the screen
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 * *************************************/
#ifndef LAUNCHER_H
#define LAUNCHER_H

#include <QWidget>
#include <functional>

/**
 * The Gateway's contents: a search bar and, under it, one panel.
 *
 * At rest the panel holds the pinned apps (each in a MaterialShape), the
 * files opened lately, and a toolbar (lock, sleep, restart, Settings, Status
 * Center, power). "All apps" turns it into a grid with category chips.
 * Typing turns it into results: a top hit with its actions as buttons, then
 * apps, settings, power commands and files in groups, and a web search.
 * Run mode (Alt+F2, or ">" typed) takes a command instead: the completed
 * command is the top hit, the history and PATH come under it.
 *
 * Everything but the text field is painted here, so shapes, state layers
 * and springs are its own; the keyboard reaches all of it (arrows, Enter,
 * Tab through a result's actions, Esc).
 */
class Launcher : public QWidget {
        Q_OBJECT
    public:
        explicit Launcher(QWidget* parent = nullptr);
        ~Launcher() override;

        void opened(bool run = false); // back to rest (or Run mode), focused, and its entrance played
        QRegion shape() const; // the bar and the panel: what's solid
        static int columnWidth(int available);

    signals:
        void closeRequested();
        void shapeChanged();

    protected:
        void paintEvent(QPaintEvent* event) override;
        void resizeEvent(QResizeEvent* event) override;
        void mouseMoveEvent(QMouseEvent* event) override;
        void mousePressEvent(QMouseEvent* event) override;
        void mouseReleaseEvent(QMouseEvent* event) override;
        void leaveEvent(QEvent* event) override;
        void wheelEvent(QWheelEvent* event) override;
        void contextMenuEvent(QContextMenuEvent* event) override;
        bool event(QEvent* event) override;
        bool eventFilter(QObject* watched, QEvent* event) override;

    private:
        struct Private;
        Private* d;

        void relayout();
        void layoutHome(double& y);
        void layoutAllApps(double& y);
        void layoutSearch(double& y);
        void layoutToolbar(double& y);
        void layoutRun(double& y);
        void enterRun();
        void leaveRun();
        void runCommand(const QString& command, bool inTerminal);
        void frame();
        int itemAt(const QPointF& point, int* action = nullptr) const;
        void activate(int item, int action = 0);
        void moveSelection(int dx, int dy);
        void select(int item);
        void thenClose(const std::function<void()>& run);
};

#endif // LAUNCHER_H
