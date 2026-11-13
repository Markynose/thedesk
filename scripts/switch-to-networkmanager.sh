#!/usr/bin/env bash
# Switch an Artix/OpenRC system from ConnMan to NetworkManager.
#
# theDesk's network UI (plugins/NetworkPlugin) only talks to NetworkManager,
# so this replaces connmand with NetworkManager and migrates saved ConnMan
# WPA-PSK Wi-Fi networks into NetworkManager.
#
# Usage:
#   sudo ./switch-to-networkmanager.sh             # switch (keeps connman installed)
#   sudo ./switch-to-networkmanager.sh --purge     # switch and uninstall connman + trays
#   sudo ./switch-to-networkmanager.sh --rollback  # go back to connman
#
# Run it from a local session (TTY or desktop): the network drops for a few
# seconds while the services are swapped. Packages are installed *before*
# connman is stopped, so you are never left offline without NetworkManager.

set -euo pipefail

NM_PKGS=(networkmanager networkmanager-openrc)
CONNMAN_PKGS=(connman connman-openrc connman-gtk cmst)
CONNMAN_DIR=/var/lib/connman
TRAY_ENTRIES=(connman-gtk.desktop cmst-autostart.desktop)

mode=switch
case "${1:-}" in
    "") ;;
    --purge) mode=purge ;;
    --rollback) mode=rollback ;;
    -h|--help) sed -n '2,15p' "$0"; exit 0 ;;
    *) echo "Unknown option: $1 (try --help)" >&2; exit 2 ;;
esac

log()  { printf '\033[1;33m==>\033[0m %s\n' "$*"; }
warn() { printf '\033[1;31m!!\033[0m %s\n' "$*" >&2; }

[[ $EUID -eq 0 ]] || { warn "Run as root (sudo $0 $*)"; exit 1; }

# The desktop user whose autostart entries we touch.
target_user=${SUDO_USER:-}
if [[ -z $target_user || $target_user == root ]]; then
    target_user=$(logname 2>/dev/null || true)
fi
user_home=""
if [[ -n $target_user && $target_user != root ]]; then
    user_home=$(getent passwd "$target_user" | cut -d: -f6)
fi
autostart_dir=${user_home:+$user_home/.config/autostart}

# Hide/unhide a tray autostart entry by toggling Hidden=true (reversible,
# unlike deleting the file).
set_tray_hidden() {
    local hidden=$1 entry file
    [[ -n $autostart_dir && -d $autostart_dir ]] || return 0
    for entry in "${TRAY_ENTRIES[@]}"; do
        file=$autostart_dir/$entry
        [[ -f $file ]] || continue
        sed -i '/^Hidden=/d' "$file"
        if [[ $hidden == true ]]; then
            # Insert right after the [Desktop Entry] header.
            sed -i '0,/^\[Desktop Entry\]/s//[Desktop Entry]\nHidden=true/' "$file"
            log "Disabled autostart: $entry"
        else
            log "Re-enabled autostart: $entry"
        fi
    done
}

service_enabled() { rc-update show default 2>/dev/null | grep -qw "$1"; }

wait_for_nm_connected() {
    local i state
    for i in $(seq 1 30); do
        state=$(nmcli -t -f STATE general 2>/dev/null || true)
        [[ $state == connected* ]] && return 0
        sleep 1
    done
    return 1
}

# ConnMan stores Name=/Passphrase= in /var/lib/connman/<service>/settings.
# Only WPA-PSK services (…_managed_psk) are migrated; enterprise/open networks
# are left for you to add with nmtui.
migrate_wifi() {
    local dir name pass count=0
    shopt -s nullglob
    for dir in "$CONNMAN_DIR"/wifi_*_managed_psk; do
        [[ -f $dir/settings ]] || continue
        name=$(sed -n 's/^Name=//p' "$dir/settings" | head -n1)
        pass=$(sed -n 's/^Passphrase=//p' "$dir/settings" | head -n1)
        if [[ -z $name || -z $pass ]]; then
            warn "Skipping $(basename "$dir") (no name or passphrase stored)"
            continue
        fi
        if nmcli -t -f NAME connection show | grep -Fxq -- "$name"; then
            log "NetworkManager already knows \"$name\", skipping"
            continue
        fi
        # Passed as argv to nmcli (not via a shell string), so odd SSIDs
        # and passphrases are safe.
        nmcli connection add type wifi con-name "$name" ssid "$name" \
            wifi-sec.key-mgmt wpa-psk wifi-sec.psk "$pass" \
            connection.autoconnect yes >/dev/null
        log "Migrated Wi-Fi network \"$name\""
        count=$((count + 1))
    done
    shopt -u nullglob
    log "Migrated $count Wi-Fi network(s) from ConnMan"
}

do_rollback() {
    command -v connmand >/dev/null || { warn "connman is not installed; install it first: pacman -S ${CONNMAN_PKGS[*]:0:2}"; exit 1; }
    log "Stopping NetworkManager"
    rc-service NetworkManager stop 2>/dev/null || true
    rc-update del NetworkManager default 2>/dev/null || true
    log "Starting connmand"
    rc-update add connmand default
    rc-service connmand start
    set_tray_hidden false
    log "Rolled back to ConnMan."
}

do_switch() {
    log "Installing ${NM_PKGS[*]} (while connman still provides network)"
    pacman -S --needed --noconfirm "${NM_PKGS[@]}"

    [[ -x /etc/init.d/NetworkManager ]] || { warn "/etc/init.d/NetworkManager missing after install; aborting before touching connman"; exit 1; }

    # ConnMan and NM both want to own wpa_supplicant and the interfaces;
    # stop connman first.
    if service_enabled connmand || rc-service connmand status >/dev/null 2>&1; then
        log "Stopping and disabling connmand"
        rc-service connmand stop || true
        rc-update del connmand default || true
    fi

    log "Enabling and starting NetworkManager"
    rc-update add NetworkManager default
    rc-service NetworkManager start

    # Give NM a moment to come up before talking to it.
    for _ in $(seq 1 10); do nmcli general status >/dev/null 2>&1 && break; sleep 1; done

    migrate_wifi
    set_tray_hidden true

    log "Waiting for a connection…"
    if wait_for_nm_connected; then
        log "Connected: $(nmcli -t -f NAME connection show --active | head -n1)"
    else
        warn "Not connected yet. Try: nmtui   or   nmcli device wifi connect <SSID> --ask"
        warn "To undo: sudo $0 --rollback"
    fi

    if [[ $mode == purge ]]; then
        local installed=()
        for p in "${CONNMAN_PKGS[@]}"; do pacman -Qq "$p" >/dev/null 2>&1 && installed+=("$p"); done
        if ((${#installed[@]})); then
            log "Removing ${installed[*]}"
            pacman -Rns --noconfirm "${installed[@]}"
        fi
    else
        log "connman is still installed (disabled). Re-run with --purge to remove it."
    fi
}

case $mode in
    rollback) do_rollback ;;
    *) do_switch ;;
esac
