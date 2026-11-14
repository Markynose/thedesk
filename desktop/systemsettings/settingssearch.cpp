/****************************************
 *
 *   theDesk - System Settings search
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 * *************************************/
#include "settingssearch.h"

#include <QAbstractButton>
#include <QApplication>
#include <QComboBox>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPainter>
#include <QPainterPath>
#include <QPointer>
#include <QStyledItemDelegate>
#include <QTextDocumentFragment>
#include <QToolButton>
#include <QVBoxLayout>
#include <m3motion.h>
#include <m3scheme.h>
#include <statuscenterpane.h>

namespace {
    constexpr int BarHeight = 56, RowHeight = 64, Width = 480, Shown = 6;
    enum Role {
        CrumbRole = Qt::UserRole,
        EntryRole
    };

    QString textOf(QObject* object) {
        QString text;
        if (auto* label = qobject_cast<QLabel*>(object)) text = Qt::mightBeRichText(label->text()) ? QTextDocumentFragment::fromHtml(label->text()).toPlainText() : label->text();
        else if (auto* combo = qobject_cast<QComboBox*>(object)) text = combo->currentText();
        else if (auto* button = qobject_cast<QAbstractButton*>(object)) text = button->text();
        text.remove(QLatin1Char('&'));
        return text.simplified();
    }

    // `text` with the part matching `query` in bold Primary
    void drawMatched(QPainter* p, const QRectF& r, const QString& text, const QString& query, QFont font, QColor ink, QColor hit) {
        const QString shown = QFontMetrics(font).elidedText(text, Qt::ElideRight, int(r.width()));
        const int at = query.isEmpty() ? -1 : shown.indexOf(query, 0, Qt::CaseInsensitive);
        if (at < 0) {
            p->setFont(font);
            p->setPen(ink);
            p->drawText(r, Qt::AlignLeft | Qt::AlignVCenter, shown);
            return;
        }
        QFont bold = font;
        bold.setWeight(QFont::Bold);
        double x = r.left();
        const QString parts[3] = {shown.left(at), shown.mid(at, query.size()), shown.mid(at + query.size())};
        for (int i = 0; i < 3; i++) {
            const QFont& f = i == 1 ? bold : font;
            p->setFont(f);
            p->setPen(i == 1 ? hit : ink);
            p->drawText(QRectF(x, r.top(), r.right() - x, r.height()), Qt::AlignLeft | Qt::AlignVCenter, parts[i]);
            x += QFontMetricsF(f).horizontalAdvance(parts[i]);
        }
    }

    // A result: the pane's symbol in a tonal circle, the setting, where it lives
    class ResultDelegate : public QStyledItemDelegate {
        public:
            using QStyledItemDelegate::QStyledItemDelegate;
            QString query;

            QSize sizeHint(const QStyleOptionViewItem&, const QModelIndex&) const override {
                return QSize(Width - 32, RowHeight);
            }

            void paint(QPainter* p, const QStyleOptionViewItem& option, const QModelIndex& index) const override {
                const M3::Scheme s = M3::currentScheme();
                const QRectF r = QRectF(option.rect).adjusted(0, 2, 0, -2);
                p->save();
                p->setRenderHint(QPainter::Antialiasing);
                if (option.state & (QStyle::State_Selected | QStyle::State_MouseOver)) {
                    QColor layer = s.onSurface;
                    layer.setAlphaF(option.state & QStyle::State_Selected ? 0.10 : 0.08);
                    QPainterPath shape;
                    shape.addRoundedRect(r, 16, 16);
                    p->fillPath(shape, layer);
                }
                const QPointF centre(r.left() + 28, r.center().y());
                p->setPen(Qt::NoPen);
                p->setBrush(s.secondaryContainer);
                p->drawEllipse(centre, 18, 18);
                index.data(Qt::DecorationRole).value<QIcon>().paint(p, QRect(int(centre.x()) - 10, int(centre.y()) - 10, 20, 20));
                QFont title = option.font;
                title.setPixelSize(16);
                QFont crumb = option.font;
                crumb.setPixelSize(13);
                const QRectF text(r.left() + 58, r.top() + 6, r.width() - 70, 24);
                drawMatched(p, text, index.data(Qt::DisplayRole).toString(), query, title, s.onSurface, s.primary);
                drawMatched(p, text.translated(0, 24).adjusted(0, 0, 0, -4), index.data(CrumbRole).toString(), query, crumb, s.onSurfaceVariant, s.primary);
                p->restore();
            }
    };

    struct Entry {
            QPointer<StatusCenterPane> pane;
            QPointer<QWidget> target;
            QString title, crumb;
    };
} // namespace

struct SettingsSearchPrivate {
        std::function<QList<StatusCenterPane*>()> panes;
        QPointer<QWidget> from;
        QLineEdit* field;
        QListWidget* results;
        ResultDelegate* delegate;
        M3SpringAnimation* geometry;
        QList<Entry> entries;
        bool closing = false;
};

SettingsSearch::SettingsSearch(QWidget* window, std::function<QList<StatusCenterPane*>()> panes) :
    QWidget(window) {
    d = new SettingsSearchPrivate();
    d->panes = std::move(panes);
    this->hide();

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 0, 8, 8);
    layout->setSpacing(0);
    auto* bar = new QHBoxLayout();
    bar->setContentsMargins(0, 0, 8, 0);
    auto* back = new QToolButton(this);
    back->setAutoRaise(true);
    back->setIcon(QIcon::fromTheme(QStringLiteral("go-previous")));
    back->setToolTip(tr("Close the search"));
    connect(back, &QToolButton::clicked, this, &SettingsSearch::close);
    d->field = new QLineEdit(this);
    d->field->setFrame(false);
    d->field->setPlaceholderText(tr("Search settings"));
    QFont font = d->field->font();
    font.setPixelSize(16);
    d->field->setFont(font);
    d->field->setFixedHeight(BarHeight);
    d->field->installEventFilter(this);
    connect(d->field, &QLineEdit::textChanged, this, &SettingsSearch::search);
    bar->addWidget(back);
    bar->addWidget(d->field, 1);
    layout->addLayout(bar);

    d->results = new QListWidget(this);
    d->results->setFrameShape(QFrame::NoFrame);
    d->results->setProperty("m3Segmented", false);
    d->results->viewport()->setAutoFillBackground(false);
    d->results->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    d->results->setMouseTracking(true);
    d->results->installEventFilter(this);
    d->delegate = new ResultDelegate(d->results);
    d->results->setItemDelegate(d->delegate);
    d->results->hide();
    connect(d->results, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
        choose(d->results->row(item));
    });
    layout->addWidget(d->results, 1);

    d->geometry = new M3SpringAnimation(this);
    d->geometry->setToken(M3Motion::SpatialDefault);
    connect(d->geometry, &M3SpringAnimation::valueChanged, this, [this](QVariant value) {
        this->setGeometry(value.toRect());
    });
    connect(d->geometry, &QAbstractAnimation::finished, this, [this] {
        if (d->closing) this->hide();
    });
}

SettingsSearch::~SettingsSearch() {
    delete d;
}

void SettingsSearch::open(QWidget* from) {
    d->from = from;
    d->closing = false;
    // Polish every pane now, so its groups have published their rows by the time anyone types
    for (StatusCenterPane* pane : d->panes()) pane->ensurePolished();
    const QRect start(from->mapTo(this->parentWidget(), QPoint(0, 0)), from->size());
    if (!this->isVisible()) d->geometry->snapTo(start);
    this->setGeometry(start);
    this->show();
    this->raise();
    d->field->setFocus();
    qApp->installEventFilter(this);
    follow();
}

void SettingsSearch::close() {
    if (!d->from || d->closing) return;
    d->closing = true;
    qApp->removeEventFilter(this);
    d->field->clear();
    d->geometry->animateTo(QRect(d->from->mapTo(this->parentWidget(), QPoint(0, 0)), d->from->size()));
}

void SettingsSearch::follow() {
    // The bar alone, or the bar over its results; never past the window
    if (!d->from) return;
    const QPoint at = d->from->mapTo(this->parentWidget(), QPoint(0, 0));
    const int rows = qMin(Shown, d->results->count());
    int height = BarHeight + (rows ? rows * RowHeight + 16 : 0);
    height = qMin(height, this->parentWidget()->height() - at.y() - 16);
    d->geometry->animateTo(QRect(at, QSize(Width, height)));
}

void SettingsSearch::search(const QString& query) {
    d->results->clear();
    d->entries.clear();
    d->delegate->query = query.trimmed();
    const QString q = query.trimmed();
    if (!q.isEmpty()) {
        // Every pane, then every row its groups published
        QList<QPair<int, Entry>> found;
        for (StatusCenterPane* pane : d->panes()) {
            const QString paneName = pane->displayName();
            if (paneName.contains(q, Qt::CaseInsensitive)) found.append({paneName.startsWith(q, Qt::CaseInsensitive) ? 0 : 3, {pane, nullptr, paneName, tr("System Settings")}});
            for (QWidget* group : pane->findChildren<QWidget*>()) {
                if (!group->property("m3SettingsGroup").toBool()) continue;
                for (const QVariant& tile : group->property("m3Tiles").toList()) {
                    const QVariantMap row = tile.toMap();
                    auto* title = qobject_cast<QWidget*>(row.value(QStringLiteral("title")).value<QObject*>());
                    if (!title || !title->isVisibleTo(pane)) continue;
                    const QString name = textOf(title);
                    const QString section = textOf(row.value(QStringLiteral("section")).value<QObject*>());
                    QString value = textOf(row.value(QStringLiteral("value")).value<QObject*>());
                    if (name.isEmpty() || value == name) value.clear();
                    QString crumb = section.isEmpty() ? paneName : tr("%1 › %2").arg(paneName, section);
                    if (!value.isEmpty()) crumb = tr("%1 · %2").arg(crumb, value);
                    // a menu's other choices find it too ("hibernate" finds the power button)
                    QStringList choices;
                    if (auto* combo = qobject_cast<QComboBox*>(row.value(QStringLiteral("control")).value<QObject*>())) {
                        for (int i = 0; i < combo->count(); i++) choices.append(combo->itemText(i));
                    }
                    int rank;
                    if (name.startsWith(q, Qt::CaseInsensitive)) rank = 1;
                    else if (name.contains(q, Qt::CaseInsensitive)) rank = 2;
                    else if (crumb.contains(q, Qt::CaseInsensitive)) rank = 4;
                    else if (choices.join(QLatin1Char('\n')).contains(q, Qt::CaseInsensitive)) rank = 5;
                    else continue;
                    found.append({rank, {pane, title, name, crumb}});
                }
            }
        }
        std::stable_sort(found.begin(), found.end(), [](const auto& a, const auto& b) {
            return a.first < b.first;
        });
        for (const auto& f : std::as_const(found)) {
            if (d->entries.size() >= 30) break;
            auto* item = new QListWidgetItem(f.second.title);
            item->setData(CrumbRole, f.second.crumb);
            item->setData(Qt::DecorationRole, f.second.pane->icon());
            d->results->addItem(item);
            d->entries.append(f.second);
        }
        if (d->results->count()) d->results->setCurrentRow(0);
    }
    d->results->setVisible(d->results->count() > 0);
    this->update();
    follow();
}

void SettingsSearch::choose(int row) {
    if (row < 0 || row >= d->entries.size()) return;
    const Entry entry = d->entries.at(row);
    close();
    if (entry.pane) emit chosen(entry.pane, entry.target);
}

void SettingsSearch::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event)
    const M3::Scheme s = M3::currentScheme();
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QRectF r = QRectF(this->rect());
    const double radius = qMin(28.0, r.height() / 2);
    QPainterPath shape;
    shape.addRoundedRect(r, radius, radius);
    p.fillPath(shape, s.surfaceContainerHigh);
    if (d->results->isVisible()) p.fillRect(QRectF(0, BarHeight - 1, r.width(), 1), s.outlineVariant);
}

bool SettingsSearch::eventFilter(QObject* watched, QEvent* event) {
    if (event->type() == QEvent::MouseButtonPress && this->isVisible() && !d->closing) {
        // A click anywhere else closes it
        auto* w = qobject_cast<QWidget*>(watched);
        if (w && w != this && !this->isAncestorOf(w) && w->window() == this->window()) close();
        return false;
    }
    if ((watched == d->field || watched == d->results) && event->type() == QEvent::KeyPress) {
        auto* key = static_cast<QKeyEvent*>(event);
        switch (key->key()) {
            case Qt::Key_Escape:
                close();
                return true;
            case Qt::Key_Down:
            case Qt::Key_Up:
                if (watched != d->field || !d->results->count()) return false;
                d->results->setCurrentRow(qBound(0, d->results->currentRow() + (key->key() == Qt::Key_Down ? 1 : -1), d->results->count() - 1));
                return true;
            case Qt::Key_Return:
            case Qt::Key_Enter:
                choose(qMax(0, d->results->currentRow()));
                return true;
            default:
                return false;
        }
    }
    return false;
}
