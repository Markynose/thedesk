#ifndef LIB$($UCPNAME)_GLOBAL_H
#define LIB$($UCPNAME)_GLOBAL_H

#include <QtCore/qglobal.h>

#if defined(LIB$($UCPNAME)_LIBRARY)
    #define LIB$($UCPNAME)_EXPORT Q_DECL_EXPORT
#else
    #define LIB$($UCPNAME)_EXPORT Q_DECL_IMPORT
#endif

#endif // LIB$($UCPNAME)_GLOBAL_H
