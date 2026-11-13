#ifndef SELECTIONBUTTON_H
#define SELECTIONBUTTON_H

#include <QWidget>

// Pebble's face, for a moment, next to text the user just selected: a click
// opens Pebble with the selection and a few things to do with it
class PebbleSelectionButton : public QWidget {
        Q_OBJECT
    public:
        static void setUp();

    protected:
        void paintEvent(QPaintEvent* event) override;
        void mousePressEvent(QMouseEvent* event) override;
        void enterEvent(QEnterEvent* event) override;
        void leaveEvent(QEvent* event) override;

    private:
        PebbleSelectionButton();
        struct Private;
        Private* d;
        void selectionChanged();
        void appear();
};

#endif // SELECTIONBUTTON_H
