#ifndef TEXCEPTION_H
#define TEXCEPTION_H

#include "libcontemporary_global.h"
#include <QException>

#define T_EXCEPTION(cls)          \
public:                           \
    cls();                        \
    cls(QString reason);          \
                                  \
    void raise() const;           \
    cls* clone() const;           \
    QString reason() const;       \
    virtual QString type() const; \
                                  \
private:                          \
    QString _reason;

#define T_EXCEPTION_IMPL(cls)        \
    cls::cls() {                     \
        _reason = "";                \
    }                                \
                                     \
    cls::cls(QString reason) {       \
        _reason = reason;            \
    }                                \
                                     \
    void cls::raise() const {        \
        throw *this;                 \
    }                                \
                                     \
    cls* cls::clone() const {        \
        return new cls(*this);       \
    }                                \
                                     \
    QString cls::type() const {      \
        return QStringLiteral(#cls); \
    }                                \
                                     \
    QString cls::reason() const {    \
        return _reason;              \
    }

class LIBCONTEMPORARY_EXPORT tException : public QException {
        T_EXCEPTION(tException)
};
class LIBCONTEMPORARY_EXPORT tDBusException : public tException {
        T_EXCEPTION(tDBusException)
};

#endif // TEXCEPTION_H
