#ifndef DSSTOREENTRY_H
#define DSSTOREENTRY_H

#include <QByteArray>
#include <QString>

struct DsStoreEntryPrivate;
class DsStoreEntry {
    public:
        DsStoreEntry(const DsStoreEntry& other);
        DsStoreEntry(DsStoreEntry&& other);
        ~DsStoreEntry();

        DsStoreEntry& operator=(const DsStoreEntry& other);

        static DsStoreEntry entryFor(QString filename, QString structureId, QVariantMap options);

        quint64 length();
        QByteArray buffer();

    private:
        DsStoreEntry(QString filename, QString structureId, QString dataType, QByteArray blob);

        DsStoreEntryPrivate* d;
};

#endif // DSSTOREENTRY_H
