#ifndef M3FABMENU_H
#define M3FABMENU_H

#include "libthedesk_global.h"
#include <QWidget>
#include <functional>

// M3E FAB menu (FloatingActionButtonMenu): a FAB in its host's bottom corner
// that springs open into a stack of pill items, staggered. It opens into a
// round close button; a click outside or Esc closes it.
class LIBTHEDESK_EXPORT M3FabMenu : public QWidget {
        Q_OBJECT
    public:
        // `symbol`: a Material Symbols name, e.g. "add"
        explicit M3FabMenu(QWidget* host, const QString& symbol, const QString& label);
        ~M3FabMenu();

        void addItem(const QString& symbol, const QString& text, std::function<void()> act);
        void setOpen(bool open);

    protected:
        void paintEvent(QPaintEvent* event) override;
        void mousePressEvent(QMouseEvent* event) override;
        void mouseMoveEvent(QMouseEvent* event) override;
        void leaveEvent(QEvent* event) override;
        void keyPressEvent(QKeyEvent* event) override;
        bool eventFilter(QObject* watched, QEvent* event) override;

    private:
        struct Private;
        Private* d;

        void place();
        QRectF fabRect() const;
        QRectF itemRect(int i, double k) const;
};

#endif // M3FABMENU_H
