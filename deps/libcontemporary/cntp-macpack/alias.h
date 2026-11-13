#ifndef ALIAS_H
#define ALIAS_H

#include <QObject>

struct AliasPrivate;

class Alias : public QObject {
        Q_OBJECT
    public:
        explicit Alias(QObject* parent = nullptr);
        ~Alias();

        static QByteArray aliasFor(QString path);

        QByteArray data();

    signals:

    private:
        AliasPrivate* d;

        static QString volumeName(QString volumePath);
};

#endif // ALIAS_H
