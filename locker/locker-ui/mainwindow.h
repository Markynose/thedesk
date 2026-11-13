#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

struct MainWindowPrivate;

/**
 * The lock screen: libthedesk's M3LockSurface (the same screen as the
 * login greeter) with the lock's own behaviour: the password goes to the
 * PAM checker, the horizon sets when the machine sleeps and rises when it
 * wakes, notifications rise as moons.
 */
class MainWindow : public QMainWindow {
        Q_OBJECT

    public:
        explicit MainWindow(QWidget* parent = nullptr);
        ~MainWindow();

    private slots:

    private:
        MainWindowPrivate* d;

        bool checkPassword(QString password);
        void submit(const QString& password);

    protected:
        void changeEvent(QEvent* event) override;
        bool eventFilter(QObject* watched, QEvent* event) override;
        void showEvent(QShowEvent* event) override;
};

#endif // MAINWINDOW_H
