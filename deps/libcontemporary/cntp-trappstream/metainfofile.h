#ifndef METAINFOFILE_H
#define METAINFOFILE_H

#include <QIODevice>
#include <QString>

struct MetainfoFilePrivate;
class MetainfoFile {
    public:
        MetainfoFile(QString file);
        ~MetainfoFile();

        void setOutput(QIODevice* device);

        bool moveToNext();

        QString trKey();
        QString trContents();

        void writeTranslation(QString language, QString contents);

        bool isError();

    private:
        MetainfoFilePrivate* d;
};

#endif // METAINFOFILE_H
