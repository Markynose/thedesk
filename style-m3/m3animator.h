/****************************************
 *
 *   theDesk - Material 3 Expressive widget style
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 * *************************************/
#ifndef M3ANIMATOR_H
#define M3ANIMATOR_H

#include <QColor>
#include <QCursor>
#include <QElapsedTimer>
#include <QHash>
#include <QList>
#include <QObject>
#include <QPointF>
#include <QPointer>
#include <QTimer>
#include <functional>

class QPainter;
class QPainterPath;
class QWidget;
class M3SpringAnimation;

/**
 * Per-widget interaction state for the style, animated the Compose way:
 * hover/focus layers on the effects spring, press shape-morph and toggles on
 * the fast spatial spring, and Material ripples on press.
 */
class M3Animator : public QObject {
        Q_OBJECT
    public:
        explicit M3Animator(QObject* parent = nullptr);
        ~M3Animator() override;

        void watch(QWidget* widget);
        void unwatch(QWidget* widget);
        void watchBusy(QWidget* widget); // repaint every frame while visible

        // 0..1 animated values; widgets that aren't watched report `fallback`
        double hover(const QWidget* widget, bool fallback) const;
        double press(const QWidget* widget, bool fallback) const;
        double checked(const QWidget* widget, bool fallback) const;

        // Hover-merge: a fill that grows from where the pointer entered until it
        // covers `shape`, and fades out on leave. Returns the covered part of
        // `shape` (empty when idle) and its opacity.
        QPainterPath mergePath(const QWidget* widget, const QPainterPath& shape, double* opacity) const;

        // Draws this widget's ripples clipped to `shape`
        void paintRipples(QPainter* painter, const QWidget* widget, const QPainterPath& shape, QColor color) const;

        static qint64 now(); // ms since the style loaded; drives looping animations

        // Widgets the pointer melts into (labelled buttons); the style decides
        void setCursorMergePredicate(std::function<bool(const QWidget*)> predicate);

    protected:
        bool eventFilter(QObject* watched, QEvent* event) override;

    private:
        struct Ripple {
                QPointF origin;
                qint64 start;
                qint64 release = -1;
        };

        struct State {
                M3SpringAnimation* hover = nullptr;
                M3SpringAnimation* press = nullptr;
                M3SpringAnimation* checked = nullptr;
                M3SpringAnimation* merge = nullptr;
                QPointF entry;
                QList<Ripple> ripples;
        };

        QHash<const QWidget*, State> states;

        // Pointer merge: while over a labelled button the arrow shrinks into
        // its tip (the fill grows from there) and is hidden; it grows back out
        // on leave. Drawn as app-wide override cursors.
        std::function<bool(const QWidget*)> cursorMergeAllowed;
        QPointer<QWidget> cursorOwner;
        M3SpringAnimation* cursorScale = nullptr;
        bool cursorOverridden = false;
        QTimer cursorWatch;
        QHash<int, QCursor> cursorCache;
        void cursorEnter(QWidget* widget);
        void cursorLeave();
        void applyCursorScale(double scale);
        void checkCursorOwner();

        QList<QPointer<QWidget>> busy;
        QTimer frameTimer;

        M3SpringAnimation* spring(QWidget* widget, M3SpringAnimation*& slot, int token, double initial);
        void setTarget(QWidget* widget, M3SpringAnimation*& slot, int token, double target, double initial);
        void startMerge(QWidget* widget, State& state, QPointF entry);
        void startRipple(QWidget* widget, QPointF origin);
        void releaseRipples(QWidget* widget);
        void tick();
};

#endif // M3ANIMATOR_H
