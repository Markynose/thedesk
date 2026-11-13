#ifndef COLOURPICKER_H
#define COLOURPICKER_H

#include <QWidget>

// Super+Shift+C: the pointer becomes a drop of magnified pixels that
// stretches as it moves; a click lets it fall into a swatch (the hex is
// copied) and the colour's tones fan out around it, each one copyable
class ColourPicker : public QWidget {
        Q_OBJECT
    public:
        static void setUp();
        static void pick();

    protected:
        void paintEvent(QPaintEvent* event) override;
        void mouseMoveEvent(QMouseEvent* event) override;
        void mousePressEvent(QMouseEvent* event) override;
        void wheelEvent(QWheelEvent* event) override;
        void keyPressEvent(QKeyEvent* event) override;

    private:
        ColourPicker();
        ~ColourPicker() override;
        struct Private;
        Private* d;

        void frame();
        void drop();
        void copy(QColor colour);
        void finish();
        QColor colourAt(QPoint point) const;
};

#endif // COLOURPICKER_H
