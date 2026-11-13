#include "tpainter.h"

struct tPainterPrivate {
        QPainter* painter;
};

tPainter::tPainter(QPainter& painter) {
    d = new tPainterPrivate();
    d->painter = &painter;
}

tPainter::tPainter(QPainter* painter) {
    d = new tPainterPrivate();
    d->painter = painter;
}

tPainter::~tPainter() {
    delete d;
}

void tPainter::drawSquashedText(const QRectF& rect, const int& flags, const QString& text) const {
    auto textWidth = d->painter->fontMetrics().horizontalAdvance(text);
    if (textWidth > rect.width()) {
        // We need to squish the text
        d->painter->save();

        auto scaleFactor = rect.width() / textWidth;
        d->painter->scale(scaleFactor, 1);

        auto wideRect = rect;
        wideRect.setWidth(rect.width() / scaleFactor);
        d->painter->drawText(wideRect, Qt::AlignLeft | Qt::AlignVCenter, text);
        d->painter->restore();
    } else {
        d->painter->drawText(rect, flags, text);
    }
}
