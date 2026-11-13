#ifndef DISKIMAGE_H
#define DISKIMAGE_H

#include <QDir>
#include <QObject>

struct DiskImagePrivate;
class DiskImage : public QObject {
        Q_OBJECT
    public:
        explicit DiskImage(qulonglong size, QString name, QString output, QString filesystem = "HFS+", QObject* parent = nullptr);
        explicit DiskImage(QString name);
        ~DiskImage();

        static bool convert(QString input, QString output, QString format = "UDZO");

        bool isValid();

        bool mount();
        void unmount();
        QDir mountPath();

    signals:

    private:
        DiskImagePrivate* d;
};

#endif // DISKIMAGE_H
