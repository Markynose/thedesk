#ifndef TTOUCHBARACTIONITEM_H
#define TTOUCHBARACTIONITEM_H

#include "ttouchbarbuttonitem.h"
#include <QIcon>

class QAction;
struct tTouchBarActionItemPrivate;
class tTouchBarActionItem : public tTouchBarButtonItem {
        Q_OBJECT
    public:
        explicit tTouchBarActionItem(QString identifier, QAction* action, QString customizationLabel = "", QIcon icon = {}, QObject* parent = nullptr);
        ~tTouchBarActionItem();

        void setTouchBarIcon(QIcon touchBarIcon);
        QIcon touchBarIcon();

    signals:

    private:
        tTouchBarActionItemPrivate* d;

        void updateItem();
};

#endif // TTOUCHBARACTIONITEM_H
