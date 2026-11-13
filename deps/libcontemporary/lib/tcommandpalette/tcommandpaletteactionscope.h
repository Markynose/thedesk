#ifndef TCOMMANDPALETTEACTIONSCOPE_H
#define TCOMMANDPALETTEACTIONSCOPE_H

#include "tcommandpalettescope.h"

class QAction;
class QMenu;
class QMenuBar;
struct tCommandPaletteActionScopePrivate;
class LIBCONTEMPORARY_EXPORT tCommandPaletteActionScope : public tCommandPaletteScope {
        Q_OBJECT
    public:
        explicit tCommandPaletteActionScope(QObject* parent = nullptr, QAction* exclusion = nullptr);
        ~tCommandPaletteActionScope();

        void addAction(QAction* action);
        void addMenu(QMenu* menu);
        void addMenuBar(QMenuBar* menuBar);

    signals:

    private:
        tCommandPaletteActionScopePrivate* d;

        // tCommandPaletteScope interface
    public:
        QString displayName();
        void filter(QString filter);
        void activate(QModelIndex index);

        // QAbstractItemModel interface
    public:
        int rowCount(const QModelIndex& parent) const;
        QVariant data(const QModelIndex& index, int role) const;

        // QAbstractItemModel interface
    public:
        Qt::ItemFlags flags(const QModelIndex &index) const;
};

#endif // TCOMMANDPALETTEACTIONSCOPE_H
