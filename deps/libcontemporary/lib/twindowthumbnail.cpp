#include "twindowthumbnail.h"

#ifdef Q_OS_WIN
    #include "private/twindowthumbnail-win.h"
#endif


tWindowThumbnail* tWindowThumbnail::thumbnailFor(QWidget *parent) {
#ifdef Q_OS_WIN
    return new tWindowThumbnailWin(parent);
#else
    return nullptr;
#endif
}