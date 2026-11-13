#include "clipboardpanel.h"
#include "clipboardhistory.h"
#include "pasteback.h"

#include <QApplication>
#include <QElapsedTimer>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMenu>
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

namespace {
    constexpr int Width = 440;
    constexpr int Pad = 12;       // inside the panel
    constexpr int SearchHeight = 56;
    constexpr int Gap = 8;        // between cards
    constexpr int Inner = 14;     // inside a card
    constexpr int LineHeight = 20;
    constexpr int CaptionHeight = 16;
    constexpr int ThumbHeight = 120;
    constexpr int HintHeight = 30;
    constexpr int MaxLines = 3;

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

    QString ago(const QDateTime& when) {
        const qint64 secs = when.secsTo(QDateTime::currentDateTime());
        if (secs < 60) return ClipboardPanel::tr("Just now");
        if (secs < 3600) return ClipboardPanel::tr("%n min ago", nullptr, int(secs / 60));
        return QLocale().toString(when.time(), QLocale::ShortFormat);
    }

    QStringList linesOf(const QString& text) {
        QStringList lines = text.trimmed().split('\n');
        for (QString& line : lines) line = line.replace('\t', QStringLiteral("    ")).trimmed();
        return lines;
    }

} // namespace

struct ClipboardPanel::Private {
        QLineEdit* search;
        struct Row {
                ClipboardHistory::Entry entry;
                QRectF rect; // in the list's coordinates (before scrolling)
                QStringList lines;
                int moreLines = 0;
        };
        QList<Row> rows;
        QHash<quint64, QPixmap> thumbs;
        int selected = 0;
        DesktopWmWindowPtr previous; // where Enter pastes

        double listTop = 0, listHeight = 0, contentHeight = 0;
        M3SpringAnimation* reveal;    // the panel unrolls from the bar
        M3SpringAnimation* highlight; // the selection glides between cards
        M3SpringAnimation* scroll;
        QVariantAnimation* leave;
        QElapsedTimer opened;
        bool open = false;
};

ClipboardPanel* ClipboardPanel::instance() {
    static auto* panel = new ClipboardPanel();
    return panel;
}

void ClipboardPanel::setUp() {
    ClipboardHistory::instance(); // starts listening from login
    connect(new KeyGrab(QKeySequence(Qt::MetaModifier | Qt::Key_V), QStringLiteral("clipboard")), &KeyGrab::activated, instance(), [] {
        if (instance()->d->open) instance()->close();
        else instance()->open();
    });
}

ClipboardPanel::ClipboardPanel() :
    QDialog(nullptr) {
    d = new Private();
    this->setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    this->setAttribute(Qt::WA_TranslucentBackground);
    this->setMouseTracking(true);
    this->setWindowTitle(tr("Clipboard"));

    d->search = new QLineEdit(this);
    d->search->setProperty("m3SearchBar", true);
    d->search->setPlaceholderText(tr("Search clipboard"));
    d->search->installEventFilter(this);
    connect(d->search, &QLineEdit::textChanged, this, [this] {
        relayout();
        select(0, false);
    });

    auto spring = [this](M3Motion::Token token, const QVariant& from) {
        auto* animation = new M3SpringAnimation(this);
        animation->setToken(token);
        animation->snapTo(from);
        connect(animation, &M3SpringAnimation::valueChanged, this, [this] {
            this->update();
        });
        return animation;
    };
    d->reveal = spring(M3Motion::SpatialDefault, 0.0);
    d->highlight = spring(M3Motion::SpatialFast, QRectF());
    d->scroll = spring(M3Motion::SpatialFast, 0.0);

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

    connect(ClipboardHistory::instance(), &ClipboardHistory::changed, this, [this] {
        if (!this->isVisible()) return;
        relayout();
        select(qMin(d->selected, int(d->rows.size()) - 1), false);
    });
}

void ClipboardPanel::open() {
    d->previous = DesktopWm::activeWindow();
    d->open = true;
    d->leave->stop();
    this->setWindowOpacity(1);
    d->search->clear();
    d->thumbs.clear();
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

void ClipboardPanel::close() {
    if (!d->open) return;
    d->open = false;
    d->reveal->animateTo(0.0);
    if (Compositor::isActive() && M3Motion::expressive()) d->leave->start();
    else QDialog::hide();
}

void ClipboardPanel::relayout() {
    const QString filter = d->search->text().trimmed();
    const QFontMetrics metrics(flex(15, 400));
    const double cardWidth = Width - 2 * Pad;
    const double textWidth = cardWidth - 2 * Inner;

    d->rows.clear();
    double y = 0;
    for (const ClipboardHistory::Entry& entry : ClipboardHistory::instance()->entries()) {
        const bool image = !entry.image.isNull();
        if (!filter.isEmpty()) {
            if (image ? !tr("Image").contains(filter, Qt::CaseInsensitive) : (entry.secret || !entry.text.contains(filter, Qt::CaseInsensitive))) continue;
        }
        Private::Row row{entry, {}, {}, 0};
        double inner;
        if (image) {
            if (!d->thumbs.contains(entry.id)) {
                const QImage scaled = entry.image.scaled(QSize(int(textWidth), ThumbHeight) * this->devicePixelRatioF(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
                QPixmap thumb = QPixmap::fromImage(scaled);
                thumb.setDevicePixelRatio(this->devicePixelRatioF());
                d->thumbs.insert(entry.id, thumb);
            }
            inner = d->thumbs.value(entry.id).deviceIndependentSize().height();
        } else {
            const QStringList lines = entry.secret ? QStringList{QString(qMin(16, int(entry.text.trimmed().size())), QChar(0x2022))} : linesOf(entry.text);
            for (const QString& line : lines.first(qMin(MaxLines, int(lines.size())))) row.lines.append(metrics.elidedText(line, Qt::ElideRight, int(textWidth)));
            row.moreLines = qMax(0, int(lines.size()) - MaxLines);
            inner = row.lines.size() * LineHeight;
        }
        const double height = Inner + inner + 6 + CaptionHeight + Inner;
        row.rect = QRectF(Pad, y, cardWidth, height);
        y += height + Gap;
        d->rows.append(row);
    }
    d->contentHeight = qMax(0.0, y - Gap);

    // Under the bar, in the middle; as tall as its cards, up to most of the screen
    QScreen* screen = this->screen() ? this->screen() : qApp->primaryScreen();
    const QRect area = screen->geometry();
    const int top = qMax(56, int(area.height() * 0.094));
    d->listTop = Pad + SearchHeight + Gap;
    const double empty = 96;
    const double maxList = area.height() - top - 24 - d->listTop - HintHeight;
    d->listHeight = qMin(maxList, d->rows.isEmpty() ? empty : d->contentHeight);
    const int height = int(d->listTop + d->listHeight + HintHeight);
    this->setGeometry(area.left() + (area.width() - Width) / 2, area.top() + top, Width, height);
    d->search->setGeometry(Pad, Pad, Width - 2 * Pad, SearchHeight);

    // Without a compositor nothing is see-through: cut the window to the card
    if (!Compositor::isActive()) {
        QPainterPath path;
        path.addRoundedRect(QRectF(this->rect()), 28, 28);
        this->setMask(QRegion(path.toFillPolygon().toPolygon()));
    } else {
        this->clearMask();
    }
    this->update();
}

void ClipboardPanel::select(int index, bool spring) {
    if (d->rows.isEmpty()) {
        d->selected = 0;
        return;
    }
    d->selected = qBound(0, index, int(d->rows.size()) - 1);
    const QRectF rect = d->rows.at(d->selected).rect;
    if (spring) d->highlight->animateTo(rect);
    else d->highlight->snapTo(rect);

    // Keep it in view
    const double scroll = d->scroll->endValue().toDouble();
    const double maxScroll = qMax(0.0, d->contentHeight - d->listHeight);
    double next = scroll;
    if (rect.top() < scroll) next = rect.top();
    if (rect.bottom() > scroll + d->listHeight) next = rect.bottom() - d->listHeight;
    next = qBound(0.0, next, maxScroll);
    if (!qFuzzyCompare(next + 1, scroll + 1)) {
        if (spring) d->scroll->animateTo(next);
        else d->scroll->snapTo(next);
    }
    this->update();
}

void ClipboardPanel::activate(int index, bool paste) {
    if (index < 0 || index >= d->rows.size()) return;
    ClipboardHistory::instance()->use(d->rows.at(index).entry.id);
    DesktopWmWindowPtr target = d->previous;
    close();
    if (paste) PasteBack::into(target);
}

int ClipboardPanel::rowAt(QPointF point) const {
    const double y = point.y() - d->listTop + d->scroll->currentValue().toDouble();
    if (point.y() < d->listTop || point.y() > d->listTop + d->listHeight) return -1;
    for (int i = 0; i < d->rows.size(); i++) {
        if (d->rows.at(i).rect.contains(QPointF(point.x(), y))) return i;
    }
    return -1;
}

void ClipboardPanel::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event)
    const M3::Scheme s = M3::currentScheme();
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setCompositionMode(QPainter::CompositionMode_Source);
    p.fillRect(this->rect(), Qt::transparent);
    p.setCompositionMode(QPainter::CompositionMode_SourceOver);

    // The panel unrolls from its top edge (only the movement overshoots)
    const double reveal = d->reveal->currentValue().toDouble();
    const double shown = Compositor::isActive() ? qMax(SearchHeight + 2.0 * Pad, this->height() * (0.55 + 0.45 * reveal)) : this->height();
    QPainterPath card;
    card.addRoundedRect(QRectF(0, 0, this->width(), shown), 28, 28);
    p.setPen(Qt::NoPen);
    p.setBrush(s.surfaceContainer);
    p.drawPath(card);
    p.setClipPath(card);

    // The cards
    p.save();
    p.setClipRect(QRectF(0, d->listTop, this->width(), d->listHeight), Qt::IntersectClip);
    p.translate(0, d->listTop - d->scroll->currentValue().toDouble());

    if (d->rows.isEmpty()) {
        p.setFont(flex(14, 400));
        p.setPen(s.onSurfaceVariant);
        const QString text = d->search->text().isEmpty() ? tr("Copy something and it shows up here") : tr("Nothing matches");
        p.drawText(QRectF(0, 0, this->width(), 96), Qt::AlignCenter, text);
    }

    // Cards arrive one after another as the panel opens
    const double since = d->opened.isValid() ? d->opened.elapsed() / 1000.0 : 1;
    bool arriving = false;
    for (int i = 0; i < d->rows.size(); i++) {
        const Private::Row& row = d->rows.at(i);
        const double t = qBound(0.0, (since - 0.03 * qMin(i, 8)) / 0.28, 1.0);
        if (t < 1) arriving = true;
        const double ease = 1 - std::pow(1 - t, 3);
        const QRectF rect = row.rect.translated(0, (1 - ease) * 18);
        p.setOpacity(ease);

        p.setPen(Qt::NoPen);
        p.setBrush(s.surfaceContainerHighest);
        p.drawRoundedRect(rect, 16, 16);
        if (i == d->selected) {
            QRectF light = d->highlight->currentValue().toRectF();
            if (light.isEmpty()) light = rect;
            p.save();
            QPainterPath clip;
            clip.addRoundedRect(rect, 16, 16);
            p.setClipPath(clip, Qt::IntersectClip);
            p.setBrush(s.secondaryContainer);
            p.drawRoundedRect(light.united(rect).intersected(rect), 16, 16);
            p.restore();
        }
        const QColor ink = i == d->selected ? s.onSecondaryContainer : s.onSurface;
        const QColor soft = i == d->selected ? s.onSecondaryContainer : s.onSurfaceVariant;

        double y = rect.top() + Inner;
        QString caption;
        if (!row.entry.image.isNull()) {
            const QPixmap& thumb = d->thumbs.value(row.entry.id);
            const QSizeF size = thumb.deviceIndependentSize();
            QPainterPath frame;
            frame.addRoundedRect(QRectF(rect.left() + Inner, y, size.width(), size.height()), 10, 10);
            p.save();
            p.setClipPath(frame, Qt::IntersectClip);
            p.drawPixmap(QPointF(rect.left() + Inner, y), thumb);
            p.restore();
            y += size.height();
            caption = tr("Image · %1 × %2").arg(row.entry.image.width()).arg(row.entry.image.height());
        } else {
            p.setFont(flex(15, 400));
            p.setPen(ink);
            for (const QString& line : row.lines) {
                p.drawText(QRectF(rect.left() + Inner, y, rect.width() - 2 * Inner, LineHeight), Qt::AlignVCenter | Qt::AlignLeft, line);
                y += LineHeight;
            }
            caption = row.entry.secret ? tr("Looks like a password") + QStringLiteral(" · ") + ago(row.entry.when) : row.moreLines > 0 ? tr("%n more lines", nullptr, row.moreLines) + QStringLiteral(" · ") + ago(row.entry.when) : ago(row.entry.when);
        }
        y += 6;
        p.setFont(flex(12, 500));
        p.setPen(soft);
        p.drawText(QRectF(rect.left() + Inner, y, rect.width() - 2 * Inner - 22, CaptionHeight), Qt::AlignVCenter | Qt::AlignLeft, caption);
        if (row.entry.pinned) {
            p.setFont(symbols(16));
            p.setPen(i == d->selected ? s.onSecondaryContainer : s.primary);
            p.drawText(QRectF(rect.right() - Inner - 18, y - 1, 18, 18), Qt::AlignCenter, QStringLiteral("keep"));
        }
    }
    p.setOpacity(1);
    p.restore();
    if (arriving) QTimer::singleShot(16, this, qOverload<>(&QWidget::update));

    // What the keys do
    p.setFont(flex(12, 450));
    p.setPen(s.onSurfaceVariant);
    p.drawText(QRectF(Pad + 6, d->listTop + d->listHeight, this->width() - 2 * Pad - 12, HintHeight), Qt::AlignVCenter | Qt::AlignLeft, tr("Enter paste · Shift+Enter copy · Ctrl+P pin · Del remove"));
}

void ClipboardPanel::changeEvent(QEvent* event) {
    QDialog::changeEvent(event);
    if (event->type() == QEvent::ActivationChange && !this->isActiveWindow() && !QApplication::activePopupWidget()) close();
}

void ClipboardPanel::mouseMoveEvent(QMouseEvent* event) {
    const int row = rowAt(event->position());
    if (row >= 0 && row != d->selected) select(row);
}

void ClipboardPanel::mousePressEvent(QMouseEvent* event) {
    if (event->button() != Qt::LeftButton) return;
    const int row = rowAt(event->position());
    if (row >= 0) activate(row, true);
}

void ClipboardPanel::wheelEvent(QWheelEvent* event) {
    const double maxScroll = qMax(0.0, d->contentHeight - d->listHeight);
    const double next = qBound(0.0, d->scroll->endValue().toDouble() - event->angleDelta().y() * 0.6, maxScroll);
    d->scroll->animateTo(next);
}

void ClipboardPanel::contextMenuEvent(QContextMenuEvent* event) {
    const int row = rowAt(event->pos());
    if (row < 0) return;
    select(row);
    const ClipboardHistory::Entry entry = d->rows.at(row).entry;
    QMenu menu(this);
    menu.addAction(QIcon::fromTheme(QStringLiteral("edit-paste")), tr("Paste"), this, [this, row] {
        activate(row, true);
    });
    menu.addAction(QIcon::fromTheme(QStringLiteral("edit-copy")), tr("Copy"), this, [this, row] {
        activate(row, false);
    });
    menu.addAction(QIcon::fromTheme(QStringLiteral("pin")), entry.pinned ? tr("Unpin") : tr("Pin"), this, [entry] {
        ClipboardHistory::instance()->togglePin(entry.id);
    });
    menu.addAction(QIcon::fromTheme(QStringLiteral("edit-delete")), tr("Remove"), this, [entry] {
        ClipboardHistory::instance()->remove(entry.id);
    });
    menu.addSeparator();
    menu.addAction(QIcon::fromTheme(QStringLiteral("edit-clear-history")), tr("Clear history (keeps pins)"), this, [] {
        ClipboardHistory::instance()->clear();
    });
    menu.exec(event->globalPos());
}

bool ClipboardPanel::eventFilter(QObject* watched, QEvent* event) {
    if (watched != d->search || event->type() != QEvent::KeyPress) return QDialog::eventFilter(watched, event);
    auto* key = static_cast<QKeyEvent*>(event);
    switch (key->key()) {
        case Qt::Key_Down:
            select(d->selected + 1);
            return true;
        case Qt::Key_Up:
            select(d->selected - 1);
            return true;
        case Qt::Key_Return:
        case Qt::Key_Enter:
            activate(d->selected, !(key->modifiers() & Qt::ShiftModifier));
            return true;
        case Qt::Key_Delete:
            if (!d->search->text().isEmpty() && d->search->cursorPosition() < d->search->text().size()) break;
            if (d->selected < d->rows.size()) ClipboardHistory::instance()->remove(d->rows.at(d->selected).entry.id);
            return true;
        case Qt::Key_P:
            if (!(key->modifiers() & Qt::ControlModifier)) break;
            if (d->selected < d->rows.size()) ClipboardHistory::instance()->togglePin(d->rows.at(d->selected).entry.id);
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
