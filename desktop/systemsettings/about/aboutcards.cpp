/****************************************
 *
 *   theDesk - System Settings: About, redesigned
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 * *************************************/
#include "aboutcards.h"

#include <QDate>
#include <QDesktopServices>
#include <QFile>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <m3motion.h>
#include <m3scheme.h>
#include <onboarding/onboardingintro.h>
#include <shapes/materialshapes.h>
#include <shapes/shapepath.h>
#include <tapplication.h>

namespace {
    constexpr double TitlePx = 64;
    constexpr int HeroHeight = 168;

    QFont flex(const QFont& base, double px, double wght, double wdth = 100) {
        QFont font = base;
        font.setFamily(QStringLiteral("Roboto Flex"));
        font.setPixelSize(qRound(px));
        font.setVariableAxis(QFont::Tag("wght"), float(wght));
        font.setVariableAxis(QFont::Tag("wdth"), float(wdth));
        font.setVariableAxis(QFont::Tag("opsz"), float(qBound(8.0, px, 144.0)));
        return font;
    }

    QFont symbolFont(double px) {
        QFont font(QStringLiteral("Material Symbols Rounded"));
        font.setPixelSize(qRound(px));
        font.setVariableAxis(QFont::Tag("FILL"), 1);
        font.setVariableAxis(QFont::Tag("wght"), 500);
        font.setVariableAxis(QFont::Tag("opsz"), float(qBound(20.0, px, 48.0)));
        return font;
    }

    // /etc/os-release, key by key
    QHash<QString, QString> osRelease() {
        QHash<QString, QString> values;
        for (const QString& path : {QStringLiteral("/etc/os-release"), QStringLiteral("/usr/lib/os-release")}) {
            QFile file(path);
            if (!file.open(QFile::ReadOnly)) continue;
            for (const QByteArray& raw : file.readAll().split('\n')) {
                const QString line = QString::fromUtf8(raw).trimmed();
                const int eq = line.indexOf(QLatin1Char('='));
                if (line.startsWith(QLatin1Char('#')) || eq <= 0) continue;
                QString value = line.mid(eq + 1).trimmed();
                if (value.size() >= 2 && (value.startsWith(QLatin1Char('"')) || value.startsWith(QLatin1Char('\'')))) value = value.mid(1, value.size() - 2);
                values.insert(line.left(eq), value);
            }
            break;
        }
        return values;
    }

    QString readDmi(const char* name) {
        QFile file(QStringLiteral("/sys/class/dmi/id/") + QLatin1String(name));
        if (!file.open(QFile::ReadOnly)) return QString();
        const QString value = QString::fromUtf8(file.readAll()).trimmed();
        // Firmware fills unset fields with placeholders
        static const QStringList junk = {QStringLiteral("None"), QStringLiteral("To be filled by O.E.M."), QStringLiteral("System Product Name"), QStringLiteral("Default string"), QStringLiteral("Not Applicable")};
        return junk.contains(value) ? QString() : value;
    }

    bool isLaptop() {
        static const QList<int> portable = {8, 9, 10, 11, 14, 30, 31, 32};
        return portable.contains(readDmi("chassis_type").toInt());
    }

    // A rounded card that rises into place on the expressive spring
    class Card : public QWidget {
        public:
            explicit Card(QWidget* parent) :
                QWidget(parent) {
                enter = new M3SpringAnimation(this);
                enter->setToken(M3Motion::SpatialDefault);
                enter->snapTo(1.0);
                QObject::connect(enter, &M3SpringAnimation::valueChanged, this, qOverload<>(&QWidget::update));
            }

            void hold() {
                enter->snapTo(0.0);
                // Its buttons wait with it
                for (QWidget* child : this->findChildren<QWidget*>(Qt::FindDirectChildrenOnly)) child->hide();
                this->update();
            }

            void playEntrance(int delay) {
                enter->snapTo(0.0);
                for (QWidget* child : this->findChildren<QWidget*>(Qt::FindDirectChildrenOnly)) child->hide();
                QTimer::singleShot(delay, this, [this] {
                    enter->animateTo(1.0);
                    QTimer::singleShot(180, this, [this] {
                        for (QWidget* child : this->findChildren<QWidget*>(Qt::FindDirectChildrenOnly)) child->show();
                    });
                });
            }

        protected:
            M3SpringAnimation* enter;
            virtual void paintContent(QPainter* p, const M3::Scheme& s, const QRectF& r) = 0;

            void paintEvent(QPaintEvent*) override {
                QPainter p(this);
                p.setRenderHint(QPainter::Antialiasing);
                p.setRenderHint(QPainter::TextAntialiasing);
                const double v = enter->currentValue().toDouble();
                if (v <= 0.001) return;
                p.setOpacity(qBound(0.0, v * 1.4, 1.0));
                p.translate(0, (1 - v) * 28);
                const M3::Scheme s = M3::currentScheme();
                QPainterPath card;
                card.addRoundedRect(QRectF(this->rect()), 28, 28);
                p.fillPath(card, s.surfaceContainerHigh);
                paintContent(&p, s, QRectF(this->rect()).adjusted(22, 20, -22, -20));
            }
    };

    // The distribution: its logo, name, what it's based on, its release
    class SystemCard : public Card {
        public:
            explicit SystemCard(QWidget* parent) :
                Card(parent) {
                const QHash<QString, QString> os = osRelease();
                name = os.value(QStringLiteral("PRETTY_NAME"), os.value(QStringLiteral("NAME"), QStringLiteral("Linux")));
                const QString id = os.value(QStringLiteral("ID"));
                const QStringList like = os.value(QStringLiteral("ID_LIKE")).split(QLatin1Char(' '), Qt::SkipEmptyParts);
                static const QStringList archBased = {QStringLiteral("artix"), QStringLiteral("endeavouros"), QStringLiteral("manjaro"), QStringLiteral("cachyos"), QStringLiteral("garuda")};
                if (id != QStringLiteral("arch") && (like.contains(QStringLiteral("arch")) || archBased.contains(id))) basedOn = tr("Based on Arch Linux");
                else if (!like.isEmpty() && !like.contains(id)) basedOn = tr("Based on %1").arg(like.first().at(0).toUpper() + like.first().mid(1));

                // Rolling releases date their builds: "Rolling · 2 November 2026"
                const QString version = os.value(QStringLiteral("VERSION_ID"));
                const QDate date = QDate::fromString(version, QStringLiteral("yyyyMMdd"));
                const bool rolling = os.value(QStringLiteral("BUILD_ID")) == QStringLiteral("rolling") || id == QStringLiteral("arch") || id == QStringLiteral("artix");
                if (rolling) release = date.isValid() ? tr("Rolling release · %1").arg(QLocale().toString(date, QStringLiteral("d MMMM yyyy"))) : tr("Rolling release");
                else if (!version.isEmpty()) release = tr("Version %1").arg(os.value(QStringLiteral("VERSION"), version));

                url = os.value(QStringLiteral("HOME_URL"));
                const QString logoName = os.value(QStringLiteral("LOGO"));
                logo = QIcon::fromTheme(logoName);
                for (const QString& ext : {QStringLiteral(".svg"), QStringLiteral(".png")}) {
                    if (!logo.isNull()) break;
                    if (QFile::exists(QStringLiteral("/usr/share/pixmaps/") + logoName + ext)) logo = QIcon(QStringLiteral("/usr/share/pixmaps/") + logoName + ext);
                }
                this->setMouseTracking(true);
                this->setAccessibleName(QStringLiteral("%1. %2. %3").arg(name, basedOn, release));
            }

        protected:
            void paintContent(QPainter* p, const M3::Scheme& s, const QRectF& r) override {
                // The logo, in a soft circle
                p->setPen(Qt::NoPen);
                p->setBrush(s.surfaceContainerHighest);
                p->drawEllipse(QRectF(r.left(), r.top(), 56, 56));
                if (!logo.isNull()) logo.paint(p, QRect(int(r.left() + 10), int(r.top() + 10), 36, 36));
                p->setPen(s.onSurfaceVariant);
                p->setFont(flex(this->font(), 13, 500));
                p->drawText(QRectF(r.left() + 72, r.top() + 2, r.width() - 72, 18), Qt::AlignLeft | Qt::AlignVCenter, tr("Your system"));
                p->setPen(s.onSurface);
                p->setFont(flex(this->font(), 24, 680, 108));
                p->drawText(QRectF(r.left() + 72, r.top() + 20, r.width() - 72, 32), Qt::AlignLeft | Qt::AlignVCenter, p->fontMetrics().elidedText(name, Qt::ElideRight, int(r.width() - 72)));
                double y = r.top() + 72;
                p->setFont(flex(this->font(), 14, 450));
                p->setPen(s.onSurfaceVariant);
                for (const QString& line : {basedOn, release}) {
                    if (line.isEmpty()) continue;
                    p->drawText(QRectF(r.left(), y, r.width(), 20), Qt::AlignLeft | Qt::AlignVCenter, line);
                    y += 22;
                }
                if (!url.isEmpty()) {
                    linkRect = QRectF(r.left(), r.bottom() - 18, r.width(), 18);
                    p->setPen(hoverLink ? s.primary.lighter(115) : s.primary);
                    QFont f = flex(this->font(), 14, 560);
                    f.setUnderline(hoverLink);
                    p->setFont(f);
                    QString host = QUrl(url).host();
                    if (host.startsWith(QStringLiteral("www."))) host = host.mid(4);
                    p->drawText(linkRect, Qt::AlignLeft | Qt::AlignVCenter, host + QStringLiteral("  ↗"));
                }
            }

            void mouseMoveEvent(QMouseEvent* event) override {
                const bool over = linkRect.contains(event->position());
                if (over != hoverLink) {
                    hoverLink = over;
                    this->setCursor(over ? Qt::PointingHandCursor : Qt::ArrowCursor);
                    this->update();
                }
            }

            void mouseReleaseEvent(QMouseEvent* event) override {
                if (linkRect.contains(event->position())) QDesktopServices::openUrl(QUrl(url));
            }

        private:
            QString name, basedOn, release, url;
            QIcon logo;
            QRectF linkRect;
            bool hoverLink = false;
    };

    // This device: its model in a shape, who made it, its name
    class DeviceCard : public Card {
        public:
            explicit DeviceCard(QWidget* parent) :
                Card(parent) {
                QString vendorName = readDmi("sys_vendor");
                // "LENOVO" reads better as "Lenovo"
                if (vendorName == vendorName.toUpper() && vendorName.size() > 3) vendorName = vendorName.at(0) + vendorName.mid(1).toLower();
                vendor = vendorName;
                model = readDmi("product_version");
                if (model.isEmpty() || model.at(0).isDigit()) model = readDmi("product_name");
                if (model.isEmpty()) model = isLaptop() ? tr("Laptop") : tr("Computer");
                rename = new QPushButton(this);
                rename->setText(tr("Rename"));
                rename->setIcon(QIcon::fromTheme(QStringLiteral("edit-rename")));
                rename->setProperty("m3ButtonStyle", QStringLiteral("tonal"));
            }

            void setHostname(const QString& name) {
                hostname = name;
                this->setAccessibleName(QStringLiteral("%1 %2, %3").arg(vendor, model, hostname));
                this->update();
            }

            QPushButton* rename;

        protected:
            void resizeEvent(QResizeEvent*) override {
                const QSize size = rename->sizeHint();
                rename->setGeometry(22, this->height() - 20 - size.height(), size.width(), size.height());
            }

            void paintContent(QPainter* p, const M3::Scheme& s, const QRectF& r) override {
                // The device's symbol in a cookie
                const QRectF shape(r.left(), r.top(), 56, 56);
                p->setPen(Qt::NoPen);
                p->setBrush(s.primaryContainer);
                p->drawPath(m3shapes::toPath(MaterialShapes::cookie9Sided(), shape));
                p->setPen(s.onPrimaryContainer);
                p->setFont(symbolFont(30));
                p->drawText(shape, Qt::AlignCenter, isLaptop() ? QStringLiteral("laptop") : QStringLiteral("desktop_windows"));
                p->setPen(s.onSurfaceVariant);
                p->setFont(flex(this->font(), 13, 500));
                p->drawText(QRectF(r.left() + 72, r.top() + 2, r.width() - 72, 18), Qt::AlignLeft | Qt::AlignVCenter, vendor.isEmpty() ? tr("This device") : vendor);
                p->setPen(s.onSurface);
                p->setFont(flex(this->font(), 24, 680, 108));
                p->drawText(QRectF(r.left() + 72, r.top() + 20, r.width() - 72, 32), Qt::AlignLeft | Qt::AlignVCenter, p->fontMetrics().elidedText(model, Qt::ElideRight, int(r.width() - 72)));
                p->setFont(flex(this->font(), 14, 450));
                p->setPen(s.onSurfaceVariant);
                p->drawText(QRectF(r.left(), r.top() + 72, r.width(), 20), Qt::AlignLeft | Qt::AlignVCenter, tr("Named %1").arg(hostname));
            }

        private:
            QString vendor, model, hostname;
    };

    // The specs: a symbol in a tonal circle, what it is, what it says
    class SpecsCard : public Card {
        public:
            explicit SpecsCard(QWidget* parent) :
                Card(parent) {}

            void setSpecs(const QList<AboutPage::Spec>& list) {
                specs = list;
                const int rows = int((specs.size() + 1) / 2);
                this->setFixedHeight(40 + rows * 64 - 8);
                this->update();
            }

        protected:
            void paintContent(QPainter* p, const M3::Scheme& s, const QRectF& r) override {
                const double column = (r.width() - 24) / 2;
                for (int i = 0; i < specs.size(); i++) {
                    const AboutPage::Spec& spec = specs.at(i);
                    const QRectF cell(r.left() + (i % 2) * (column + 24), r.top() + (i / 2) * 64, column, 56);
                    p->setPen(Qt::NoPen);
                    p->setBrush(s.secondaryContainer);
                    p->drawEllipse(QRectF(cell.left(), cell.top() + 6, 40, 40));
                    p->setPen(s.onSecondaryContainer);
                    p->setFont(symbolFont(22));
                    p->drawText(QRectF(cell.left(), cell.top() + 6, 40, 40), Qt::AlignCenter, spec.symbol);
                    p->setPen(s.onSurfaceVariant);
                    p->setFont(flex(this->font(), 12, 500));
                    p->drawText(QRectF(cell.left() + 54, cell.top() + 6, cell.width() - 54, 18), Qt::AlignLeft | Qt::AlignVCenter, spec.label);
                    p->setPen(s.onSurface);
                    p->setFont(flex(this->font(), 15, 520));
                    p->drawText(QRectF(cell.left() + 54, cell.top() + 24, cell.width() - 54, 22), Qt::AlignLeft | Qt::AlignVCenter, p->fontMetrics().elidedText(spec.value, Qt::ElideMiddle, int(cell.width() - 54)));
                }
            }

        private:
            QList<AboutPage::Spec> specs;
    };

    bool playedThisSession = false;
} // namespace

struct AboutPagePrivate {
        QWidget* hero;
        QLabel* version;
        QHBoxLayout* chips;
        SystemCard* system;
        DeviceCard* device;
        SpecsCard* specs;
        QLabel* copyright;
        OnboardingIntro* intro;
        QWidget* titleHotspot;
        std::function<void()> rename;
        bool opened = false;
};

AboutPage::AboutPage(QWidget* parent) :
    QWidget(parent) {
    d = new AboutPagePrivate();
    this->setMinimumWidth(620);
    this->setMaximumWidth(860);
    this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 8, 0, 24);
    layout->setSpacing(12);

    // The hero: room for "theDesk" (drawn by the intro), the version, chips
    d->hero = new QWidget(this);
    d->hero->setFixedHeight(HeroHeight);
    auto* heroLayout = new QVBoxLayout(d->hero);
    heroLayout->setContentsMargins(8, int(TitlePx * 1.3), 8, 0);
    heroLayout->setSpacing(14);
    d->version = new QLabel(d->hero);
    d->version->setText(tr("Version %1 · Material 3 Expressive · a desktop for X11").arg(tApplication::applicationVersion()));
    d->version->setFont(flex(this->font(), 15, 450));
    heroLayout->addWidget(d->version);
    d->chips = new QHBoxLayout();
    d->chips->setSpacing(8);
    d->chips->addStretch();
    heroLayout->addLayout(d->chips);
    heroLayout->addStretch();
    layout->addWidget(d->hero);

    auto* row = new QHBoxLayout();
    row->setSpacing(12);
    d->system = new SystemCard(this);
    d->system->setFixedHeight(176);
    d->device = new DeviceCard(this);
    d->device->setFixedHeight(176);
    connect(d->device->rename, &QPushButton::clicked, this, [this] {
        if (d->rename) d->rename();
    });
    row->addWidget(d->system, 1);
    row->addWidget(d->device, 1);
    layout->addLayout(row);

    d->specs = new SpecsCard(this);
    layout->addWidget(d->specs);

    d->copyright = new QLabel(this);
    d->copyright->setWordWrap(true);
    d->copyright->setText(tr("Copyright © Victor Tran and contributors %1; the Material 3 Expressive redesign, its contributors. Licensed under the terms of the GNU General Public License, version 3 or later.").arg(tApplication::copyrightYear()));
    d->copyright->setFont(flex(this->font(), 12, 420));
    d->copyright->setContentsMargins(8, 4, 8, 0);
    layout->addWidget(d->copyright);
    layout->addStretch();

    // The title: onboarding's opening, staying at rest; a click plays it again
    d->intro = new OnboardingIntro(this);
    d->intro->setStays(true);
    // It plays over the page and never stands in its way: the cards are
    // there and take clicks while it plays
    d->intro->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    d->titleHotspot = new QWidget(this);
    d->titleHotspot->setCursor(Qt::PointingHandCursor);
    d->titleHotspot->setToolTip(tr("Play it again"));
    struct Replay : QObject {
            OnboardingIntro* intro;
            QList<Card*> cards;
            bool eventFilter(QObject*, QEvent* event) override {
                if (event->type() != QEvent::MouseButtonRelease) return false;
                intro->raise();
                intro->replay();
                return true;
            }
    };
    auto* replay = new Replay();
    replay->setParent(this);
    replay->intro = d->intro;
    replay->cards = {d->system, d->device, d->specs};
    d->titleHotspot->installEventFilter(replay);
}

AboutPage::~AboutPage() {
    delete d;
}

void AboutPage::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    d->intro->setGeometry(this->rect());
    d->intro->setPlacement(QPointF(16, d->hero->y() + TitlePx * 1.05), TitlePx);
    d->titleHotspot->setGeometry(8, d->hero->y(), int(TitlePx * 4.6), int(TitlePx * 1.25));
    d->intro->raise();
    // Colours that follow the scheme
    const M3::Scheme s = M3::currentScheme();
    QPalette muted = d->version->palette();
    muted.setColor(QPalette::WindowText, s.onSurfaceVariant);
    d->version->setPalette(muted);
    muted.setColor(QPalette::WindowText, s.outline);
    d->copyright->setPalette(muted);
}

void AboutPage::opened() {
    if (d->opened) return;
    d->opened = true;
    // The cards rise straight away; the title plays alongside (the first
    // time this session), or is simply there
    if (!playedThisSession && M3Motion::expressive()) {
        playedThisSession = true;
        d->intro->raise();
        d->intro->start(250);
    } else {
        d->intro->showResting();
    }
    int delay = 0;
    for (Card* card : {static_cast<Card*>(d->system), static_cast<Card*>(d->device), static_cast<Card*>(d->specs)}) {
        card->playEntrance(delay);
        delay += 60;
    }
}

void AboutPage::setSpecs(const QList<Spec>& specs) {
    d->specs->setSpecs(specs);
}

void AboutPage::setHostname(const QString& hostname) {
    d->device->setHostname(hostname);
}

QPushButton* AboutPage::addChip(const QString& symbolName, const QString& text, std::function<void()> clicked) {
    auto* chip = new QPushButton(d->hero);
    chip->setText(text);
    chip->setIcon(QIcon::fromTheme(symbolName));
    chip->setProperty("m3ButtonStyle", QStringLiteral("tonal"));
    connect(chip, &QPushButton::clicked, this, [clicked] {
        if (clicked) clicked();
    });
    d->chips->insertWidget(d->chips->count() - 1, chip);
    return chip;
}

void AboutPage::onRename(std::function<void()> rename) {
    d->rename = std::move(rename);
}
