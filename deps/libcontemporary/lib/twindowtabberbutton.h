//
// Created by Victor Tran on 13/4/2022.
//

#ifndef LIBCONTEMPORARY_TWINDOWTABBERBUTTON_H
#define LIBCONTEMPORARY_TWINDOWTABBERBUTTON_H

#include "libcontemporary_global.h"
#include <QWidget>

struct tWindowTabberButtonPrivate;
class tStackedWidget;
class tWindowTabber;
class LIBCONTEMPORARY_EXPORT tWindowTabberButton : public QWidget {
        Q_OBJECT
    public:
        tWindowTabberButton(tWindowTabber* parent = nullptr);
        tWindowTabberButton(const QIcon& icon, const QString& text);
        tWindowTabberButton(const QIcon& icon, const QString& text, tStackedWidget* toTrack, QWidget* widgetToTrack);
        virtual ~tWindowTabberButton();

        void setText(const QString& text);
        void setSupplementaryText(const QString& supplementaryText);
        void setIcon(const QIcon& icon);

        bool isSelected();
        void setSelected(bool selected);

        QString text();
        QString supplementaryText();
        QIcon icon();

        void setParent(tWindowTabber* tabber);
        void syncWithStackedWidget(tStackedWidget* stackedWidget, QWidget* widget);

    signals:
        void activated();
        void changed();

    protected:
        friend tWindowTabber;
        void actionEvent(QActionEvent* event) override;
        void dragEnterEvent(QDragEnterEvent* event) override;

    private:
        tWindowTabberButtonPrivate* d;

        void init();
        void configureAction(QAction* action);
        void configureActionStrip();
};

#endif // LIBCONTEMPORARY_TWINDOWTABBERBUTTON_H
