/****************************************
 *
 *   theDesk - Material Symbols icon engine
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 * *************************************/
#include "materialsymbolengine.h"

#include <QFontDatabase>
#include <QGlyphRun>
#include <QGuiApplication>
#include <QHash>
#include <QMutex>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QTextLayout>
#include <tsettings.h>

namespace {
    const QString family = QStringLiteral("Material Symbols Rounded");

    // Freedesktop (Icon Naming Specification and common extras) -> Material Symbols
    const QHash<QString, QString>& table() {
        static const QHash<QString, QString> map = {
            // Navigation
            {"go-previous", "arrow_back"}, {"go-next", "arrow_forward"}, {"go-up", "arrow_upward"}, {"go-down", "arrow_downward"},
            {"go-home", "home"}, {"go-first", "first_page"}, {"go-last", "last_page"}, {"go-top", "vertical_align_top"},
            {"go-bottom", "vertical_align_bottom"}, {"go-jump", "redo"},
            {"arrow-left", "chevron_left"}, {"arrow-right", "chevron_right"}, {"arrow-up", "expand_less"}, {"arrow-down", "expand_more"},
            {"pan-start", "chevron_left"}, {"pan-end", "chevron_right"}, {"pan-up", "expand_less"}, {"pan-down", "expand_more"},
            // Editing
            {"edit-copy", "content_copy"}, {"edit-paste", "content_paste"}, {"edit-cut", "content_cut"}, {"edit-delete", "delete"},
            {"edit-undo", "undo"}, {"edit-redo", "redo"}, {"edit-find", "search"}, {"edit-find-replace", "find_replace"},
            {"edit-clear", "close"}, {"edit-clear-all", "clear_all"}, {"edit-select-all", "select_all"}, {"edit-rename", "edit"},
            {"document-edit", "edit"}, {"edit", "edit"},
            // Documents
            {"document-new", "note_add"}, {"document-open", "folder_open"}, {"document-open-recent", "history"},
            {"document-save", "save"}, {"document-save-as", "save_as"}, {"document-print", "print"},
            {"document-print-preview", "preview"}, {"document-properties", "info"}, {"document-revert", "restore"},
            {"document-send", "send"}, {"document-share", "share"},
            // Lists, windows, dialogs
            {"list-add", "add"}, {"list-remove", "remove"}, {"window-close", "close"}, {"window-new", "open_in_new"},
            {"window-minimize", "minimize"}, {"window-maximize", "open_in_full"}, {"window-restore", "close_fullscreen"},
            {"application-exit", "logout"}, {"dialog-ok", "check"}, {"dialog-ok-apply", "check"}, {"dialog-apply", "check"},
            {"dialog-cancel", "close"}, {"dialog-close", "close"}, {"dialog-information", "info"}, {"dialog-warning", "warning"},
            {"dialog-error", "error"}, {"dialog-question", "help"}, {"dialog-password", "password"},
            {"help-about", "info"}, {"help-contents", "help"}, {"help-faq", "help"}, {"process-stop", "block"},
            // View
            {"view-refresh", "refresh"}, {"view-list-details", "list"}, {"view-list-icons", "grid_view"}, {"view-grid", "grid_view"},
            {"view-fullscreen", "fullscreen"}, {"view-restore", "fullscreen_exit"}, {"view-hidden", "visibility_off"},
            {"view-visible", "visibility"}, {"view-sort", "sort"}, {"view-sort-ascending", "arrow_upward"},
            {"view-sort-descending", "arrow_downward"}, {"view-filter", "filter_list"}, {"view-more", "more_horiz"},
            {"zoom-in", "zoom_in"}, {"zoom-out", "zoom_out"}, {"zoom-original", "fit_screen"}, {"zoom-fit-best", "fit_screen"},
            // Menus and settings
            {"application-menu", "menu"}, {"open-menu", "menu"}, {"overflow-menu", "more_vert"}, {"view-more-horizontal", "more_horiz"},
            {"configure", "settings"}, {"preferences-system", "settings"}, {"settings-configure", "settings"},
            {"preferences-other", "tune"}, {"preferences-desktop", "desktop_windows"}, {"emblem-system", "settings"},
            // Media
            {"media-playback-start", "play_arrow"}, {"media-playback-pause", "pause"}, {"media-playback-stop", "stop"},
            {"media-skip-forward", "skip_next"}, {"media-skip-backward", "skip_previous"}, {"media-seek-forward", "fast_forward"},
            {"media-seek-backward", "fast_rewind"}, {"media-record", "fiber_manual_record"}, {"media-eject", "eject"},
            {"media-playlist-shuffle", "shuffle"}, {"media-playlist-repeat", "repeat"}, {"media-playlist-repeat-song", "repeat_one"},
            {"audio-volume-high", "volume_up"}, {"audio-volume-medium", "volume_down"}, {"audio-volume-low", "volume_mute"},
            {"audio-volume-muted", "volume_off"}, {"audio-speakers", "speaker"}, {"audio-headphones", "headphones"},
            {"audio-input-microphone", "mic"}, {"microphone-sensitivity-muted", "mic_off"}, {"camera-photo", "photo_camera"},
            {"camera-web", "videocam"}, {"video-display", "monitor"},
            // Network and radios
            {"network-wireless", "wifi"}, {"network-wireless-signal-excellent", "wifi"}, {"network-wireless-signal-good", "network_wifi_3_bar"},
            {"network-wireless-signal-ok", "network_wifi_2_bar"}, {"network-wireless-signal-weak", "network_wifi_1_bar"},
            {"network-wireless-signal-none", "signal_wifi_0_bar"}, {"network-wireless-offline", "wifi_off"},
            {"network-wired", "lan"}, {"network-offline", "cloud_off"}, {"network-cellular", "signal_cellular_alt"},
            {"network-vpn", "vpn_key"}, {"bluetooth", "bluetooth"}, {"bluetooth-active", "bluetooth"},
            {"bluetooth-disabled", "bluetooth_disabled"}, {"airplane-mode", "flight"}, {"preferences-system-network", "lan"},
            // theDesk's own network names (NetworkPluginCommon): signal in quarters
            {"network-wireless-connected-00", "signal_wifi_0_bar"}, {"network-wireless-connected-25", "network_wifi_1_bar"},
            {"network-wireless-connected-50", "network_wifi_2_bar"}, {"network-wireless-connected-75", "network_wifi_3_bar"},
            {"network-wireless-connected-100", "wifi"}, {"network-wireless-error-00", "signal_wifi_bad"},
            {"network-wireless-error-25", "signal_wifi_bad"}, {"network-wireless-error-50", "signal_wifi_bad"},
            {"network-wireless-error-75", "signal_wifi_bad"}, {"network-wireless-error-100", "signal_wifi_bad"},
            {"network-wireless-disconnected", "wifi_off"}, {"network-wireless-tethered", "wifi_tethering"}, {"network-tethering", "wifi_tethering"},
            {"network-cellular-connected-00", "signal_cellular_0_bar"}, {"network-cellular-connected-25", "signal_cellular_1_bar"},
            {"network-cellular-connected-50", "signal_cellular_2_bar"}, {"network-cellular-connected-75", "signal_cellular_3_bar"},
            {"network-cellular-connected-100", "signal_cellular_4_bar"}, {"network-cellular-error-00", "signal_cellular_nodata"},
            {"network-cellular-error-25", "signal_cellular_nodata"}, {"network-cellular-error-50", "signal_cellular_nodata"},
            {"network-cellular-error-75", "signal_cellular_nodata"}, {"network-cellular-error-100", "signal_cellular_nodata"},
            {"network-cellular-disconnected", "signal_cellular_off"}, {"network-cellular-activated", "signal_cellular_alt"},
            {"network-wired-activated", "lan"}, {"network-wired-error", "lan"}, {"network-wired-unavailable", "lan"},
            {"network-disconnect", "link_off"}, {"network-modem", "router"},
            // Power and session
            {"battery", "battery_full"}, {"battery-full", "battery_full"}, {"battery-good", "battery_5_bar"},
            {"battery-low", "battery_2_bar"}, {"battery-caution", "battery_1_bar"}, {"battery-empty", "battery_0_bar"},
            {"battery-full-charging", "battery_charging_full"}, {"ac-adapter", "power"},
            // UPower levels as libtdesktopenvironment names them
            {"battery-020", "battery_2_bar"}, {"battery-040", "battery_3_bar"}, {"battery-060", "battery_4_bar"},
            {"battery-080", "battery_6_bar"}, {"battery-100", "battery_full"}, {"battery-charging-empty", "battery_charging_20"},
            {"battery-charging-020", "battery_charging_20"}, {"battery-charging-040", "battery_charging_50"},
            {"battery-charging-060", "battery_charging_60"}, {"battery-charging-080", "battery_charging_80"},
            {"battery-charging-100", "battery_charging_full"}, {"battery-full-charged", "battery_charging_full"},
            {"battery-stretch", "energy_savings_leaf"}, {"quiet-mode", "notifications_off"}, {"quiet-mode-critical-only", "notification_important"},
            {"power-profile-power-saver", "energy_savings_leaf"}, {"battery-profile-powersave", "energy_savings_leaf"},
            {"system-shutdown", "power_settings_new"}, {"system-reboot", "restart_alt"}, {"system-suspend", "bedtime"},
            {"system-hibernate", "ac_unit"}, {"system-suspend-hibernate", "ac_unit"}, {"system-lock-screen", "lock"}, {"system-log-out", "logout"},
            {"system-search", "search"}, {"system-run", "terminal"}, {"system-switch-user", "switch_account"},
            // People, places, things
            {"user", "person"}, {"user-identity", "person"}, {"avatar-default", "account_circle"},
            {"folder", "folder"}, {"folder-open", "folder_open"}, {"user-home", "home"}, {"user-trash", "delete"},
            {"user-trash-full", "delete"}, {"user-desktop", "desktop_windows"}, {"folder-download", "download"},
            {"folder-documents", "description"}, {"folder-pictures", "image"}, {"folder-music", "music_note"},
            {"folder-videos", "movie"}, {"drive-harddisk", "hard_drive"}, {"drive-removable-media", "usb"},
            {"computer", "computer"}, {"input-keyboard", "keyboard"}, {"input-mouse", "mouse"}, {"input-touchpad", "touchpad_mouse"},
            {"printer", "print"}, {"phone", "smartphone"}, {"mail-send", "send"}, {"mail-message", "mail"},
            {"mail-unread", "mark_email_unread"}, {"call-start", "call"}, {"call-stop", "call_end"}, {"bookmark-new", "bookmark_add"},
            {"bookmarks", "bookmarks"}, {"starred", "star"}, {"non-starred", "star"}, {"emblem-favorite", "favorite"},
            {"tag", "sell"}, {"globe", "public"}, {"applications-internet", "public"},
            // Status and notifications
            {"preferences-desktop-notification", "notifications"}, {"thedesk-notifications", "notifications"}, {"preferences-system-notifications", "notifications"},
            {"notifications", "notifications"}, {"notifications-disabled", "notifications_off"},
            {"night-light", "nightlight"}, {"weather-clear-night", "bedtime"}, {"weather-clear", "light_mode"},
            {"weather-few-clouds", "partly_cloudy_day"}, {"weather-overcast", "cloud"}, {"weather-showers", "rainy"},
            {"weather-snow", "weather_snowy"}, {"weather-storm", "thunderstorm"}, {"chronometer", "timer"},
            {"clock", "schedule"}, {"x-office-calendar", "calendar_today"}, {"appointment-new", "event"},
            {"security-high", "verified_user"}, {"security-low", "gpp_maybe"}, {"dialog-security", "shield"},
            {"caffeine-cup-full", "coffee"}, {"my-caffeine-on", "coffee"}, {"display-brightness", "brightness_6"},
            {"keyboard-brightness", "keyboard"}, {"preferences-desktop-display", "monitor"}, {"preferences-desktop-theme", "palette"},
            {"preferences-desktop-locale", "language"}, {"preferences-desktop-keyboard", "keyboard"},
            {"preferences-system-time", "schedule"}, {"preferences-desktop-accessibility", "accessibility_new"},
            // Text formatting
            {"format-text-bold", "format_bold"}, {"format-text-italic", "format_italic"}, {"format-text-underline", "format_underlined"},
            {"format-text-strikethrough", "format_strikethrough"}, {"format-justify-left", "format_align_left"},
            {"format-justify-center", "format_align_center"}, {"format-justify-right", "format_align_right"},
            {"format-justify-fill", "format_align_justify"}, {"format-indent-more", "format_indent_increase"},
            {"format-indent-less", "format_indent_decrease"}, {"insert-image", "add_photo_alternate"}, {"insert-link", "link"},
            {"insert-text", "text_fields"},
            // Settings panes and the rest theDesk asks for (they fell through
            // to the old colourful theme)
            {"preferences-system", "settings"}, {"preferences-system-network", "wifi"},
            {"preferences-system-locale", "language"}, {"preferences-system-bluetooth", "bluetooth"},
            {"preferences-system-power", "battery_charging_full"}, {"preferences-system-plugins", "extension"},
            {"preferences-system-danger", "healing"}, {"preferences-desktop-about", "info"},
            {"preferences-desktop-default-applications", "apps"}, {"preferences-desktop-input", "keyboard"},
            {"preferences-desktop-mouse", "mouse"}, {"preferences-desktop-user", "person"}, {"systemsettings", "settings"},
            {"thedesk-overview", "dashboard"}, {"tools-report-bug", "bug_report"}, {"applications-development", "code"}, {"package-upgrade", "system_update"},
            {"redshift-on", "nightlight"}, {"caffeinate", "coffee"}, {"flight-mode", "flight"}, {"gps", "my_location"},
            {"sim-card", "sim_card"}, {"sim-card-none", "sim_card_alert"}, {"key-enter", "keyboard_return"},
            {"commit", "commit"}, {"image", "image"}, {"text-html", "html"},
            {"image-stretch-fit", "fit_screen"}, {"image-stretch-zoom-crop", "crop"}, {"image-stretch-center", "filter_center_focus"},
            {"image-stretch-tile", "grid_view"}, {"image-stretch-zoom-fit", "zoom_out_map"},
        };
        return map;
    }

    // Does the font draw `symbol` as one glyph (a known ligature)?
    bool isSymbol(const QString& symbol) {
        static QHash<QString, bool> cache;
        static QMutex lock;
        QMutexLocker locker(&lock);
        auto it = cache.constFind(symbol);
        if (it != cache.constEnd()) return *it;
        QFont font(family);
        font.setPixelSize(24);
        QTextLayout layout(symbol, font);
        layout.beginLayout();
        layout.createLine();
        layout.endLayout();
        qsizetype glyphs = 0;
        for (const QGlyphRun& run : layout.glyphRuns()) glyphs += run.glyphIndexes().size();
        bool ok = glyphs == 1 && QFontDatabase::families().contains(family);
        cache.insert(symbol, ok);
        return ok;
    }

    QColor colourFor(QIcon::Mode mode) {
        const QPalette pal = QGuiApplication::palette();
        switch (mode) {
            case QIcon::Disabled:
                return pal.color(QPalette::Disabled, QPalette::WindowText);
            case QIcon::Selected:
                return pal.color(QPalette::HighlightedText);
            default:
                return pal.color(QPalette::WindowText);
        }
    }
} // namespace

bool MaterialSymbolEngine::enabled() {
    static const bool on = [] {
        tSettings settings(QStringLiteral("theSuite"), QStringLiteral("theDesk.platform"));
        return settings.value(QStringLiteral("Platform/icons.symbols")).toString() != QStringLiteral("theme");
    }();
    return on;
}

QString MaterialSymbolEngine::symbolFor(const QString& iconName) {
    if (!enabled()) return {};
    QString name = iconName;
    const bool symbolic = name.endsWith(QLatin1String("-symbolic"));
    if (symbolic) name.chop(9);
    for (const char* suffix : {"-rtl", "-ltr"}) {
        if (name.endsWith(QLatin1String(suffix))) name.chop(4);
    }
    // Names apps also use as their own (full-colour) icons only become
    // symbols when asked for as -symbolic
    const bool appLike = name.startsWith(QLatin1String("preferences-")) || name.startsWith(QLatin1String("applications-")) || name == QLatin1String("computer") || name == QLatin1String("systemsettings");
    if (appLike && !symbolic) return {};
    QString symbol = table().value(name);
    // Other symbolic names may already be Material names ("wifi-symbolic")
    if (symbol.isEmpty() && symbolic) symbol = QString(name).replace(QLatin1Char('-'), QLatin1Char('_'));
    if (symbol.isEmpty() || !isSymbol(symbol)) return {};
    return symbol;
}

MaterialSymbolEngine::MaterialSymbolEngine(const QString& iconName, const QString& symbol) :
    name(iconName), symbol(symbol) {
}

void MaterialSymbolEngine::paint(QPainter* painter, const QRect& rect, QIcon::Mode mode, QIcon::State state) {
    const double size = qMin(rect.width(), rect.height());
    if (size <= 0) return;
    QFont font(family);
    font.setPixelSize(qRound(size));
    font.setVariableAxis(QFont::Tag("FILL"), state == QIcon::On ? 1 : 0);
    font.setVariableAxis(QFont::Tag("wght"), size <= 16 ? 500 : 400);
    font.setVariableAxis(QFont::Tag("GRAD"), 0);
    font.setVariableAxis(QFont::Tag("opsz"), float(qBound(20.0, size, 48.0)));
    font.setStyleStrategy(QFont::PreferAntialias);

    QPainterPath glyph;
    glyph.setFillRule(Qt::WindingFill);
    glyph.addText(0, 0, font, symbol);
    // The em square is the icon: place the glyph by the font's metrics so
    // every symbol keeps its designed position
    QFontMetricsF metrics(font);
    const double emTop = -metrics.ascent();
    const double emHeight = metrics.ascent() + metrics.descent();
    QPointF offset(rect.left() + (rect.width() - metrics.horizontalAdvance(symbol)) / 2.0,
        rect.top() + (rect.height() - emHeight) / 2.0 - emTop);

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    painter->fillPath(glyph.translated(offset), colourFor(mode));
    painter->restore();
}

QPixmap MaterialSymbolEngine::pixmap(const QSize& size, QIcon::Mode mode, QIcon::State state) {
    return scaledPixmap(size, mode, state, 1);
}

QPixmap MaterialSymbolEngine::scaledPixmap(const QSize& size, QIcon::Mode mode, QIcon::State state, qreal scale) {
    QPixmap pixmap(size * scale);
    pixmap.setDevicePixelRatio(scale);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    paint(&painter, QRect(QPoint(0, 0), size), mode, state);
    return pixmap;
}

QSize MaterialSymbolEngine::actualSize(const QSize& size, QIcon::Mode mode, QIcon::State state) {
    Q_UNUSED(mode)
    Q_UNUSED(state)
    int side = qMin(size.width(), size.height());
    return QSize(side, side);
}

QList<QSize> MaterialSymbolEngine::availableSizes(QIcon::Mode mode, QIcon::State state) {
    Q_UNUSED(mode)
    Q_UNUSED(state)
    return {QSize(16, 16), QSize(20, 20), QSize(24, 24), QSize(32, 32), QSize(48, 48), QSize(64, 64)};
}

QString MaterialSymbolEngine::key() const {
    return QStringLiteral("MaterialSymbol");
}

QString MaterialSymbolEngine::iconName() {
    return name;
}

QIconEngine* MaterialSymbolEngine::clone() const {
    return new MaterialSymbolEngine(name, symbol);
}

bool MaterialSymbolEngine::isNull() {
    return false;
}
