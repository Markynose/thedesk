#ifndef RICHTIP_H
#define RICHTIP_H

#include "libthedesk_global.h"
#include <QWidget>
#include <functional>

// M3 rich tooltip (RichTooltipTokens): a subhead, supporting text and an
// optional text-button action, under a bar chunk. One window, shared
class LIBTHEDESK_EXPORT RichTip : public QWidget {
        Q_OBJECT
    public:
        struct Content {
                QString title, text, action;
                std::function<void()> act;
        };

        // Hovering `owner` shows `content` under it after a moment; leaving hides it
        static void attach(QWidget* owner, std::function<Content()> content);

    protected:
        void paintEvent(QPaintEvent* event) override;
        void enterEvent(QEnterEvent* event) override;
        void leaveEvent(QEvent* event) override;
        void mouseMoveEvent(QMouseEvent* event) override;
        void mousePressEvent(QMouseEvent* event) override;

    private:
        RichTip();
        static RichTip* instance();
        struct Private;
        Private* d;

        void showFor(QWidget* owner, const Content& content);
        void hideSoon();
        void layOut();
};

#endif // RICHTIP_H
