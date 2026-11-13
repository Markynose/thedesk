#ifndef TPLUGINMANAGERPANE_H
#define TPLUGINMANAGERPANE_H

#include "libcontemporary_global.h"
#include "tsettingswindow/tsettingspane.h"

namespace Ui {
    class tPluginManagerPane;
}

class LIBCONTEMPORARY_EXPORT tPluginManagerPane : public tSettingsPane {
        Q_OBJECT

    public:
        explicit tPluginManagerPane(QWidget* parent = nullptr);
        ~tPluginManagerPane();

    private:
        Ui::tPluginManagerPane* ui;

        // tSettingsPane interface
    public:
        QString paneName();
        void updateRestartState();

    private slots:
        void on_manageButton_clicked();
        void on_restartButton_clicked();
};

#endif // TPLUGINMANAGERPANE_H
