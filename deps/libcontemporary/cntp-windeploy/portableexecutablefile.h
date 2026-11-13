#ifndef PORTABLEEXECUTABLEFILE_H
#define PORTABLEEXECUTABLEFILE_H

#include <QObject>
#include "enums.h"

struct PortableExecutableFilePrivate;
class PortableExecutableFile : public QObject {
        Q_OBJECT
    public:
        explicit PortableExecutableFile(QString fileName, QObject* parent = nullptr);
        ~PortableExecutableFile();

        bool isValid();
        TargetMachine targetMachine();

    signals:

    private:
        PortableExecutableFilePrivate* d;
        void loadPe(QString fileName);
};

#endif // PORTABLEEXECUTABLEFILE_H
