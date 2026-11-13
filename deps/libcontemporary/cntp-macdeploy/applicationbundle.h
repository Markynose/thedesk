//
// Created by Victor Tran on 2/4/2022.
//

#ifndef LIBCONTEMPORARY_APPLICATIONBUNDLE_H
#define LIBCONTEMPORARY_APPLICATIONBUNDLE_H

#include <QObject>
#include <QDir>

struct ApplicationBundlePrivate;
class ApplicationBundle : public QObject {
    Q_OBJECT

public:
    explicit ApplicationBundle(QString path, QObject* parent = nullptr);
    ~ApplicationBundle();

    enum BundleDirectories {
        Resources,
        QtPlugins,
        ApplicationPlugins,
        Frameworks,
        QtStylePlugins,
        ExecutableBinary,
        IconResources
    };

    struct LibraryList {
        QStringList x86Libraries;
        QStringList armLibraries;
    };

    bool exists();

    QDir bundleDir();
    QDir bundleDir(BundleDirectories directory);

    void sign(QString identity);
    void makeSelfContained();

private:
    ApplicationBundlePrivate* d;

    bool doMakeSelfContained();
    QMultiMap<QString, QString> otool(QString filePath);
    QStringList rpaths(QString filePath);
    void copySystemPlugins(QStringList plugins);
    void installContemporaryIcons();
    void installQtConfigurationFile();
    void installNameTool(QString oldPath, QString newPath, QString filePath);
};


#endif //LIBCONTEMPORARY_APPLICATIONBUNDLE_H
