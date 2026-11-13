#ifndef SYSTEMLIBRARYDATABASE_H
#define SYSTEMLIBRARYDATABASE_H

#include <QObject>
#include <QStringList>
#include "enums.h"

struct SystemLibraryDatabasePrivate;
class SystemLibraryDatabase : public QObject {
        Q_OBJECT

    public:
        explicit SystemLibraryDatabase(QStringList extraSearchPaths, QString sdkVersion, TargetMachine arch, QStringList deepSearchPaths, QObject* parent = nullptr);
        ~SystemLibraryDatabase();

        QString library(QString libraryName);
        bool ignore(QString libraryName);

    private:
        SystemLibraryDatabasePrivate* d;

        bool isInDisallowedPath(QString path);
        bool isLibraryOk(QString libraryPath);
};

#endif
