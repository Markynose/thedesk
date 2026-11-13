#ifndef LIBCONTEMPORARY_UNZIP_H
#define LIBCONTEMPORARY_UNZIP_H

#include <QBuffer>
#include <QDir>

void Unzip(const QString& zipFile, const QDir& destinationDir);

#endif