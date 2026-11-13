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
#include "onboardingregion.h"
#include "ui_onboardingregion.h"

#include <QLocale>
#include <QTimeZone>
#include <QTimer>
#include <localemanager.h>
#include <onboardingmanager.h>
#include <statemanager.h>

#include "localeplugincommon.h"
#include <QBoxLayout>
#include <QDateTime>
#include <QElapsedTimer>
#include <QPainter>
#include <QPainterPath>
#include <cmath>
#include <m3scheme.h>

namespace {
    QFont flex(double px, double wght, double wdth = 100) {
        QFont font(QStringLiteral("Roboto Flex"));
        font.setPixelSize(qRound(px));
        font.setVariableAxis(QFont::Tag("wght"), float(wght));
        font.setVariableAxis(QFont::Tag("wdth"), float(wdth));
        font.setVariableAxis(QFont::Tag("opsz"), float(qBound(8.0, px, 144.0)));
        return font;
    }

    // "How it will look": the clock, the date and a few numbers in the
    // highlighted region's formats, as theDesk will show them
    class FormatsPreview : public QWidget {
        public:
            explicit FormatsPreview(QWidget* parent) :
                QWidget(parent) {
                this->setFixedWidth(280);
                this->setMinimumHeight(250);
                frame.setInterval(16);
                QObject::connect(&frame, &QTimer::timeout, this, [this] {
                    if (clock.elapsed() > 700) frame.stop();
                    this->update();
                });
            }

            void setCountry(QLocale::Country country) {
                const QLocale next = LocaleManager::localeForCountry(country);
                if (next == locale && shown) return;
                locale = next;
                shown = true;
                clock.restart();
                frame.start();
                this->update();
            }

        protected:
            void paintEvent(QPaintEvent*) override {
                QPainter p(this);
                p.setRenderHint(QPainter::Antialiasing);
                p.setRenderHint(QPainter::TextAntialiasing);
                const M3::Scheme s = M3::currentScheme();
                const QRectF r = QRectF(this->rect()).adjusted(0, 0, 0, 0);
                QPainterPath card;
                card.addRoundedRect(r, 28, 28);
                p.fillPath(card, s.surfaceContainerHigh);
                if (!shown) return;

                // A change swells the clock's weight and narrows it for a moment
                const double t = qMin(1.0, clock.elapsed() / 700.0);
                const double pulse = std::sin(M_PI * t) * (1 - t * 0.3);
                const QDateTime now = QDateTime::currentDateTime();
                p.setPen(s.onSurface);
                p.setFont(flex(48, 450 + 300 * pulse, 100 - 25 * pulse));
                p.drawText(QRectF(r.left(), r.top() + 22, r.width(), 64), Qt::AlignCenter, locale.toString(now.time(), QLocale::ShortFormat));
                p.setFont(flex(15, 500));
                p.setPen(s.onSurfaceVariant);
                p.drawText(QRectF(r.left() + 12, r.top() + 88, r.width() - 24, 40), Qt::AlignCenter | Qt::TextWordWrap, locale.toString(now.date(), QLocale::LongFormat));

                const bool metric = locale.measurementSystem() == QLocale::MetricSystem;
                const QStringList examples = {locale.toString(1234.56, 'f', 2), locale.toCurrencyString(12.0), metric ? QStringLiteral("24 \u00b0C") : QStringLiteral("75 \u00b0F")};
                const double chipWidth = (r.width() - 24 - 16) / 3.0;
                for (int i = 0; i < examples.size(); i++) {
                    QRectF chip(r.left() + 12 + i * (chipWidth + 8), r.top() + 140, chipWidth, 48);
                    QPainterPath path;
                    path.addRoundedRect(chip, 16, 16);
                    p.fillPath(path, s.surfaceContainerHighest);
                    p.setFont(flex(14, 650, 92));
                    p.setPen(s.onSurface);
                    p.drawText(chip, Qt::AlignCenter, QFontMetrics(p.font()).elidedText(examples.at(i), Qt::ElideRight, int(chip.width() - 8)));
                }
                p.setFont(flex(13, 500));
                p.setPen(s.outline);
                p.drawText(QRectF(r.left(), r.bottom() - 44, r.width(), 24), Qt::AlignCenter, OnboardingRegion::tr("How it will look"));
            }

        private:
            QLocale locale;
            bool shown = false;
            QElapsedTimer clock;
            QTimer frame;
    };
} // namespace

OnboardingRegion::OnboardingRegion(QWidget* parent) :
    OnboardingPage(parent),
    ui(new Ui::OnboardingRegion) {
    ui->setupUi(this);

    ui->titleLabel->setBackButtonShown(true);
    ui->searchEdit->setProperty("m3SearchBar", true); // the M3 search bar pill

    // The list on the left, how it will look on the right
    if (auto* page = qobject_cast<QBoxLayout*>(this->layout())) {
        const int at = page->indexOf(ui->searchEdit);
        page->removeWidget(ui->searchEdit);
        page->removeWidget(ui->countriesWidget);
        auto* left = new QVBoxLayout();
        left->setSpacing(page->spacing());
        left->addWidget(ui->searchEdit);
        left->addWidget(ui->countriesWidget, 1);
        auto* row = new QHBoxLayout();
        row->setSpacing(16);
        row->addLayout(left, 1);
        auto* preview = new FormatsPreview(this);
        d_preview = preview;
        row->addWidget(preview, 0, Qt::AlignTop);
        page->insertLayout(at, row, 1);
    }
    search("");
}

OnboardingRegion::~OnboardingRegion() {
    delete ui;
}

void OnboardingRegion::on_nextButton_clicked() {
    // Next takes the highlighted row, even a suggestion never clicked
    QListWidgetItem* current = ui->countriesWidget->currentItem();
    if (current && current->data(Qt::UserRole).isValid()) {
        StateManager::localeManager()->setFormatCountry(current->data(Qt::UserRole).value<QLocale::Country>());
    }
    StateManager::onboardingManager()->nextStep();
}

QString OnboardingRegion::name() {
    return "OnboardingRegion";
}

QString OnboardingRegion::displayName() {
    return tr("Region");
}
void OnboardingRegion::on_titleLabel_backButtonClicked() {
    StateManager::onboardingManager()->previousStep();
}

void OnboardingRegion::on_countriesWidget_currentItemChanged(QListWidgetItem* current, QListWidgetItem* previous) {
    if (current && current->data(Qt::UserRole).isValid() && d_preview) {
        static_cast<FormatsPreview*>(d_preview)->setCountry(current->data(Qt::UserRole).value<QLocale::Country>());
    }
    if (current && current->data(Qt::UserRole).isValid()) {
        StateManager::localeManager()->setFormatCountry(current->data(Qt::UserRole).value<QLocale::Country>());
    }
}

void OnboardingRegion::showEvent(QShowEvent* event) {
    OnboardingPage::showEvent(event);
    // The list lays itself out when first shown and keeps the current row in
    // view; put "Suggested" back at the top
    if (ui->searchEdit->text().isEmpty()) QTimer::singleShot(0, ui->countriesWidget, &QListWidget::scrollToTop);
}

void OnboardingRegion::on_searchEdit_textChanged(const QString& arg1) {
    search(arg1);
}

void OnboardingRegion::search(QString query) {
    ui->countriesWidget->clear();

    auto header = [this](const QString& text) {
        // A small section heading (not selectable)
        auto* item = new QListWidgetItem(text);
        item->setFlags(Qt::NoItemFlags);
        QFont font = ui->countriesWidget->font();
        font.setPixelSize(12);
        font.setWeight(QFont::DemiBold);
        item->setFont(font);
        ui->countriesWidget->addItem(item);
    };
    auto add = [this](const LocalePluginCommon::Country& country) {
        auto* item = new QListWidgetItem(country.text);
        item->setData(Qt::UserRole, country.country);
        ui->countriesWidget->addItem(item);
        return item;
    };

    QList<LocalePluginCommon::Country> countries = LocalePluginCommon::countries();
    QListWidgetItem* currentItem = nullptr;
    bool currentIsReal = false; // the region set now (not a guess)
    if (query.isEmpty()) {
        // Suggested first: the region set now, where the clock's time zone
        // is, and the system's locale, so it's usually one click
        QList<QLocale::Country> suggested;
        for (const LocalePluginCommon::Country& country : std::as_const(countries)) {
            if (country.isCurrent) suggested.append(country.country);
        }
        for (QLocale::Country guess : {QTimeZone::systemTimeZone().territory(), QLocale::system().territory()}) {
            if (guess != QLocale::AnyTerritory && guess != QLocale::World && !suggested.contains(guess)) suggested.append(guess);
        }
        if (!suggested.isEmpty()) {
            header(tr("Suggested"));
            for (QLocale::Country guess : std::as_const(suggested)) {
                for (const LocalePluginCommon::Country& country : std::as_const(countries)) {
                    if (country.country != guess) continue;
                    QListWidgetItem* item = add(country);
                    if (country.isCurrent || !currentItem) currentItem = item;
                    if (country.isCurrent) currentIsReal = true;
                }
            }
            header(tr("All regions"));
        }
    }
    for (const LocalePluginCommon::Country& country : std::as_const(countries)) {
        if (!country.text.contains(query, Qt::CaseInsensitive)) continue;
        QListWidgetItem* item = add(country);
        if (country.isCurrent && !currentItem) {
            currentItem = item;
            currentIsReal = true;
        }
    }
    if (currentItem) {
        // A guess is only shown, not set: the format country changes when
        // the user picks a row
        QSignalBlocker blocker(currentIsReal ? nullptr : ui->countriesWidget);
        ui->countriesWidget->setCurrentItem(currentItem);
        if (d_preview && currentItem->data(Qt::UserRole).isValid()) static_cast<FormatsPreview*>(d_preview)->setCountry(currentItem->data(Qt::UserRole).value<QLocale::Country>());
        if (query.isEmpty()) ui->countriesWidget->scrollToTop(); // "Suggested" in view
        else ui->countriesWidget->scrollToItem(currentItem);
    }
}
