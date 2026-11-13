/****************************************
 *
 *   INSERT-PROJECT-NAME-HERE - INSERT-GENERIC-NAME-HERE
 *   Copyright (C) 2020 Victor Tran
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * *************************************/
#include "onboardingstepper.h"
#include <QPainterPath>
#include <m3scheme.h>
#include "onboardingchrome.h"
#include <shapes/materialshapes.h>
#include <shapes/shapepath.h>

#include <QPainter>
#include <libcontemporary_global.h>
#include <tpaintcalculator.h>

struct OnboardingStepperPrivate {
        QString text;
        QString pageName;
        int step = 1;
        int currentStep = 1;
        bool isFinal = false;
};

OnboardingStepper::OnboardingStepper(QWidget* parent) :
    QWidget(parent) {
    d = new OnboardingStepperPrivate();

    QFont fnt(QStringLiteral("Roboto Flex"));
    fnt.setPixelSize(15);
    this->setFont(fnt);
}

OnboardingStepper::~OnboardingStepper() {
    delete d;
}

void OnboardingStepper::setText(QString text) {
    d->text = text;
    this->update();
}

void OnboardingStepper::setPageName(QString pageName) {
    d->pageName = pageName;
    this->update();
}

void OnboardingStepper::setStep(int step) {
    d->step = step;
    this->update();
}

void OnboardingStepper::setCurrentStep(int step) {
    d->currentStep = step;
    this->update();
}

void OnboardingStepper::setIsFinal(bool isFinal) {
    d->isFinal = isFinal;
    this->update();
}

QSize OnboardingStepper::sizeHint() const {
    return QSize(300, SC_DPI(48)); // room for each step's shape
}

void OnboardingStepper::paintEvent(QPaintEvent* event) {
    QPainter* painter = new QPainter(this);
    painter->setRenderHint(QPainter::Antialiasing);

    tPaintCalculator calculator;
    calculator.setPainter(painter);
    calculator.setDrawBounds(this->size());
    calculator.setLayoutDirection(this->layoutDirection());


    int radius = this->height() - SC_DPI(16);
    QRect circleRect;
    circleRect.setSize(QSize(radius, radius));
    circleRect.moveLeft(SC_DPI(9));
    circleRect.moveTop(this->height() / 2 - radius / 2);

    calculator.addRect(circleRect, [=](QRectF drawBounds) {
        // Each step wears its own shape and symbol (as its hero does): the
        // current one in Primary, finished ones a Primary circle with a tick,
        // the ones ahead quiet in SurfaceContainerHighest
        const M3::Scheme s = M3::currentScheme();
        const bool current = d->currentStep == d->step;
        const bool done = d->currentStep > d->step;
        const OnboardingChrome::StepLook look = OnboardingChrome::stepLook(d->pageName);
        const double size = drawBounds.width() * (current ? 1.0 : 0.86);
        const QRectF box(drawBounds.center().x() - size / 2, drawBounds.center().y() - size / 2, size, size);
        const char* shapeName = done ? "Circle" : look.shape;
        const m3shapes::RoundedPolygon* polygon = &MaterialShapes::circle();
        for (const auto& named : MaterialShapes::all()) {
            if (qstrcmp(named.name, shapeName) == 0) polygon = &named.shape();
        }
        painter->setPen(Qt::NoPen);
        painter->fillPath(m3shapes::toPath(*polygon, box), current || done ? s.primary : s.surfaceContainerHighest);
        QFont symbol(QStringLiteral("Material Symbols Rounded"));
        const double px = size * 0.56;
        symbol.setPixelSize(qMax(1, qRound(px)));
        symbol.setVariableAxis(QFont::Tag("FILL"), 1);
        symbol.setVariableAxis(QFont::Tag("wght"), 500);
        symbol.setVariableAxis(QFont::Tag("opsz"), 20);
        painter->setFont(symbol);
        painter->setPen(current || done ? s.onPrimary : s.onSurfaceVariant);
        painter->drawText(box, Qt::AlignCenter, QString::fromLatin1(done ? "check" : look.symbol));
        painter->setFont(this->font());
    });

    QRect textRect;
    textRect.setWidth(this->fontMetrics().horizontalAdvance(d->text));
    textRect.setHeight(this->fontMetrics().height());
    textRect.moveLeft(circleRect.right() + SC_DPI(9));
    textRect.moveTop(this->height() / 2 - textRect.height() / 2);

    calculator.addRect(textRect, [=](QRectF drawBounds) {
        const M3::Scheme s = M3::currentScheme();
        const bool current = d->currentStep == d->step;
        QFont label = this->font();
        label.setFamily(QStringLiteral("Roboto Flex"));
        label.setVariableAxis(QFont::Tag("wght"), current ? 750 : 450);
        label.setVariableAxis(QFont::Tag("wdth"), current ? 112 : 100);
        painter->setFont(label);
        painter->setPen(current ? s.onSurface : d->currentStep > d->step ? s.onSurface : s.onSurfaceVariant);
        painter->drawText(drawBounds.adjusted(0, 0, 40, 0), Qt::AlignLeft, d->text);
        painter->setPen(QPen(s.outlineVariant, 1.5)); // the line between the steps
    });

    if (d->step != 1) {
        calculator.addRect(circleRect, [=](QRectF drawBounds) {
            painter->drawLine(drawBounds.center().x(), 0, drawBounds.center().x(), drawBounds.top());
        });
    }
    if (!d->isFinal) {
        calculator.addRect(circleRect, [=](QRectF drawBounds) {
            painter->drawLine(drawBounds.center().x(), drawBounds.bottom(), drawBounds.center().x(), this->height());
        });
    }

    calculator.performPaint();

    delete painter;
}
