#ifndef KNOWNPLUGINMODEL_H
#define KNOWNPLUGINMODEL_H

#include <QAbstractListModel>

class KnownPluginModel : public QAbstractListModel {
        Q_OBJECT

    public:
        explicit KnownPluginModel(QObject* parent = nullptr);

        enum Roles {
            UuidRole = Qt::UserRole,
            LoadedRole,
            BlacklistedRole
        };

        int rowCount(const QModelIndex& parent = QModelIndex()) const override;
        QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

    private:
};

#endif // KNOWNPLUGINMODEL_H
