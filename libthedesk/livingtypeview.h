/****************************************
 *
 *   theDesk - living type loading screen
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 * *************************************/
#ifndef LIVINGTYPEVIEW_H
#define LIVINGTYPEVIEW_H

#include <QDate>
#include <QWidget>
#include "libthedesk_global.h"

struct LivingTypeViewPrivate;

/**
 * The loading screen: words in Roboto Flex that keep changing their whole
 * look through the variable axes (weight, width), squash thin "|" or flat
 * "_", and turn on their side or upside down, around the M3 Expressive
 * loading indicator. All motion is M3 Expressive springs.
 *
 *   Collage: a few big words around the indicator
 *   Field:   the whole screen tiled with low-contrast text; a wave moves
 *            through it and it makes room around the indicator
 */
class LIBTHEDESK_EXPORT LivingTypeView : public QWidget {
        Q_OBJECT
    public:
        enum Mode {
            Collage,
            Field
        };

        enum ExitStyle {
            RandomExit = -1,
            Vortex = 0, // words spiral into the indicator, which opens onto the desktop
            Bloom,      // the indicator grows and punches through to the desktop
            Ripple,     // a ring spreads from the indicator and flattens the words
            Drop,       // letters fall away
            Sweep,      // words squeeze thin and whoosh off to the side
            Cascade,    // rows collapse top to bottom
            ExitStyleCount
        };

        explicit LivingTypeView(QWidget* parent = nullptr);
        ~LivingTypeView() override;

        void setMode(Mode mode);
        Mode mode() const;
        void setTimeScale(double scale); // 1 = normal, 0.25 = slow motion
        void restart();
        void playExit();
        void setExitStyle(ExitStyle style); // the field's exit; RandomExit picks one each time
        ExitStyle exitStyle() const;
        static QString exitStyleName(int style);

        // The field's rows in the MLM (gay men's) pride flag's stripes
        void setFlagColours(bool flag);

        // After a crash (startdesk restarting theDesk): its own phrases,
        // calmer colours and motion
        void setRecovery(bool recovery);
        static QStringList recoveryPhrases();

        // The goodbye (shutting down, restarting, logging out): the same
        // loading screen with its own phrases (empty: the loading ones again)
        void setFarewell(QStringList phrases);

        // Preview (inside another window, e.g. the Theme settings): the exit
        // clips this view's own painting instead of masking the window
        void setPreview(bool preview);

        // The phrases for a day: the four always, plus seasonal ones
        // (November/Movember: men's mental health; May and 10 October:
        // mental health)
        static QStringList phrasesFor(QDate date);

        static QString userDisplayName();

    signals:
        void exitFinished();

    protected:
        void paintEvent(QPaintEvent* event) override;
        void resizeEvent(QResizeEvent* event) override;
        void showEvent(QShowEvent* event) override;
        void hideEvent(QHideEvent* event) override;

    private:
        LivingTypeViewPrivate* d;
        void tick();
        void build();
};

#endif // LIVINGTYPEVIEW_H
