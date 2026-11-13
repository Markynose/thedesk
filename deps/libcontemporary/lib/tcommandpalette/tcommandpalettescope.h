#ifndef TCOMMANDPALETTESCOPE_H
#define TCOMMANDPALETTESCOPE_H

#include "libcontemporary_global.h"
#include <QAbstractListModel>
#include <QObject>

class LIBCONTEMPORARY_EXPORT tCommandPaletteScope : public QAbstractListModel {
        Q_OBJECT
    public:
        explicit tCommandPaletteScope(QObject* parent = nullptr);

        virtual QString displayName() = 0;
        virtual void filter(QString filter) = 0;
        virtual void activate(QModelIndex index) = 0;

    signals:
};

#endif // TCOMMANDPALETTESCOPE_H
