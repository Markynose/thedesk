#!/bin/sh
# Make theDesk's greeter (thedesk-greeter, installed with theDesk) LightDM's
# login screen, set up for a seamless boot:
#   - greeter-session=thedesk-greeter
#   - minimum-vt=1: X starts on VT 1 over Plymouth's last frame (no switch to
#     another console); the text login on tty1 is turned off (tty2-6 stay)
#   - /var/lib/lightdm-data passable (1771), so each session can leave its
#     colours and picture for the greeter; a pacman hook keeps it so after
#     LightDM upgrades
# A copy of lightdm.conf is kept as lightdm.conf.thedesk-backup.
#   scripts/install-greeter.sh [--dry-run]
set -eu
here=$(cd "$(dirname "$0")/.." && pwd)
dry=
[ "${1:-}" = "--dry-run" ] && dry=1
run() {
    echo "+ $*"
    [ -n "$dry" ] || sudo "$@"
}
conf=/etc/lightdm/lightdm.conf

[ -x /usr/bin/thedesk-greeter ] || { echo "thedesk-greeter isn't installed (build theDesk first)" >&2; exit 1; }
[ -f $conf.thedesk-backup ] || run cp $conf $conf.thedesk-backup
grep -q '^greeter-session=thedesk-greeter$' $conf || run sed -i 's/^#\?greeter-session=.*/greeter-session=thedesk-greeter/' $conf
grep -q '^minimum-vt=1$' $conf || run sed -i '0,/^\[LightDM\]$/s//[LightDM]\nminimum-vt=1/' $conf
if rc-update show default 2>/dev/null | grep -q 'agetty.tty1'; then run rc-update del agetty.tty1 default; fi
run chmod 1771 /var/lib/lightdm-data
run install -Dm644 "$here/boot/lightdm/thedesk-lightdm-data.hook" /etc/pacman.d/hooks/thedesk-lightdm-data.hook
echo "Done: the next login screen is theDesk's."
