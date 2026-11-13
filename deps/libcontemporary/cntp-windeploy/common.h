//
// Created by Victor Tran on 2/4/2022.
//

#ifndef LIBCONTEMPORARY_COMMON_H
#define LIBCONTEMPORARY_COMMON_H

#include <QStringList>
#include <QDir>

namespace Common {
    QStringList findInPaths(const QString& file, const QStringList& paths, bool subdirs);
    void copyDirRecursively(QDir dir, QDir destination);
}


#endif //LIBCONTEMPORARY_COMMON_H
