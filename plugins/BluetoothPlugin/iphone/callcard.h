/****************************************
 *
 *   theDesk - an incoming call on a linked iPhone
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 * *************************************/
#ifndef CALLCARD_H
#define CALLCARD_H

#include <QWidget>
#include <functional>

struct CallCardPrivate;

/**
 * A card that drops in under the bar while the phone rings: the caller's
 * initial in a Material shape that beats with each ring (wavy rings
 * spreading from it), their name, "<phone> · Incoming call", and a red
 * Decline and a green Answer pill. Answering shows "Answered on <phone>"
 * (the call itself stays on the phone) and the card goes back up.
 *
 * Needs a compositor (it's translucent); IPhoneLink falls back to a
 * notification without one.
 */
class CallCard : public QWidget {
        Q_OBJECT
    public:
        // answer(true) answers, answer(false) declines
        CallCard(QString caller, QString phone, std::function<void(bool)> answer);
        ~CallCard() override;

        static bool available();

        // The phone stopped ringing (answered there, or the caller hung up)
        void end();

    protected:
        void paintEvent(QPaintEvent* event) override;
        void mouseMoveEvent(QMouseEvent* event) override;
        void mousePressEvent(QMouseEvent* event) override;
        void mouseReleaseEvent(QMouseEvent* event) override;
        void leaveEvent(QEvent* event) override;

    private:
        CallCardPrivate* d;
        void tick();
        QRectF card() const;
        QRectF declineRect() const;
        QRectF answerRect() const;
        void choose(bool answer);
};

#endif // CALLCARD_H
