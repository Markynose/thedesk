//
// Created by victor on 28/3/22.
//

#ifndef LIBCONTEMPORARY_TSTANDARDLABEL_H
#define LIBCONTEMPORARY_TSTANDARDLABEL_H

#include "libcontemporary_global.h"
#include <QLabel>

struct tStandardLabelPrivate;
class LIBCONTEMPORARY_EXPORT tStandardLabel : public QLabel {
        Q_OBJECT

    public:
        explicit tStandardLabel(QWidget* parent = nullptr);
        ~tStandardLabel();

        enum StandardLabelType {
            Normal,
            Subtitle
        };

        enum ElideMode {
            NoElide,
            ElideRight,
            ElideCenter,
            ElideLeft
        };

        StandardLabelType type();
        void setType(StandardLabelType type);

        void setText(QString text);
        QString text();

        void setElideMode(ElideMode elideMode);
        ElideMode elideMode();

        void setMaxSquashFactor(double maxSquashFactor);
        double maxSquashFactor();

    private:
        tStandardLabelPrivate* d;

        void updateText();

        // QWidget interface
    protected:
        void resizeEvent(QResizeEvent* event);

        // QWidget interface
    protected:
        void paintEvent(QPaintEvent* event);
};

#endif // LIBCONTEMPORARY_TSTANDARDLABEL_H
