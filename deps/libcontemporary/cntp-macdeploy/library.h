//
// Created by Victor Tran on 2/4/2022.
//

#ifndef LIBCONTEMPORARY_LIBRARY_H
#define LIBCONTEMPORARY_LIBRARY_H

#include <QObject>
#include "applicationbundle.h"

struct LibraryPrivate;
class Library : public QObject {
    Q_OBJECT

public:
    explicit Library(QString identifier, ApplicationBundle* bundle, QObject* parent = nullptr);
    ~Library();

    static QString extractIdentifierFromPath(QString path);

    void addLibraryPath(QString arch, QString path);
    QString libraryPath(QString arch);
    QString outputPath();

    static QStringList archs(QString path);
    static void lipo(QMap<QString, QString> toMerge, QString output);

private:
    LibraryPrivate* d;
};


#endif //LIBCONTEMPORARY_LIBRARY_H
