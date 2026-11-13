/****************************************
 *
 *   theDesk - Material 3 themed app icons
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 * *************************************/
#include "m3themedicon.h"

#include <QCache>
#include <QIconEngine>
#include <QPainter>
#include <QPainterPath>
#include <cmath>
#include <m3scheme.h>
#include <tsettings.h>

namespace {
    QColor mix(const QColor& a, const QColor& b, double t) {
        t = qBound(0.0, t, 1.0);
        return QColor::fromRgbF(a.redF() + (b.redF() - a.redF()) * t, a.greenF() + (b.greenF() - a.greenF()) * t, a.blueF() + (b.blueF() - a.blueF()) * t);
    }

    constexpr double inner = 0.66; // the icon's share of the circle

    class ThemedIconEngine : public QIconEngine {
        public:
            explicit ThemedIconEngine(QIcon source) :
                source(std::move(source)) {}

            void paint(QPainter* painter, const QRect& rect, QIcon::Mode mode, QIcon::State state) override {
                Q_UNUSED(mode)
                Q_UNUSED(state)
                qreal dpr = painter->device() ? painter->device()->devicePixelRatioF() : 1;
                painter->drawImage(rect, image(rect.size(), dpr));
            }
            QPixmap pixmap(const QSize& size, QIcon::Mode mode, QIcon::State state) override {
                return scaledPixmap(size, mode, state, 1);
            }
            QPixmap scaledPixmap(const QSize& size, QIcon::Mode mode, QIcon::State state, qreal scale) override {
                Q_UNUSED(mode)
                Q_UNUSED(state)
                QPixmap pixmap = QPixmap::fromImage(image(size, scale));
                pixmap.setDevicePixelRatio(scale);
                return pixmap;
            }
            QSize actualSize(const QSize& size, QIcon::Mode, QIcon::State) override {
                int side = qMin(size.width(), size.height());
                return QSize(side, side);
            }
            QString key() const override {
                return QStringLiteral("M3ThemedIcon");
            }
            QIconEngine* clone() const override {
                return new ThemedIconEngine(source);
            }
            bool isNull() override {
                return source.isNull();
            }

        private:
            QIcon source;

            QImage image(QSize size, qreal dpr) {
                QSize inside = (QSizeF(size) * inner * dpr).toSize();
                return M3ThemedIcon::render(source.pixmap(inside).toImage(), size, dpr);
            }
    };
} // namespace

bool M3ThemedIcon::enabled() {
    static const bool on = [] {
        tSettings settings(QStringLiteral("theSuite"), QStringLiteral("theDesk"));
        if (!settings.keysInGroup(QStringLiteral("Appearance")).contains(QStringLiteral("themedIcons"))) return true;
        return settings.value(QStringLiteral("Appearance/themedIcons")).toBool();
    }();
    return on;
}

QImage M3ThemedIcon::render(const QImage& source, QSize size, qreal dpr) {
    const M3::Scheme s = M3::currentScheme();
    // Cached per source, size and scheme (the horizon recolours it)
    static QCache<QString, QImage> cache(400);
    const QString key = QStringLiteral("%1-%2x%3-%4-%5").arg(source.cacheKey()).arg(size.width()).arg(size.height()).arg(dpr).arg(s.primaryContainer.rgb());
    if (QImage* hit = cache.object(key)) return *hit;

    QImage out((QSizeF(size) * dpr).toSize(), QImage::Format_ARGB32_Premultiplied);
    out.setDevicePixelRatio(dpr);
    out.fill(Qt::transparent);
    QPainter p(&out);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    const QRectF r(0, 0, size.width(), size.height());
    const double side = qMin(r.width(), r.height());
    QPainterPath circle;
    circle.addEllipse(r.center(), side / 2, side / 2);
    p.fillPath(circle, s.primaryContainer);

    // Duotone: dark parts sink toward the container, bright ones rise to
    // OnPrimaryContainer; the icon keeps its own alpha
    QImage duo = source.convertToFormat(QImage::Format_ARGB32);
    const QColor low = mix(s.primaryContainer, s.onPrimaryContainer, 0.14);
    const QColor high = s.onPrimaryContainer;
    for (int y = 0; y < duo.height(); y++) {
        QRgb* line = reinterpret_cast<QRgb*>(duo.scanLine(y));
        for (int x = 0; x < duo.width(); x++) {
            int a = qAlpha(line[x]);
            if (a == 0) continue;
            QColor c = mix(low, high, std::pow(qGray(line[x]) / 255.0, 0.75) * 1.3);
            line[x] = qRgba(c.red(), c.green(), c.blue(), a);
        }
    }
    const double inside = side * inner;
    p.drawImage(QRectF(r.center().x() - inside / 2, r.center().y() - inside / 2, inside, inside), duo);
    p.end();
    cache.insert(key, new QImage(out));
    return out;
}

QImage M3ThemedIcon::glyph(const QImage& source, const QColor& container, const QColor& content) {
    QImage duo = source.convertToFormat(QImage::Format_ARGB32);
    const QColor low = mix(container, content, 0.14);
    for (int y = 0; y < duo.height(); y++) {
        QRgb* line = reinterpret_cast<QRgb*>(duo.scanLine(y));
        for (int x = 0; x < duo.width(); x++) {
            const int a = qAlpha(line[x]);
            if (a == 0) continue;
            const QColor c = mix(low, content, std::pow(qGray(line[x]) / 255.0, 0.75) * 1.3);
            line[x] = qRgba(c.red(), c.green(), c.blue(), a);
        }
    }
    return duo;
}

QIcon M3ThemedIcon::icon(const QIcon& source) {
    if (!enabled() || source.isNull()) return source;
    return QIcon(new ThemedIconEngine(source));
}
