#ifndef SELECTIONBUTTON_H
#define SELECTIONBUTTON_H

#include <QWidget>

// Pebble's face, for a moment, next to text the user just selected. Pointed
// at, it opens into a pill of four things to do with it (M3E); the face
// itself opens Pebble with the selection
class PebbleSelectionButton : public QWidget {
        Q_OBJECT
    public:
        static void setUp();

    protected:
        void paintEvent(QPaintEvent* event) override;
        void mousePressEvent(QMouseEvent* event) override;
        void mouseMoveEvent(QMouseEvent* event) override;
        void enterEvent(QEnterEvent* event) override;
        void leaveEvent(QEvent* event) override;

    private:
        PebbleSelectionButton();
        struct Private;
        Private* d;
        void selectionChanged();
        void appear();
        void fit();
        QRectF actionRect(int i) const;
};

#endif // SELECTIONBUTTON_H
