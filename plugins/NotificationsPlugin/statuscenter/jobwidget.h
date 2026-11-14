#ifndef JOBWIDGET_H
#define JOBWIDGET_H

#include <QWidget>
#include <SystemJob/systemjob.h>

namespace Ui {
    class JobWidget;
}

struct JobWidgetPrivate;
class JobWidget : public QWidget {
        Q_OBJECT

    public:
        explicit JobWidget(SystemJobPtr job, QWidget* parent = nullptr);
        ~JobWidget();

        // done, 0 to 1 (its app's shape wears it as a wavy ring)
        double progress() const;

    protected:
        void paintEvent(QPaintEvent* event) override;

    private:
        Ui::JobWidget* ui;
        JobWidgetPrivate* d;

        QCoro::Task<> loadData();
        void updateState(QString state);
};

#endif // JOBWIDGET_H
