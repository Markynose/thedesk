#ifndef EMOJIPANEL_H
#define EMOJIPANEL_H

#include <QDialog>

// Super+.: emoji and symbols drop from the bar; Enter or a click types the
// pick into the window that was focused
class EmojiPanel : public QDialog {
        Q_OBJECT
    public:
        static void setUp();
        static EmojiPanel* instance();

        void open() override;
        void close();

    protected:
        void paintEvent(QPaintEvent* event) override;
        void changeEvent(QEvent* event) override;
        void mouseMoveEvent(QMouseEvent* event) override;
        void mousePressEvent(QMouseEvent* event) override;
        void wheelEvent(QWheelEvent* event) override;
        bool eventFilter(QObject* watched, QEvent* event) override;

    private:
        explicit EmojiPanel();
        struct Private;
        Private* d;

        void relayout();
        void select(int index, bool spring = true);
        void type(int index);
        int cellAt(QPointF point) const;
        void scrollTo(double y);
};

#endif // EMOJIPANEL_H
