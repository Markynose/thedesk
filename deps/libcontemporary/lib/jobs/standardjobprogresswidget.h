#ifndef STANDARDJOBPROGRESSWIDGET_H
#define STANDARDJOBPROGRESSWIDGET_H

#include <QWidget>

namespace Ui {
    class StandardJobProgressWidget;
}

class tStandardJob;
struct StandardJobProgressWidgetPrivate;
class StandardJobProgressWidget : public QWidget {
        Q_OBJECT

    public:
        explicit StandardJobProgressWidget(tStandardJob* job, QWidget* parent = nullptr);
        ~StandardJobProgressWidget();

    private:
        Ui::StandardJobProgressWidget* ui;
        StandardJobProgressWidgetPrivate* d;

        void updateState();
};

#endif // STANDARDJOBPROGRESSWIDGET_H
