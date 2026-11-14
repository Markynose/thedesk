#ifndef CIRCLESEARCH_H
#define CIRCLESEARCH_H

#include <QDialog>

// Circle to search (Super+Shift+Space): the screen freezes with a shimmer in
// Pebble's colours; circle, scribble or tap on anything and Pebble answers
// about it in a sheet that rises from the pill at the bottom. The selection's
// corners can be dragged to ask about a different part.
class CircleSearch : public QDialog { // a dialog: the window manager floats it, like Pebble's own
        Q_OBJECT
    public:
        static CircleSearch* instance();
        static void setUp();

        void start();
        void finish();

    protected:
        void paintEvent(QPaintEvent* event) override;
        void mousePressEvent(QMouseEvent* event) override;
        void mouseMoveEvent(QMouseEvent* event) override;
        void mouseReleaseEvent(QMouseEvent* event) override;
        void keyPressEvent(QKeyEvent* event) override;
        bool eventFilter(QObject* watched, QEvent* event) override;

    private:
        CircleSearch();
        struct Private;
        Private* d;

        void ask(const QString& question);
        QRectF pillRect() const;
};

#endif // CIRCLESEARCH_H
