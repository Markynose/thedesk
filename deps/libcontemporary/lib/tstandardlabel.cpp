//
// Created by victor on 28/3/22.
//

#include "tstandardlabel.h"

#include <QPainter>
#include "libcontemporary_global.h"

struct tStandardLabelPrivate {
        tStandardLabel::StandardLabelType type = tStandardLabel::Normal;

        QString text;
        tStandardLabel::ElideMode elideMode = tStandardLabel::ElideRight;
        double maxSquashFactor = 1.3;
};

tStandardLabel::tStandardLabel(QWidget* parent) :
    QLabel(parent) {
    d = new tStandardLabelPrivate();
}

tStandardLabel::~tStandardLabel() {
    delete d;
}

tStandardLabel::StandardLabelType tStandardLabel::type() {
    return d->type;
}

void tStandardLabel::setType(tStandardLabel::StandardLabelType type) {
    d->type = type;

    QFont font = QApplication::font();
    switch (type) {
        case Subtitle:
            {
                if (libContemporaryCommon::isMaterial3()) {
                    // theDesk M3: TitleSmall in Primary, sentence case
                    font.setPixelSize(14);
                    font.setWeight(QFont::DemiBold);
                    QPalette pal = this->palette();
                    for (auto group : {QPalette::Active, QPalette::Inactive}) pal.setColor(group, QPalette::WindowText, pal.color(group, QPalette::Highlight));
                    this->setPalette(pal);
                    this->setContentsMargins(0, 8, 0, 2);
                } else {
                    font.setBold(true);
                    font.setCapitalization(QFont::AllUppercase);
                }
                break;
            }
        default:
            break;
    }

    this->setFont(font);
}

void tStandardLabel::setText(QString text) {
    d->text = text;
    this->updateText();
}

QString tStandardLabel::text() {
    return d->text;
}

void tStandardLabel::setElideMode(ElideMode elideMode) {
    d->elideMode = elideMode;
}

tStandardLabel::ElideMode tStandardLabel::elideMode() {
    return d->elideMode;
}

void tStandardLabel::setMaxSquashFactor(double maxSquashFactor) {
    d->maxSquashFactor = maxSquashFactor;
    this->update();
}

double tStandardLabel::maxSquashFactor() {
    return d->maxSquashFactor;
}

void tStandardLabel::updateText() {
    int availableWidth = this->width() * d->maxSquashFactor;
    auto text = d->text;
    switch (d->elideMode) {
        case ElideRight:
            text = this->fontMetrics().elidedText(d->text, Qt::ElideRight, availableWidth);
            break;
        case ElideCenter:
            text = this->fontMetrics().elidedText(d->text, Qt::ElideMiddle, availableWidth);
            break;
        case ElideLeft:
            text = this->fontMetrics().elidedText(d->text, Qt::ElideLeft, availableWidth);
            break;
        case NoElide:
            break;
    }
    QLabel::setText(text);
}

void tStandardLabel::resizeEvent(QResizeEvent* event) {
    this->updateText();
}

void tStandardLabel::paintEvent(QPaintEvent* event) {
    QPainter painter(this);
    QRectF rect({0, 0}, this->size());
    rect.adjust(this->margin(), this->margin(), -this->margin(), -this->margin());

    painter.setFont(this->font());
    auto metrics = painter.fontMetrics();

    QTextOption option;
    option.setTextDirection(this->layoutDirection());
    option.setAlignment(this->alignment());

    auto requiredWidth = metrics.horizontalAdvance(QLabel::text()) + 1;
    if (requiredWidth > rect.width()) {
        // We need to squish the text
        auto scaleFactor = static_cast<double>(rect.width()) / requiredWidth;
        painter.scale(scaleFactor, 1);
        rect.setWidth(requiredWidth);
    }

    painter.drawText(rect, QLabel::text(), option);
}
