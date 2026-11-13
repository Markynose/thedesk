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
#include <QPainter>
#include <m3scheme.h>
#include <shapes/shapepath.h>
#include <shapes/materialshapes.h>
#include <QTime>
#include "onboardingtheme.h"
#include "ui_onboardingtheme.h"

#include <statemanager.h>
#include <onboardingmanager.h>
#include <tsettings.h>
#include <QGridLayout>
#include <QPainterPath>
#include <horizon/horizonview.h>
#include "settings/m3coloursettings.h"

struct OnboardingThemePrivate {
    tSettings* platformSettings;
};

namespace {
    // A small desktop in the session's colours (the wallpaper's shapes, the
    // bar, a window), repainted as the colours change: what the choice does
    class MiniDesktop : public QWidget {
        public:
            explicit MiniDesktop(QWidget* parent) :
                QWidget(parent) {
                this->setFixedSize(220, 150);
            }

        protected:
            bool event(QEvent* event) override {
                if (event->type() == QEvent::ApplicationPaletteChange || event->type() == QEvent::PaletteChange) this->update();
                return QWidget::event(event);
            }

            void paintEvent(QPaintEvent*) override {
                const M3::Scheme s = M3::currentScheme();
                QPainter p(this);
                p.setRenderHint(QPainter::Antialiasing);
                const QRectF r = this->rect();
                QPainterPath screen;
                screen.addRoundedRect(r, 24, 24);
                p.setClipPath(screen);
                p.fillRect(r, s.surfaceContainerLowest);
                auto shape = [&](const char* name, QRectF box, QColor colour) {
                    for (const auto& named : MaterialShapes::all()) {
                        if (qstrcmp(named.name, name) == 0) p.fillPath(m3shapes::toPath(named.shape(), box), colour);
                    }
                };
                shape("Cookie9Sided", QRectF(r.left() - 30, r.top() + 40, 110, 110), s.primaryContainer);
                shape("Sunny", QRectF(r.right() - 62, r.top() + 46, 52, 52), s.tertiaryContainer);
                QPainterPath ground;
                ground.moveTo(r.left(), r.bottom());
                ground.lineTo(r.left(), r.bottom() - 46);
                ground.quadTo(r.center().x(), r.bottom() - 92, r.right(), r.bottom() - 46);
                ground.lineTo(r.right(), r.bottom());
                p.fillPath(ground, s.surfaceContainer);
                auto rounded = [&](QRectF box, double radius, QColor colour) {
                    QPainterPath path;
                    path.addRoundedRect(box, radius, radius);
                    p.fillPath(path, colour);
                };
                // the bar
                rounded(QRectF(r.left() + 10, r.top() + 10, r.width() - 20, 24), 12, s.surfaceContainer);
                shape("Sunny", QRectF(r.left() + 16, r.top() + 14, 16, 16), s.primary);
                QFont clock(QStringLiteral("Roboto Flex"));
                clock.setPixelSize(11);
                clock.setVariableAxis(QFont::Tag("wght"), 750);
                p.setFont(clock);
                p.setPen(s.onSurface);
                p.drawText(QRectF(r.left() + 38, r.top() + 10, 80, 24), Qt::AlignVCenter, QLocale().toString(QTime::currentTime(), QLocale::ShortFormat));
                for (int i = 0; i < 3; i++) rounded(QRectF(r.right() - 30 - i * 16, r.top() + 17, 10, 10), 5, i ? s.tertiaryContainer : s.secondaryContainer);
                // a window
                const QRectF window(r.left() + 44, r.top() + 44, r.width() - 64, r.height() - 66);
                rounded(window, 14, s.surfaceContainerHigh);
                rounded(QRectF(window.left(), window.top(), window.width(), 24), 14, s.surfaceContainerHighest);
                for (int i = 0; i < 1; i++) {
                    rounded(QRectF(window.left() + 12, window.top() + 34 + i * 22, 16, 16), 5, i == 0 ? s.primaryContainer : s.secondaryContainer);
                    QColor line = s.onSurfaceVariant;
                    line.setAlphaF(0.5f);
                    rounded(QRectF(window.left() + 36, window.top() + 39 + i * 22, 50 + i * 20, 6), 3, line);
                }
                rounded(QRectF(window.right() - 70, window.bottom() - 32, 58, 22), 11, s.primary);
                rounded(QRectF(window.right() - 130, window.bottom() - 32, 52, 22), 11, s.secondaryContainer);
            }
    };
} // namespace

OnboardingTheme::OnboardingTheme(QWidget* parent) :
    OnboardingPage(parent),
    ui(new Ui::OnboardingTheme) {
    ui->setupUi(this);
    d = new OnboardingThemePrivate();
    d->platformSettings = new tSettings("theDesk.platform", this);

    ui->titleLabel->setBackButtonShown(true);

    connect(d->platformSettings, &tSettings::settingChanged, this, [ = ](QString key, QVariant value) {
        if (key == "Palette/base") {
            updateSettings();
        }
    });

    QPalette lightPal;
    lightPal.setColor(QPalette::Window, QColor(210, 210, 210));
    lightPal.setColor(QPalette::WindowText, Qt::black);
    ui->lightContainer->setPalette(lightPal);

    QPalette darkPal;
    darkPal.setColor(QPalette::Window, QColor(40, 40, 40));
    darkPal.setColor(QPalette::WindowText, Qt::white);
    ui->darkContainer->setPalette(darkPal);

    updateSettings();

    if (d->platformSettings->value("Palette/base").toString() == "m3") {
        // M3: in the manner of Pixel's "Wallpaper & style": the living horizon
        // in a card, recolouring live, over the same colour picker as Theme
        // settings (the old Dark/Light switch would turn M3 off)
        for (QWidget* w : {ui->darkContainer, ui->lightContainer, ui->widget, static_cast<QWidget*>(ui->line_2), static_cast<QWidget*>(ui->line)}) w->setVisible(false);

        auto* holder = new QWidget(this);
        auto* column = new QVBoxLayout(holder);
        column->setContentsMargins(0, 4, 0, 0);
        column->setSpacing(14);
        // The colours across the top; below, the other choices with a small
        // desktop wearing them beside
        auto* grid = new QGridLayout();
        grid->setHorizontalSpacing(16);
        grid->setVerticalSpacing(12);
        grid->setColumnStretch(1, 1);
        new M3ColourSettings(grid, 0, d->platformSettings, holder);
        if (QLayoutItem* colours = grid->itemAtPosition(0, 1)) {
            if (QWidget* swatches = colours->widget()) {
                grid->removeWidget(swatches);
                grid->addWidget(swatches, 0, 1, 1, 2);
            } else if (QLayout* swatches = colours->layout()) {
                grid->removeItem(colours);
                grid->addLayout(swatches, 0, 1, 1, 2);
            }
        }
        grid->addWidget(new MiniDesktop(holder), 1, 2, qMax(1, grid->rowCount() - 1), 1, Qt::AlignTop | Qt::AlignRight);
        column->addLayout(grid);
        auto* layout = qobject_cast<QBoxLayout*>(this->layout());
        const int at = layout->indexOf(ui->label) + 1;
        layout->insertWidget(at, holder);
        layout->insertStretch(at + 1);
    }
}

OnboardingTheme::~OnboardingTheme() {
    delete d;
    delete ui;
}

void OnboardingTheme::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange) {
        ui->retranslateUi(this);
    }
}

void OnboardingTheme::updateSettings() {
    QString baseColor = d->platformSettings->value("Palette/base").toString();
    if (baseColor == "dark") {
        ui->darkButton->setChecked(true);
        ui->lightButton->setChecked(false);
        ui->emojiLabel->setPixmap(QPixmap(":/thedesk/themeplugin/icons/moonman.svg"));
        ui->quote->setText(tr("Intellect has spoken; blinding shall be no more!"));
    } else if (baseColor == "light") {
        ui->darkButton->setChecked(false);
        ui->lightButton->setChecked(true);
        ui->emojiLabel->setPixmap(QPixmap(":/thedesk/themeplugin/icons/sunglassesman.svg"));
        ui->quote->setText(tr("One whose wisdom has brought them the light of day!"));
    }
}


QString OnboardingTheme::name() {
    return "OnboardingTheme";
}

QString OnboardingTheme::displayName() {
    return tr("Theme");
}

void OnboardingTheme::on_titleLabel_backButtonClicked() {
    StateManager::onboardingManager()->previousStep();
}

void OnboardingTheme::on_nextButton_clicked() {
    StateManager::onboardingManager()->nextStep();
}

void OnboardingTheme::on_lightButton_toggled(bool checked) {
    if (checked) {
        d->platformSettings->setValue("Palette/base", "light");
    } else {
        updateSettings();
    }
}

void OnboardingTheme::on_darkButton_toggled(bool checked) {
    if (checked) {
        d->platformSettings->setValue("Palette/base", "dark");
    } else {
        updateSettings();
    }
}
