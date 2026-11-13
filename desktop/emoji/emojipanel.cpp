#include "emojipanel.h"

#include "../clipboard/clipboardhistory.h"
#include "../clipboard/pasteback.h"

#include <QApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QKeyEvent>
#include <QLineEdit>
#include <QPainter>
#include <QPainterPath>
#include <QScreen>
#include <QTimer>
#include <QVariantAnimation>
#include <Wm/desktopwm.h>
#include <compositor.h>
#include <keygrab.h>
#include <m3motion.h>
#include <m3scheme.h>
#include <shapes/materialshapes.h>
#include <shapes/shapepath.h>
#include <tsettings.h>

namespace {
    constexpr int Width = 440;
    constexpr int Pad = 12;
    constexpr int SearchHeight = 56;
    constexpr int ChipsHeight = 44;
    constexpr int Cols = 8;
    constexpr int Cell = (Width - 2 * Pad) / Cols;
    constexpr int HeaderHeight = 30;
    constexpr int ListHeight = Cell * 5 + 34;
    constexpr int HintHeight = 32;
    constexpr int MaxRecent = 16;

    struct Item {
            QString text, name;
            int group;
    };
    struct Data {
            QList<Item> items;
            QStringList groups;
            QHash<QString, int> byText;
    };

    const Data& emojiData() {
        static Data data = [] {
            Data d;
            QFile file(QStringLiteral(":/thedesk/desktop/emoji/emoji.txt"));
            if (!file.open(QFile::ReadOnly)) return d;
            for (const QString& line : QString::fromUtf8(file.readAll()).split('\n')) {
                if (line.isEmpty()) continue;
                const int tab = line.indexOf('\t');
                if (tab < 0) {
                    // "# group"; the file's first comment line is skipped as it has no group after it yet
                    if (line.startsWith(QStringLiteral("# ")) && !line.contains(QStringLiteral("Unicode"))) d.groups.append(line.mid(2));
                    continue;
                }
                d.byText.insert(line.left(tab), int(d.items.size()));
                d.items.append({line.left(tab), line.mid(tab + 1), int(d.groups.size()) - 1});
            }
            return d;
        }();
        return data;
    }

    QString symbolFor(const QString& group) {
        static const QHash<QString, QString> symbols = {
            {QStringLiteral("Smileys & Emotion"), QStringLiteral("mood")},
            {QStringLiteral("People & Body"), QStringLiteral("emoji_people")},
            {QStringLiteral("Animals & Nature"), QStringLiteral("pets")},
            {QStringLiteral("Food & Drink"), QStringLiteral("restaurant")},
            {QStringLiteral("Travel & Places"), QStringLiteral("flight")},
            {QStringLiteral("Activities"), QStringLiteral("sports_soccer")},
            {QStringLiteral("Objects"), QStringLiteral("lightbulb")},
            {QStringLiteral("Symbols"), QStringLiteral("emoji_symbols")},
            {QStringLiteral("Flags"), QStringLiteral("flag")},
            {QStringLiteral("Symbols & marks"), QStringLiteral("functions")},
        };
        return symbols.value(group, QStringLiteral("schedule"));
    }

    QFont flex(double px, int wght) {
        QFont font(QStringLiteral("Roboto Flex"));
        font.setPixelSize(qRound(px));
        font.setVariableAxis(QFont::Tag("wght"), float(wght));
        return font;
    }

    QFont symbols(double px) {
        QFont font(QStringLiteral("Material Symbols Rounded"));
        font.setPixelSize(qRound(px));
        font.setVariableAxis(QFont::Tag("FILL"), 1);
        font.setVariableAxis(QFont::Tag("opsz"), 20);
        return font;
    }

    QFont emojiFont(double px) {
        QFont font;
        font.setFamilies({QStringLiteral("Noto Color Emoji"), QStringLiteral("Roboto Flex")});
        font.setPixelSize(qRound(px));
        return font;
    }
} // namespace

struct EmojiPanel::Private {
        QLineEdit* search;
        tSettings settings;
        struct Cell {
                int item;
                QRectF rect; // in the list's coordinates
        };
        struct Section {
                QString title, group; // group: which chip it belongs to ("" for results)
                double y;
        };
        QList<Cell> cells;
        QList<Section> sections;
        double contentHeight = 0;
        int selected = 0;
        DesktopWmWindowPtr previous;

        M3SpringAnimation* reveal;
        M3SpringAnimation* highlight; // the cookie glides from emoji to emoji
        M3SpringAnimation* pop;       // the chosen emoji grows a little
        M3SpringAnimation* scroll;
        M3SpringAnimation* chip;      // the active category's pill
        QVariantAnimation* leave;
        QElapsedTimer opened;
        bool open = false;
};

EmojiPanel* EmojiPanel::instance() {
    static auto* panel = new EmojiPanel();
    return panel;
}

void EmojiPanel::setUp() {
    connect(new KeyGrab(QKeySequence(Qt::MetaModifier | Qt::Key_Period), QStringLiteral("emoji")), &KeyGrab::activated, instance(), [] {
        if (instance()->d->open) instance()->close();
        else instance()->open();
    });
}

EmojiPanel::EmojiPanel() :
    QDialog(nullptr) {
    d = new Private();
    this->setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    this->setAttribute(Qt::WA_TranslucentBackground);
    this->setMouseTracking(true);
    this->setWindowTitle(tr("Emoji"));

    d->search = new QLineEdit(this);
    d->search->setProperty("m3SearchBar", true);
    d->search->setPlaceholderText(tr("Search emoji and symbols"));
    d->search->installEventFilter(this);
    connect(d->search, &QLineEdit::textChanged, this, [this] {
        relayout();
        d->scroll->snapTo(0.0);
        select(0, false);
    });

    auto spring = [this](M3Motion::Token token, const QVariant& from) {
        auto* animation = new M3SpringAnimation(this);
        animation->setToken(token);
        animation->snapTo(from);
        connect(animation, &M3SpringAnimation::valueChanged, this, qOverload<>(&QWidget::update));
        return animation;
    };
    d->reveal = spring(M3Motion::SpatialDefault, 0.0);
    d->highlight = spring(M3Motion::SpatialFast, QRectF());
    d->pop = spring(M3Motion::SpatialFast, 0.0);
    d->scroll = spring(M3Motion::SpatialDefault, 0.0);
    d->chip = spring(M3Motion::SpatialFast, 0.0);

    d->leave = new QVariantAnimation(this);
    d->leave->setStartValue(1.0);
    d->leave->setEndValue(0.0);
    d->leave->setDuration(120);
    connect(d->leave, &QVariantAnimation::valueChanged, this, [this](const QVariant& value) {
        this->setWindowOpacity(value.toDouble());
    });
    connect(d->leave, &QVariantAnimation::finished, this, [this] {
        if (d->open) return;
        QDialog::hide();
        this->setWindowOpacity(1);
    });
}

void EmojiPanel::open() {
    d->previous = DesktopWm::activeWindow();
    d->open = true;
    d->leave->stop();
    this->setWindowOpacity(1);
    d->search->clear();
    relayout();
    d->scroll->snapTo(0.0);
    select(0, false);
    d->reveal->snapTo(0.0);
    d->reveal->animateTo(1.0);
    d->opened.start();

    QDialog::show();
    this->raise();
    QDialog::activateWindow();
    d->search->setFocus();
    DesktopWm::setSystemWindow(this, DesktopWm::SystemWindowTypeMenu);
}

void EmojiPanel::close() {
    if (!d->open) return;
    d->open = false;
    d->reveal->animateTo(0.0);
    if (Compositor::isActive() && M3Motion::expressive()) d->leave->start();
    else QDialog::hide();
}

void EmojiPanel::relayout() {
    const Data& all = emojiData();
    const QStringList words = d->search->text().trimmed().split(' ', Qt::SkipEmptyParts);
    d->cells.clear();
    d->sections.clear();

    double y = 0;
    auto section = [&](const QString& title, const QString& group, const QList<int>& items) {
        if (items.isEmpty()) return;
        d->sections.append({title, group, y});
        y += HeaderHeight;
        for (int i = 0; i < items.size(); i++) {
            d->cells.append({items.at(i), QRectF(Pad + (i % Cols) * Cell, y + (i / Cols) * Cell, Cell, Cell)});
        }
        y += ((items.size() + Cols - 1) / Cols) * Cell + 6;
    };

    if (!words.isEmpty()) {
        QList<int> found;
        for (int i = 0; i < all.items.size(); i++) {
            const QString& name = all.items.at(i).name;
            if (std::all_of(words.cbegin(), words.cend(), [&](const QString& w) {
                    return name.contains(w, Qt::CaseInsensitive);
                }))
                found.append(i);
        }
        section(tr("Results"), QString(), found);
    } else {
        QList<int> recent;
        for (const QString& text : d->settings.delimitedList(QStringLiteral("Emoji/recent"))) {
            if (all.byText.contains(text)) recent.append(all.byText.value(text));
        }
        section(tr("Recent"), QStringLiteral("Recent"), recent);
        for (int g = 0; g < all.groups.size(); g++) {
            QList<int> items;
            for (int i = 0; i < all.items.size(); i++) {
                if (all.items.at(i).group == g) items.append(i);
            }
            section(all.groups.at(g), all.groups.at(g), items);
        }
    }
    d->contentHeight = y;

    QScreen* screen = this->screen() ? this->screen() : qApp->primaryScreen();
    const QRect area = screen->geometry();
    const int top = qMax(56, int(area.height() * 0.094));
    const int height = Pad + SearchHeight + ChipsHeight + ListHeight + HintHeight;
    this->setGeometry(area.left() + (area.width() - Width) / 2, area.top() + top, Width, height);
    d->search->setGeometry(Pad, Pad, Width - 2 * Pad, SearchHeight);
    if (!Compositor::isActive()) {
        QPainterPath path;
        path.addRoundedRect(QRectF(this->rect()), 28, 28);
        this->setMask(QRegion(path.toFillPolygon().toPolygon()));
    } else {
        this->clearMask();
    }
    this->update();
}

void EmojiPanel::scrollTo(double y) {
    d->scroll->animateTo(qBound(0.0, y, qMax(0.0, d->contentHeight - ListHeight)));
}

void EmojiPanel::select(int index, bool spring) {
    if (d->cells.isEmpty()) return;
    d->selected = qBound(0, index, int(d->cells.size()) - 1);
    const QRectF rect = d->cells.at(d->selected).rect;
    if (spring) {
        d->highlight->animateTo(rect);
        d->pop->snapTo(0.0);
        d->pop->animateTo(1.0);
    } else {
        d->highlight->snapTo(rect);
        d->pop->snapTo(1.0);
    }
    const double scroll = d->scroll->endValue().toDouble();
    if (rect.top() - HeaderHeight < scroll) scrollTo(rect.top() - HeaderHeight);
    else if (rect.bottom() > scroll + ListHeight) scrollTo(rect.bottom() - ListHeight);
    this->update();
}

void EmojiPanel::type(int index) {
    if (index < 0 || index >= d->cells.size()) return;
    const QString text = emojiData().items.at(d->cells.at(index).item).text;
    QStringList recent = d->settings.delimitedList(QStringLiteral("Emoji/recent"));
    recent.removeAll(text);
    recent.prepend(text);
    d->settings.setDelimitedList(QStringLiteral("Emoji/recent"), recent.mid(0, MaxRecent));

    ClipboardHistory::instance()->putQuietly(text);
    DesktopWmWindowPtr target = d->previous;
    close();
    PasteBack::into(target);
}

int EmojiPanel::cellAt(QPointF point) const {
    const double listTop = Pad + SearchHeight + ChipsHeight;
    if (point.y() < listTop || point.y() > listTop + ListHeight) return -1;
    const QPointF at(point.x(), point.y() - listTop + d->scroll->currentValue().toDouble());
    for (int i = 0; i < d->cells.size(); i++) {
        if (d->cells.at(i).rect.contains(at)) return i;
    }
    return -1;
}

void EmojiPanel::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event)
    const M3::Scheme s = M3::currentScheme();
    const Data& all = emojiData();
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setCompositionMode(QPainter::CompositionMode_Source);
    p.fillRect(this->rect(), Qt::transparent);
    p.setCompositionMode(QPainter::CompositionMode_SourceOver);

    const double reveal = d->reveal->currentValue().toDouble();
    const double shown = Compositor::isActive() ? qMax(SearchHeight + 2.0 * Pad, this->height() * (0.55 + 0.45 * reveal)) : this->height();
    QPainterPath card;
    card.addRoundedRect(QRectF(0, 0, this->width(), shown), 28, 28);
    p.setPen(Qt::NoPen);
    p.setBrush(s.surfaceContainer);
    p.drawPath(card);
    p.setClipPath(card);

    const double scroll = d->scroll->currentValue().toDouble();
    const double listTop = Pad + SearchHeight + ChipsHeight;

    // The categories: the one in view wears a pill that slides along
    if (d->search->text().trimmed().isEmpty()) {
        const double chipTop = Pad + SearchHeight + 6;
        int active = 0;
        for (int i = 0; i < d->sections.size(); i++) {
            if (d->sections.at(i).y <= scroll + 8) active = i;
        }
        const double chipWidth = double(Width - 2 * Pad) / qMax(1, int(d->sections.size()));
        if (!qFuzzyCompare(d->chip->endValue().toDouble() + 1, active + 1.0)) d->chip->animateTo(double(active));
        const double at = d->chip->currentValue().toDouble();
        p.setBrush(s.secondaryContainer);
        p.drawRoundedRect(QRectF(Pad + at * chipWidth + 2, chipTop, chipWidth - 4, 32), 16, 16);
        p.setFont(symbols(20));
        for (int i = 0; i < d->sections.size(); i++) {
            p.setPen(i == active ? s.onSecondaryContainer : s.onSurfaceVariant);
            p.drawText(QRectF(Pad + i * chipWidth, chipTop, chipWidth, 32), Qt::AlignCenter, symbolFor(d->sections.at(i).group));
        }
    }

    p.save();
    p.setClipRect(QRectF(0, listTop, this->width(), ListHeight), Qt::IntersectClip);
    p.translate(0, listTop - scroll);

    if (d->cells.isEmpty()) {
        p.setFont(flex(14, 400));
        p.setPen(s.onSurfaceVariant);
        p.drawText(QRectF(0, scroll, this->width(), ListHeight), Qt::AlignCenter, tr("Nothing matches"));
    }
    p.setFont(flex(13, 600));
    p.setPen(s.primary);
    for (const Private::Section& section : std::as_const(d->sections)) {
        if (section.y > scroll + ListHeight || section.y + HeaderHeight < scroll) continue;
        p.drawText(QRectF(Pad + 6, section.y, this->width(), HeaderHeight), Qt::AlignVCenter | Qt::AlignLeft, section.title);
    }

    // The highlight: a cookie that glides to the chosen emoji
    if (!d->cells.isEmpty()) {
        QRectF light = d->highlight->currentValue().toRectF();
        if (light.isEmpty()) light = d->cells.at(d->selected).rect;
        p.setBrush(s.secondaryContainer);
        p.setPen(Qt::NoPen);
        p.drawPath(m3shapes::toPath(MaterialShapes::cookie9Sided(), light.adjusted(2, 2, -2, -2)));
    }

    // Rows pop in one after another as the panel opens
    const double since = d->opened.isValid() ? d->opened.elapsed() / 1000.0 : 1;
    bool arriving = false;
    const double pop = d->pop->currentValue().toDouble();
    for (int i = 0; i < d->cells.size(); i++) {
        const Private::Cell& cell = d->cells.at(i);
        if (cell.rect.bottom() < scroll || cell.rect.top() > scroll + ListHeight) continue;
        const double row = (cell.rect.top() - scroll) / Cell;
        const double t = qBound(0.0, (since - 0.025 * qBound(0.0, row, 6.0) - 0.01 * (i % Cols)) / 0.3, 1.0);
        if (t < 1) arriving = true;
        const double ease = 1 - std::pow(1 - t, 3);
        const double grow = i == d->selected ? 1 + 0.18 * pop : 1;
        p.setOpacity(ease);
        p.setFont(emojiFont(28 * (0.6 + 0.4 * ease) * grow));
        p.setPen(s.onSurface);
        p.drawText(cell.rect, Qt::AlignCenter, all.items.at(cell.item).text);
    }
    p.setOpacity(1);
    p.restore();
    if (arriving) QTimer::singleShot(16, this, qOverload<>(&QWidget::update));

    // The chosen one's name
    p.setFont(flex(12, 450));
    p.setPen(s.onSurfaceVariant);
    const QString name = d->cells.isEmpty() ? QString() : all.items.at(d->cells.at(d->selected).item).name;
    p.drawText(QRectF(Pad + 6, listTop + ListHeight, this->width() - 2 * Pad - 12, HintHeight), Qt::AlignVCenter | Qt::AlignLeft,
        name.isEmpty() ? tr("Enter types it") : tr("%1 · Enter types it").arg(name));
}

void EmojiPanel::changeEvent(QEvent* event) {
    QDialog::changeEvent(event);
    if (event->type() == QEvent::ActivationChange && !this->isActiveWindow() && !QApplication::activePopupWidget()) close();
}

void EmojiPanel::mouseMoveEvent(QMouseEvent* event) {
    const int cell = cellAt(event->position());
    if (cell >= 0 && cell != d->selected) select(cell);
}

void EmojiPanel::mousePressEvent(QMouseEvent* event) {
    if (event->button() != Qt::LeftButton) return;
    const int cell = cellAt(event->position());
    if (cell >= 0) {
        type(cell);
        return;
    }
    // A category: jump to it
    const double chipTop = Pad + SearchHeight + 6;
    if (d->search->text().trimmed().isEmpty() && event->position().y() >= chipTop && event->position().y() <= chipTop + 32 && !d->sections.isEmpty()) {
        const double chipWidth = double(Width - 2 * Pad) / d->sections.size();
        const int i = qBound(0, int((event->position().x() - Pad) / chipWidth), int(d->sections.size()) - 1);
        scrollTo(d->sections.at(i).y);
    }
}

void EmojiPanel::wheelEvent(QWheelEvent* event) {
    scrollTo(d->scroll->endValue().toDouble() - event->angleDelta().y() * 0.6);
}

bool EmojiPanel::eventFilter(QObject* watched, QEvent* event) {
    if (watched != d->search || event->type() != QEvent::KeyPress) return QDialog::eventFilter(watched, event);
    auto* key = static_cast<QKeyEvent*>(event);
    switch (key->key()) {
        case Qt::Key_Right:
            if (!d->search->text().isEmpty() && d->search->cursorPosition() < d->search->text().size()) break;
            select(d->selected + 1);
            return true;
        case Qt::Key_Left:
            if (!d->search->text().isEmpty()) break;
            select(d->selected - 1);
            return true;
        case Qt::Key_Down:
            select(d->selected + Cols);
            return true;
        case Qt::Key_Up:
            select(d->selected - Cols);
            return true;
        case Qt::Key_Return:
        case Qt::Key_Enter:
            type(d->selected);
            return true;
        case Qt::Key_Escape:
            if (d->search->text().isEmpty()) close();
            else d->search->clear();
            return true;
        default:
            break;
    }
    return QDialog::eventFilter(watched, event);
}
