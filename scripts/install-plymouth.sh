#!/bin/sh
# Turn on theDesk's boot splash (the Plymouth theme "thedesk", installed with
# theDesk by tools/boot-theme):
#   1. make it Plymouth's theme
#   2. add the plymouth hook to /etc/mkinitcpio.conf, after udev (a copy of
#      the old file is kept as mkinitcpio.conf.thedesk-backup)
#   3. add "splash" to Limine's kernel command line (scripts/install-limine.sh)
#   4. rebuild the initramfs (the ESP copy follows through the post hook)
#
#   scripts/install-plymouth.sh            do it
#   scripts/install-plymouth.sh --dry-run  show what would change
#   scripts/install-plymouth.sh --remove   take the hook and "splash" out again
#
# If a boot ever goes wrong: in Limine press E and delete "splash", or pick
# GRUB in Limine's menu (its command line has no "splash").
set -eu

here=$(cd "$(dirname "$0")/.." && pwd)
dry=
remove=
for arg in "$@"; do
    case $arg in
        --dry-run) dry=1 ;;
        --remove) remove=1 ;;
        -h | --help) sed -n '2,17p' "$0"; exit 0 ;;
        *) echo "unknown option: $arg" >&2; exit 1 ;;
    esac
done
run() {
    echo "+ $*"
    [ -n "$dry" ] || sudo "$@"
}

[ -f /usr/share/plymouth/themes/thedesk/thedesk.plymouth ] || { echo "The thedesk Plymouth theme isn't installed (build theDesk first)" >&2; exit 1; }

hooks=$(sed -n 's/^HOOKS=(\(.*\))/\1/p' /etc/mkinitcpio.conf)
cmdline=$(tr ' ' '\n' < /proc/cmdline | grep -v '^BOOT_IMAGE=' | grep -v '^initrd=' | grep -vx 'splash' | paste -sd' ' -)
if [ -n "$remove" ]; then
    new_hooks=$(echo "$hooks" | tr ' ' '\n' | grep -vx plymouth | paste -sd' ' -)
else
    run plymouth-set-default-theme thedesk
    if echo "$hooks" | tr ' ' '\n' | grep -qx plymouth; then
        new_hooks=$hooks
    else
        new_hooks=$(echo "$hooks" | sed 's/\budev\b/udev plymouth/')
    fi
    cmdline="$cmdline splash"
fi

if [ "$new_hooks" != "$hooks" ]; then
    echo "HOOKS: ($hooks) -> ($new_hooks)"
    [ -f /etc/mkinitcpio.conf.thedesk-backup ] || run cp /etc/mkinitcpio.conf /etc/mkinitcpio.conf.thedesk-backup
    run sed -i "s/^HOOKS=(.*)/HOOKS=($new_hooks)/" /etc/mkinitcpio.conf
fi

# Limine's entries take the new command line (and the theme is re-rendered)
if [ -n "$dry" ]; then
    "$here/scripts/install-limine.sh" --dry-run --cmdline "$cmdline" | grep -E 'cmdline|Kernel command line'
else
    "$here/scripts/install-limine.sh" --cmdline "$cmdline"
fi

run mkinitcpio -P
