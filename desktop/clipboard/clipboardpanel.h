#ifndef CLIPBOARDPANEL_H
#define CLIPBOARDPANEL_H

#include <QDialog>

// Super+V: the clipboard history drops from the bar. Enter or a click pastes
// into the window that was focused, Shift+Enter only copies
class ClipboardPanel : public QDialog {
        Q_OBJECT
    public:
        static void setUp();
        static ClipboardPanel* instance();

        void open() override;
        void close();

    protected:
        void paintEvent(QPaintEvent* event) override;
        void changeEvent(QEvent* event) override;
        void mouseMoveEvent(QMouseEvent* event) override;
        void mousePressEvent(QMouseEvent* event) override;
        void wheelEvent(QWheelEvent* event) override;
        void contextMenuEvent(QContextMenuEvent* event) override;
        bool eventFilter(QObject* watched, QEvent* event) override;

    private:
        explicit ClipboardPanel();
        struct Private;
        Private* d;

        void relayout();
        void select(int index, bool spring = true);
        void activate(int index, bool paste);
        int rowAt(QPointF point) const;
};

#endif // CLIPBOARDPANEL_H
