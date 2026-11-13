#include "unzip.h"

#include <QFile>
#include <QMap>
#include <QProcess>
#include <QDirIterator>

void Unzip(const QString &zipFilePath, const QDir &destinationDir) {
    QProcess unzipProc;
    unzipProc.start("pwsh", {"-NonInteractive", "-Command", QStringLiteral("Expand-Archive -Path \"%1\" -Destination \"%2\"").arg(zipFilePath).arg(destinationDir.absolutePath())});
    unzipProc.waitForFinished();

    QDirIterator iterator(destinationDir.absolutePath(), QDir::Files, QDirIterator::Subdirectories);
    QMap<QString, QString> filesToReplace;
    while (iterator.hasNext()) {
        iterator.next();

        QString currentPath = iterator.filePath();

        while (true) {
            QFile file(currentPath);
            file.open(QFile::ReadOnly);
            QString fileContents = file.readAll();
            file.close();
            
            auto absolutePathFromSymlink = iterator.fileInfo().dir().absoluteFilePath(fileContents);
            if (!QFile::exists(absolutePathFromSymlink)) {
                break;
            }

            currentPath = absolutePathFromSymlink;
        } 
        
        if (currentPath != iterator.filePath()) filesToReplace.insert(iterator.filePath(), currentPath);
    }

    for (auto it = filesToReplace.constKeyValueBegin(); it != filesToReplace.constKeyValueEnd(); it++) {
        QFile::remove(it->first);
        QFile::copy(it->second, it->first);
    }
}
