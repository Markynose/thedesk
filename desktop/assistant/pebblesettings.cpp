#include "pebblesettings.h"

#include "assistantpanel.h"
#include "claudebrain.h"

#include <QDesktopServices>
#include <QElapsedTimer>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QScrollArea>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <m3scheme.h>
#include <onboarding/onboardingpebble.h>
#include <shapes/m3loadingindicator.h>
#include <statemanager.h>
#include <statuscentermanager.h>
#include <tsettings.h>
#include <tsubtitlelabel.h>
#include <tswitch.h>
#include <ttitlelabel.h>

namespace {
    tSubtitleLabel* subtitle(const QString& text) {
        auto* label = new tSubtitleLabel(nullptr);
        label->setText(text);
        return label;
    }

    QWidget* row(const QString& title, const QString& text, QWidget* control) {
        auto* widget = new QWidget();
        auto* layout = new QHBoxLayout(widget);
        layout->setContentsMargins(0, 6, 0, 6);
        auto* words = new QVBoxLayout();
        auto* titleLabel = new QLabel(title);
        auto* textLabel = new QLabel(text);
        textLabel->setWordWrap(true);
        QPalette palette = textLabel->palette();
        palette.setColor(QPalette::WindowText, M3::currentScheme().onSurfaceVariant);
        textLabel->setPalette(palette);
        words->addWidget(titleLabel);
        words->addWidget(textLabel);
        layout->addLayout(words, 1);
        layout->addWidget(control);
        return widget;
    }
} // namespace

struct PebbleSettings::Private {
        QListWidget* memories;
        QWidget* forgetButtons;
        QLabel* nothing;
        tSettings settings;
};

PebbleSettings::PebbleSettings() :
    StatusCenterPane() {
    d = new Private();
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    auto* title = new tTitleLabel();
    title->setText(tr("Pebble"));
    title->setBackButtonIsMenu(true);
    title->setBackButtonShown(StateManager::statusCenterManager()->isHamburgerMenuRequired());
    connect(StateManager::statusCenterManager(), &StatusCenterManager::isHamburgerMenuRequiredChanged, title, &tTitleLabel::setBackButtonShown);
    connect(title, &tTitleLabel::backButtonClicked, this, [] {
        StateManager::statusCenterManager()->showStatusCenterHamburgerMenu();
    });
    outer->addWidget(title);

    auto* scroll = new QScrollArea();
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto* page = new QWidget();
    auto* centre = new QHBoxLayout(page);
    auto* column = new QWidget();
    column->setFixedWidth(StateManager::statusCenterManager()->preferredContentWidth());
    centre->addStretch();
    centre->addWidget(column);
    centre->addStretch();
    auto* layout = new QVBoxLayout(column);
    scroll->setWidget(page);
    outer->addWidget(scroll);

    // Its colour, and the tour again
    layout->addWidget(subtitle(tr("Pebble's colour")));
    layout->addWidget(new PebbleFacePicker());
    auto* replay = new QPushButton(QIcon::fromTheme(QStringLiteral("media-playlist-repeat")), tr("Replay the tour"));
    connect(replay, &QPushButton::clicked, this, [] {
        tSettings().setValue(QStringLiteral("Assistant/introduced"), false);
        AssistantPanel::instance()->open();
    });
    layout->addWidget(replay, 0, Qt::AlignLeft);

    // Nudges and the selection button
    layout->addWidget(subtitle(tr("Little things")));
    auto toggle = [this](const QString& key) {
        auto* sw = new tSwitch();
        sw->setChecked(d->settings.value(key).toBool());
        connect(sw, &tSwitch::toggled, this, [this, key](bool on) {
            d->settings.setValue(key, on);
        });
        return sw;
    };
    layout->addWidget(row(tr("Nudges"), tr("A snackbar when an answer is ready while Pebble's in the bar, and a tip when the battery runs low."), toggle(QStringLiteral("Assistant/nudges"))));
    layout->addWidget(row(tr("Button on selected text"), tr("Select some text and Pebble's face shows next to it. Click it to ask about the text."), toggle(QStringLiteral("Assistant/selectionButton"))));

    // What it remembers
    layout->addWidget(subtitle(tr("What Pebble remembers")));
    d->nothing = new QLabel(tr("Nothing yet. Tell Pebble to remember something and it shows here."));
    d->nothing->setWordWrap(true);
    layout->addWidget(d->nothing);
    d->memories = new QListWidget();
    d->memories->setMinimumHeight(160);
    layout->addWidget(d->memories);
    d->forgetButtons = new QWidget();
    auto* forgetRow = new QHBoxLayout(d->forgetButtons);
    forgetRow->setContentsMargins(0, 0, 0, 0);
    auto* forget = new QPushButton(QIcon::fromTheme(QStringLiteral("edit-delete")), tr("Forget this"));
    auto* forgetAll = new QPushButton(tr("Forget everything"));
    forgetAll->setProperty("type", "destructive");
    connect(forget, &QPushButton::clicked, this, [this] {
        const int row = d->memories->currentRow();
        if (row < 0) return;
        QStringList facts = ClaudeBrain::remembered();
        if (row < facts.size()) facts.removeAt(row);
        ClaudeBrain::setRemembered(facts);
        refreshMemories();
    });
    connect(forgetAll, &QPushButton::clicked, this, [this] {
        ClaudeBrain::setRemembered({});
        refreshMemories();
    });
    forgetRow->addWidget(forget);
    forgetRow->addWidget(forgetAll);
    forgetRow->addStretch();
    layout->addWidget(d->forgetButtons);

    // Its prompt
    layout->addWidget(subtitle(tr("How Pebble behaves")));
    auto* promptText = new QLabel(tr("Pebble's instructions live in a file you can change. New chats use what's there."));
    promptText->setWordWrap(true);
    layout->addWidget(promptText);
    auto* promptRow = new QHBoxLayout();
    auto* edit = new QPushButton(QIcon::fromTheme(QStringLiteral("document-edit")), tr("Edit the instructions"));
    auto* reset = new QPushButton(tr("Back to the default"));
    connect(edit, &QPushButton::clicked, this, [] {
        QDesktopServices::openUrl(QUrl::fromLocalFile(ClaudeBrain::promptPath()));
    });
    connect(reset, &QPushButton::clicked, this, [] {
        ClaudeBrain::resetPrompt();
    });
    promptRow->addWidget(edit);
    promptRow->addWidget(reset);
    promptRow->addStretch();
    layout->addLayout(promptRow);
    layout->addStretch();
}

PebbleSettings::~PebbleSettings() {
    delete d;
}

void PebbleSettings::showEvent(QShowEvent* event) {
    StatusCenterPane::showEvent(event);
    refreshMemories();
}

void PebbleSettings::refreshMemories() {
    d->memories->clear();
    const QStringList facts = ClaudeBrain::remembered();
    d->memories->addItems(facts);
    d->memories->setVisible(!facts.isEmpty());
    d->nothing->setVisible(facts.isEmpty());
    d->forgetButtons->setVisible(!facts.isEmpty());
}

QString PebbleSettings::name() {
    return QStringLiteral("PebbleSettings");
}

QString PebbleSettings::displayName() {
    return tr("Pebble");
}

QIcon PebbleSettings::icon() {
    return QIcon::fromTheme(QStringLiteral("preferences-desktop-assistant"), QIcon::fromTheme(QStringLiteral("dialog-information")));
}

QWidget* PebbleSettings::leftPane() {
    return nullptr;
}
