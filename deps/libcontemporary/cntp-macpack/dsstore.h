#ifndef DSSTORE_H
#define DSSTORE_H

#include <QObject>

struct DsStorePrivate;
class DsStore : public QObject {
        Q_OBJECT
    public:
        explicit DsStore(QObject* parent = nullptr);
        ~DsStore();

        void write(QString file);

        void vSrn(int value);
        void moveIcon(QString file, int x, int y);
        void setWindowGeometry(QRect geometry);
        void setWindowProperties(int iconSize, QByteArray backgroundAlias);

    signals:

    private:
        DsStorePrivate* d;
};

#endif // DSSTORE_H
