#!/bin/sh
# Install theDesk's themed Limine next to the existing bootloader (GRUB stays).
#
#   scripts/install-limine.sh             files only: theme, config, kernel
#                                         copies and the sync hooks on the ESP
#   scripts/install-limine.sh --register  also add a firmware boot entry for
#                                         Limine, NOT in the boot order
#   scripts/install-limine.sh --try       boot Limine once on the next boot
#                                         (BootNext); after that the firmware
#                                         goes back to the usual order
#   scripts/install-limine.sh --default   make Limine first in the boot order
#
# Options: --cmdline "..." (default: this boot's command line), --dry-run
#
# Steps with consequences (--register/--try/--default) only run when asked.
# GRUB keeps its own entry and is also in Limine's menu.
set -eu

here=$(cd "$(dirname "$0")/.." && pwd)
label="theDesk (Limine)"
mode=files
dry=
cmdline=
while [ $# -gt 0 ]; do
    case $1 in
        --register) mode=register ;;
        --try) mode=try ;;
        --default) mode=default ;;
        --cmdline) cmdline=${2:?--cmdline needs a value}; shift ;;
        --dry-run) dry=1 ;;
        -h | --help) sed -n '2,17p' "$0"; exit 0 ;;
        *) echo "unknown option: $1" >&2; exit 1 ;;
    esac
    shift
done

run() { # show, then run (as root) unless --dry-run
    echo "+ $*"
    [ -n "$dry" ] || sudo "$@"
}

# --- Where things are
esp=$(findmnt -no TARGET -t vfat /boot/efi 2>/dev/null || findmnt -no TARGET -t vfat /efi 2>/dev/null || findmnt -no TARGET -t vfat /boot 2>/dev/null || true)
[ -n "$esp" ] || { echo "No FAT EFI system partition mounted at /boot/efi, /efi or /boot" >&2; exit 1; }
espdev=$(findmnt -no SOURCE "$esp")
disk=/dev/$(lsblk -no PKNAME "$espdev")
part=$(cat "/sys/class/block/$(basename "$espdev")/partition")

entry_number() { # the firmware entry for our loader, if any
    efibootmgr | sed -n "s/^Boot\([0-9A-F]\{4\}\)[* ] $label\t.*/\1/p" | head -n1
}

case $mode in
    register)
        if [ -n "$(entry_number)" ]; then echo "Already registered as Boot$(entry_number)"; exit 0; fi
        # --create-only: add the entry without putting it in BootOrder
        run efibootmgr --create-only --disk "$disk" --part "$part" --label "$label" --loader '\EFI\limine\BOOTX64.EFI'
        [ -n "$dry" ] || echo "Registered as Boot$(entry_number); boot order unchanged"
        exit 0 ;;
    try)
        n=$(entry_number); [ -n "$n" ] || { echo "Not registered yet (--register)" >&2; exit 1; }
        run efibootmgr --bootnext "$n"
        echo "The next boot (only) goes through Limine; the one after is back to the usual order"
        exit 0 ;;
    default)
        n=$(entry_number); [ -n "$n" ] || { echo "Not registered yet (--register)" >&2; exit 1; }
        order=$(efibootmgr | sed -n 's/^BootOrder: //p' | tr ',' '\n' | grep -vx "$n" | paste -sd, -)
        run efibootmgr --bootorder "$n${order:+,$order}"
        exit 0 ;;
esac

# --- Files: theme rendered as you (it reads your theSuite settings), copied as root
tool=$here/build/thedesk/tools/boot-theme/thedesk-boot-theme
if [ -x "$tool" ]; then
    export LD_LIBRARY_PATH=$here/build/thedesk/libthedesk${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}
else
    tool=thedesk-boot-theme
fi
size=$(head -n1 "$(ls /sys/class/drm/card*-eDP-*/modes /sys/class/drm/card*-*/modes 2>/dev/null | head -n1)" 2>/dev/null || true)
size=${size:-1366x768}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
"$tool" limine "$work" --size "$size" 2>/dev/null

if [ -z "$cmdline" ]; then
    # This boot's command line, minus the bootloader's own additions
    cmdline=$(tr ' ' '\n' < /proc/cmdline | grep -v '^BOOT_IMAGE=' | grep -v '^initrd=' | paste -sd' ' -)
fi

{
    cat "$work/theme.conf"
    cat <<EOF
timeout: 3

/theDesk  (Artix Linux)
    comment: Boot theDesk
    protocol: linux
    path: boot():/vmlinuz-linux
    module_path: boot():/initramfs-linux.img
    cmdline: $cmdline
EOF
    if [ -f /boot/initramfs-linux-fallback.img ]; then
        cat <<EOF

/theDesk  (Artix Linux, fallback initramfs)
    comment: Every driver, for when the usual initramfs won't boot
    protocol: linux
    path: boot():/vmlinuz-linux
    module_path: boot():/initramfs-linux-fallback.img
    cmdline: $cmdline
EOF
    fi
    # The ESP is root-only, so look for GRUB as root
    grub=$(sudo find "$esp/EFI" -mindepth 2 -maxdepth 2 -iname grubx64.efi | sort | head -n1)
    if [ -n "$grub" ]; then
        rel=${grub#"$esp"}
        cat <<EOF

/GRUB  (old bootloader)
    comment: The previous boot menu, kept as a fallback
    protocol: efi
    path: boot():$rel
EOF
    fi
} > "$work/limine.conf"

echo "ESP: $esp ($disk, partition $part); menu at $size"
echo "Kernel command line: $cmdline"
run install -Dm644 /usr/share/limine/BOOTX64.EFI "$esp/EFI/limine/BOOTX64.EFI"
run install -Dm644 "$work/thedesk.png" "$esp/EFI/limine/thedesk.png"
run install -Dm644 "$work/roboto-flex.f16" "$esp/EFI/limine/roboto-flex.f16"
run install -Dm644 "$work/limine.conf" "$esp/EFI/limine/limine.conf"
for f in /boot/vmlinuz-linux /boot/initramfs-linux.img /boot/initramfs-linux-fallback.img; do
    [ -f "$f" ] && run install -m644 "$f" "$esp/$(basename "$f")"
done
sed "s|@ESP@|$esp|g" "$here/boot/limine/thedesk-limine-sync" > "$work/thedesk-limine-sync"
sed "s|@ESP@|$esp|g" "$here/boot/limine/thedesk-limine.hook" > "$work/thedesk-limine.hook"
run install -Dm755 "$work/thedesk-limine-sync" /etc/initcpio/post/thedesk-limine-sync
run install -Dm644 "$work/thedesk-limine.hook" /etc/pacman.d/hooks/thedesk-limine.hook
echo
if [ -n "$(entry_number)" ]; then
    echo "Limine is on the ESP (firmware entry Boot$(entry_number))."
else
    echo "Limine is on the ESP. Nothing boots it yet: next is --register, then --try."
fi
[ -z "$dry" ] || cat "$work/limine.conf"
