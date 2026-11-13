#ifndef ERRORFLASHWIDGET_H
#define ERRORFLASHWIDGET_H

#include <QWidget>

namespace Ui {
    class ErrorFlashWidget;
}

class ErrorFlashWidget : public QWidget {
        Q_OBJECT

    public:
        explicit ErrorFlashWidget(QWidget* parent = nullptr);
        ~ErrorFlashWidget();

        static void showError(QWidget* parent, QString message);

    private:
        Ui::ErrorFlashWidget* ui;
};

#endif // ERRORFLASHWIDGET_H
