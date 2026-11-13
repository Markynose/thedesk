#ifndef PLUGINMANAGEMENTPOPOVER_H
#define PLUGINMANAGEMENTPOPOVER_H

#include <QWidget>

namespace Ui {
    class PluginManagementPopover;
}

struct PluginManagementPopoverPrivate;
class PluginManagementPopover : public QWidget {
        Q_OBJECT

    public:
        explicit PluginManagementPopover(QWidget* parent = nullptr);
        ~PluginManagementPopover();

    signals:
        void done();

    private slots:
        void on_titleLabel_backButtonClicked();

        void on_listView_activated(const QModelIndex& index);

        void on_titleLabel_2_backButtonClicked();

        void on_activateButton_clicked();

        void on_deactivateButton_clicked();

        void on_restartButton_clicked();

    private:
        Ui::PluginManagementPopover* ui;
        PluginManagementPopoverPrivate* d;

        void updateCurrentPlugin();
};

#endif // PLUGINMANAGEMENTPOPOVER_H
