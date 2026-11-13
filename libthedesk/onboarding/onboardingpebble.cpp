#include "onboardingpebble.h"

#include <QBoxLayout>
#include <QElapsedTimer>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QTimer>
#include <m3motion.h>
#include <m3scheme.h>
#include <onboardingmanager.h>
#include <shapes/m3loadingindicator.h>
#include <statemanager.h>
#include <tsettings.h>
#include <ttitlelabel.h>

namespace {
    const QStringList Faces{"primary", "secondary", "tertiary", "ink"};
} // namespace

struct PebbleFacePicker::Private {
        QElapsedTimer clock;
        QTimer frame;
};

PebbleFacePicker::PebbleFacePicker(QWidget* parent) :
    QWidget(parent) {
    d = new Private();
    d->clock.start();
    d->frame.setInterval(16);
    connect(&d->frame, &QTimer::timeout, this, qOverload<>(&QWidget::update));
    this->setCursor(Qt::PointingHandCursor);
    this->setFixedHeight(130);
}

PebbleFacePicker::~PebbleFacePicker() {
    delete d;
}

QSize PebbleFacePicker::sizeHint() const {
    return QSize(440, 130);
}

void PebbleFacePicker::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    d->frame.start();
}

void PebbleFacePicker::hideEvent(QHideEvent* event) {
    QWidget::hideEvent(event);
    d->frame.stop();
}

void PebbleFacePicker::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event)
    const M3::Scheme s = M3::currentScheme();
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QList<QPair<QColor, QColor>> colours{{s.onPrimaryContainer, s.primaryContainer}, {s.onSecondaryContainer, s.secondaryContainer}, {s.onTertiaryContainer, s.tertiaryContainer}, {s.inverseOnSurface, s.inverseSurface}};
    const QStringList names{tr("Primary"), tr("Calm"), tr("Bright"), tr("Ink")};
    QString current = tSettings().value(QStringLiteral("Assistant/face")).toString();
    if (!Faces.contains(current)) current = Faces.first();
    for (int i = 0; i < 4; i++) {
        const QPointF c(50 + i * 110, 50);
        // they think while it shows (still under calm motion)
        if (M3Motion::expressive()) M3LoadingIndicator::paint(&p, QRectF(c.x() - 34, c.y() - 34, 68, 68), d->clock.elapsed() + i * 300, colours.at(i).first, colours.at(i).second);
        else M3LoadingIndicator::paintDeterminate(&p, QRectF(c.x() - 34, c.y() - 34, 68, 68), 0.0f, colours.at(i).first, colours.at(i).second);
        const bool chosen = Faces.at(i) == current;
        if (chosen) {
            p.setPen(QPen(s.primary, 3));
            p.setBrush(Qt::NoBrush);
            p.drawEllipse(c, 43, 43);
        }
        QFont font(QStringLiteral("Roboto Flex"));
        font.setPixelSize(13);
        font.setVariableAxis(QFont::Tag("wght"), chosen ? 650 : 480);
        p.setFont(font);
        p.setPen(chosen ? s.primary : s.onSurfaceVariant);
        p.drawText(QRectF(c.x() - 50, c.y() + 50, 100, 24), Qt::AlignCenter, names.at(i));
    }
}

void PebbleFacePicker::mousePressEvent(QMouseEvent* event) {
    const int i = int((event->position().x() + 5) / 110);
    if (i < 0 || i > 3) return;
    tSettings().setValue(QStringLiteral("Assistant/face"), Faces.at(i));
    this->update();
}

OnboardingPebble::OnboardingPebble(QWidget* parent) :
    OnboardingPage(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(12);

    auto* title = new tTitleLabel(this);
    title->setObjectName(QStringLiteral("titleLabel"));
    title->setText(tr("Meet Pebble"));
    layout->addWidget(title);

    auto* description = new QLabel(tr("Your desk's own assistant. Press Super+Space and ask, like you'd text a friend: it changes settings, looks at things with you and gets stuff done."), this);
    description->setObjectName(QStringLiteral("descriptionLabel"));
    description->setWordWrap(true);
    layout->addWidget(description);

    auto* points = new QLabel(tr("It shows what it can see, and you can hide any of it.\nIt asks before anything risky: commands, file changes, closing windows.\nEsc while it works puts it in the bar until it's done."), this);
    points->setWordWrap(true);
    layout->addWidget(points);

    auto* pick = new QLabel(tr("Pick its colour:"), this);
    layout->addWidget(pick);
    layout->addWidget(new PebbleFacePicker(this));
    layout->addStretch();

    auto* bottom = new QHBoxLayout();
    bottom->addStretch();
    auto* next = new QPushButton(tr("Next"), this);
    next->setObjectName(QStringLiteral("nextButton"));
    next->setIcon(QIcon::fromTheme(QStringLiteral("go-next")));
    connect(next, &QPushButton::clicked, this, [] {
        // met here: Pebble's own tour won't repeat it
        tSettings().setValue(QStringLiteral("Assistant/introduced"), true);
        StateManager::onboardingManager()->nextStep();
    });
    bottom->addWidget(next);
    layout->addLayout(bottom);
}

QString OnboardingPebble::name() {
    return QStringLiteral("OnboardingPebble");
}

QString OnboardingPebble::displayName() {
    return tr("Pebble");
}
