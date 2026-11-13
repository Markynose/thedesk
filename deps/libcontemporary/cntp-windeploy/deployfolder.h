//
// Created by victo on 11/04/2022.
//

#ifndef LIBCONTEMPORARY_DEPLOYFOLDER_H
#define LIBCONTEMPORARY_DEPLOYFOLDER_H

#include <QObject>
#include <QDir>

class SystemLibraryDatabase;
struct DeployFolderPrivate;
class DeployFolder : public QObject {
    Q_OBJECT

public:
    explicit DeployFolder(QString qtPath, QString hostQtPath, QString folder, QObject* parent = nullptr);
    ~DeployFolder();

    enum class DeployFolderDirectories {
        QtPlugins,
        ApplicationPlugins,
        QtStylePlugins,
        IconResources,
        Libraries
    };

    QDir destinationDir();
    QDir destinationDir(DeployFolderDirectories directory);

    void makeSelfContained(SystemLibraryDatabase *libraryDatabase);

private:
    DeployFolderPrivate* d;

    void copySystemPlugins(QStringList plugins);
    void installContemporaryIcons();
    void installQtConfigurationFile();
    bool doMakeSelfContained(SystemLibraryDatabase *libraryDatabase);

    QStringList dumpBin(QString file);
};


#endif//LIBCONTEMPORARY_DEPLOYFOLDER_H
