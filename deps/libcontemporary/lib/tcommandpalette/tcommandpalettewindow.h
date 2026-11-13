#ifndef TCOMMANDPALETTEWINDOW_H
#define TCOMMANDPALETTEWINDOW_H

#include <QDialog>

namespace Ui {
    class tCommandPaletteWindow;
}

class tCommandPaletteScope;
class tCommandPaletteController;
struct tCommandPaletteWindowPrivate;
class tCommandPaletteWindow : public QDialog {
        Q_OBJECT

    public:
        explicit tCommandPaletteWindow(tCommandPaletteController* controller, QWidget* parent = nullptr);
        ~tCommandPaletteWindow();

        void changeScope(tCommandPaletteScope* scope);
        tCommandPaletteScope* currentScope();

    signals:
        void scopeChanged(tCommandPaletteScope* scope);

    private:
        Ui::tCommandPaletteWindow* ui;
        tCommandPaletteWindowPrivate* d;

        tCommandPaletteController* controller();

        // QWidget interface
    protected:
        void changeEvent(QEvent* event);

        // QDialog interface
    public slots:
        void reject();
    private slots:
        void on_searchBox_textChanged(const QString& arg1);
        void on_listView_clicked(const QModelIndex& index);
        void on_searchBox_returnPressed();

        // QObject interface
    public:
        bool eventFilter(QObject* watched, QEvent* event);
};

#endif // TCOMMANDPALETTEWINDOW_H
