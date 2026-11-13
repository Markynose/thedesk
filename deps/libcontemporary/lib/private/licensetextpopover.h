#ifndef LICENSETEXTPOPOVER_H
#define LICENSETEXTPOPOVER_H

#include <QWidget>

namespace Ui {
    class LicenseTextPopover;
}

class LicenseTextPopover : public QWidget {
        Q_OBJECT

    public:
        explicit LicenseTextPopover(QWidget* parent = nullptr);
        ~LicenseTextPopover();

    signals:
        void done();

    private slots:
        void on_titleLabel_backButtonClicked();

        void on_textBrowser_anchorClicked(const QUrl& arg1);

    private:
        Ui::LicenseTextPopover* ui;
};

#endif // LICENSETEXTPOPOVER_H
