/****************************************
 *
 *   theDesk - Material 3 Expressive widget style
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   Sizes, shapes, colours and state opacities are Jetpack Compose's
 *   Material 3 tokens (compose/material3/.../tokens/*.kt). Sizes are Qt
 *   logical pixels (= dp); Qt applies the device pixel ratio.
 *
 * *************************************/
#ifndef M3STYLE_H
#define M3STYLE_H

#include <QHash>
#include <QProxyStyle>
#include <m3scheme.h>

class M3Animator;

class M3Style : public QProxyStyle {
        Q_OBJECT
    public:
        // Shared with libcontemporary (tSwitch, tCircularSpinner)
        static constexpr auto SH_M3Style = QStyle::StyleHint(QStyle::SH_CustomBase + 0x4D33);
        static constexpr auto PE_M3Switch = QStyle::PrimitiveElement(QStyle::PE_CustomBase + 0x4D33);
        static constexpr auto PE_M3LoadingIndicator = QStyle::PrimitiveElement(QStyle::PE_CustomBase + 0x4D34);

        M3Style();
        ~M3Style() override;

        void polish(QWidget* widget) override;
        bool eventFilter(QObject* watched, QEvent* event) override;
        void unpolish(QWidget* widget) override;
        void polish(QPalette& palette) override;
        using QProxyStyle::polish;
        using QProxyStyle::unpolish;

        void drawPrimitive(PrimitiveElement element, const QStyleOption* option, QPainter* painter, const QWidget* widget = nullptr) const override;
        void drawControl(ControlElement element, const QStyleOption* option, QPainter* painter, const QWidget* widget = nullptr) const override;
        void drawComplexControl(ComplexControl control, const QStyleOptionComplex* option, QPainter* painter, const QWidget* widget = nullptr) const override;

        int pixelMetric(PixelMetric metric, const QStyleOption* option = nullptr, const QWidget* widget = nullptr) const override;
        int styleHint(StyleHint hint, const QStyleOption* option = nullptr, const QWidget* widget = nullptr, QStyleHintReturn* returnData = nullptr) const override;
        QSize sizeFromContents(ContentsType type, const QStyleOption* option, const QSize& size, const QWidget* widget = nullptr) const override;
        QRect subElementRect(SubElement element, const QStyleOption* option, const QWidget* widget = nullptr) const override;
        QRect subControlRect(ComplexControl control, const QStyleOptionComplex* option, SubControl subControl, const QWidget* widget = nullptr) const override;

    private:
        void applyHeader(class QLabel* label);
        M3Animator* animator;
        mutable QHash<quint64, M3::Scheme> schemes;

        // The M3 scheme behind a palette: the platform's own scheme when the
        // palette came from it, otherwise one seeded from its highlight
        const M3::Scheme& scheme(const QPalette& palette) const;

        void drawButton(const QStyleOption* option, QPainter* painter, const QWidget* widget) const;
        void drawToolButton(const QStyleOptionComplex* option, QPainter* painter, const QWidget* widget) const;
        void drawSwitch(const QStyleOption* option, QPainter* painter, const QWidget* widget) const;
        void drawLoadingIndicator(const QStyleOption* option, QPainter* painter, const QWidget* widget) const;
        void drawCheckBox(const QStyleOption* option, QPainter* painter, const QWidget* widget, bool stateLayer) const;
        void drawRadio(const QStyleOption* option, QPainter* painter, const QWidget* widget) const;
        void drawField(const QRectF& rect, const QStyleOption* option, QPainter* painter, const QWidget* widget, bool focused) const;
        void drawSlider(const QStyleOptionComplex* option, QPainter* painter, const QWidget* widget) const;
        void drawScrollBar(const QStyleOptionComplex* option, QPainter* painter, const QWidget* widget) const;
        void drawProgress(const QStyleOption* option, QPainter* painter, const QWidget* widget) const;
        void drawMenuItem(const QStyleOption* option, QPainter* painter, const QWidget* widget) const;
        void drawChevron(QPainter* painter, const QRectF& rect, Qt::ArrowType direction, QColor color, double size = 0) const;
        void drawFocusRing(QPainter* painter, const QPainterPath& shape, const M3::Scheme& s) const;
};

#endif // M3STYLE_H
