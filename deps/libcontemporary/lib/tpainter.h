#ifndef TPAINTER_H
#define TPAINTER_H

#include "libcontemporary_global.h"
#include <QPainter>

struct tPainterPrivate;
class LIBCONTEMPORARY_EXPORT tPainter {
    public:
        tPainter(QPainter& painter);
        tPainter(QPainter* painter);
        ~tPainter();

        void drawSquashedText(const QRectF& rect, const int& flags, const QString& text) const;

    private:
        tPainterPrivate* d;
};

#endif // TPAINTER_H
