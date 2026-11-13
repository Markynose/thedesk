#ifndef TTOUCHBAR_FORWARDDECLARES_H
#define TTOUCHBAR_FORWARDDECLARES_H

#include <qglobal.h>

#if defined(Q_OS_MAC) && defined(__OBJC__)
    #import <AppKit/AppKit.h>
    #define HAVE_TOUCHBAR
    #define T_FORWARD_DECLARE_OBJC_INTERFACE(interface) @class interface
#else
    typedef void* NSTouchBar;
    typedef void* NSTouchBarItem;
    typedef void* NSView;
    #define T_FORWARD_DECLARE_OBJC_INTERFACE(interface) typedef void* interface
#endif

#if defined(Q_OS_MAC)
    #define HAVE_TOUCHBAR_IMPL
#endif

#endif // TTOUCHBAR_FORWARDDECLARES_H
