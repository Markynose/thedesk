#!/usr/bin/env bash
# Install the dotfiles in dots/.
#
#   dots/home/    symlinked into $HOME (existing files are backed up first)
#   dots/merge/   INI keys merged into files the apps rewrite themselves
#                 (theDesk's QSettings files can't be symlinks)
#   dots/seed/    copied into $HOME only where the file doesn't exist yet
#                 (settings an app keeps rewriting, such as Zed's)
#   dots/system/  copied into / with sudo (only with --system)
#
# Usage:
#   scripts/install-dots.sh              # home + merge
#   scripts/install-dots.sh --system     # also system files, TLP service, video driver
#   scripts/install-dots.sh --extras     # also: login shell -> zsh, VS Code extensions
#   scripts/install-dots.sh --all        # everything
#   scripts/install-dots.sh --dry-run    # show what would happen
#
# Backups go to ~/.local/state/dots-backup/<timestamp>/ (same relative paths).

set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
DOTS=$ROOT/dots
BACKUP=${XDG_STATE_HOME:-$HOME/.local/state}/dots-backup/$(date +%Y%m%d-%H%M%S)

do_system=false
do_extras=false
dry=false
for arg in "$@"; do
    case $arg in
        --system) do_system=true ;;
        --extras) do_extras=true ;;
        --all) do_system=true; do_extras=true ;;
        --dry-run|-n) dry=true ;;
        -h|--help) sed -n '2,17p' "$0"; exit 0 ;;
        *) echo "Unknown option: $arg" >&2; exit 2 ;;
    esac
done

log()  { printf '\033[1;33m==>\033[0m %s\n' "$*"; }
warn() { printf '\033[1;31m!!\033[0m %s\n' "$*" >&2; }
run()  { if $dry; then echo "    [dry-run] $*"; else "$@"; fi; }

backup() { # backup <absolute path> <relative path>
    local src=$1 rel=$2
    [[ -e $src || -L $src ]] || return 0
    run mkdir -p "$(dirname "$BACKUP/$rel")"
    run mv "$src" "$BACKUP/$rel"
    echo "    backed up $rel"
}

# --- home: symlinks ------------------------------------------------------------
log "Linking dots/home into $HOME"
while IFS= read -r -d '' file; do
    rel=${file#"$DOTS/home/"}
    target=$HOME/$rel
    if [[ -L $target && $(readlink "$target") == "$file" ]]; then
        continue # already linked
    fi
    backup "$target" "$rel"
    run mkdir -p "$(dirname "$target")"
    run ln -s "$file" "$target"
    echo "    linked $rel"
done < <(find "$DOTS/home" \( -type f -o -type l \) -print0 | sort -z)

# --- seed: starting points, never overwritten ------------------------------------
log "Seeding dots/seed into $HOME"
while IFS= read -r -d '' file; do
    rel=${file#"$DOTS/seed/"}
    target=$HOME/$rel
    [[ -e $target || -L $target ]] && continue # the app's own copy stays
    run mkdir -p "$(dirname "$target")"
    run cp "$file" "$target"
    echo "    seeded $rel"
done < <(find "$DOTS/seed" -type f -print0 | sort -z)

# --- merge: INI keys -------------------------------------------------------------
log "Merging dots/merge settings"
while IFS= read -r -d '' file; do
    rel=${file#"$DOTS/merge/"}
    target=$HOME/$rel
    if $dry; then
        echo "    [dry-run] merge $rel"
        continue
    fi
    mkdir -p "$(dirname "$target")"
    if [[ -f $target ]]; then
        mkdir -p "$(dirname "$BACKUP/$rel")"
        cp -p "$target" "$BACKUP/$rel"
    fi
    # Keys from the dots file override the user's; everything else is kept.
    python3 -I - "$file" "$target" <<'PY'
import configparser, os, sys
src, dst = sys.argv[1], sys.argv[2]
home = os.path.expanduser("~")

def load(path):
    cp = configparser.RawConfigParser(strict=False, delimiters=("=",), comment_prefixes=(";", "#"),
                                      inline_comment_prefixes=None, interpolation=None)
    cp.optionxform = str  # keys are case-sensitive in QSettings
    cp.read(path, encoding="utf-8")
    return cp

out, new = load(dst), load(src)
for section in new.sections():
    if not out.has_section(section):
        out.add_section(section)
    for key, value in new.items(section):
        out.set(section, key, value.replace("@HOME@", home))  # @HOME@ = user's home
with open(dst, "w", encoding="utf-8") as f:
    out.write(f, space_around_delimiters=False)
PY
    echo "    merged $rel"
done < <(find "$DOTS/merge" -type f -print0 | sort -z)

# Load X resources into the running session
if command -v xrdb >/dev/null && [[ -n ${DISPLAY:-} && -f $HOME/.Xresources ]]; then
    run xrdb -merge "$HOME/.Xresources"
fi

# --- system ------------------------------------------------------------------------
if $do_system; then
    log "Installing dots/system into / (sudo)"
    while IFS= read -r -d '' file; do
        rel=${file#"$DOTS/system/"}
        target=/$rel
        if [[ -f $target ]] && cmp -s "$file" "$target"; then
            continue
        fi
        if [[ -e $target ]]; then
            run sudo mkdir -p "$(dirname "$BACKUP/system/$rel")"
            run sudo cp -p "$target" "$BACKUP/system/$rel"
        fi
        run sudo install -Dm644 "$file" "$target"
        echo "    installed /$rel"
    done < <(find "$DOTS/system" -type f -print0 | sort -z)

    if pacman -Qq xf86-video-intel >/dev/null 2>&1; then
        log "Removing xf86-video-intel (modesetting is used instead; applies on next X start)"
        run sudo pacman -Rns --noconfirm xf86-video-intel
    fi

    if [[ -x /etc/init.d/tlp ]]; then
        log "Enabling TLP"
        run sudo rc-update add tlp default
        run sudo rc-service tlp restart
    else
        warn "tlp-openrc not installed; skipping TLP service"
    fi
fi

# --- extras --------------------------------------------------------------------------
if $do_extras; then
    if [[ $(getent passwd "$USER" | cut -d: -f7) != /usr/bin/zsh ]]; then
        log "Changing login shell to zsh"
        run sudo chsh -s /usr/bin/zsh "$USER"
    fi
    if command -v code >/dev/null; then
        log "Installing VS Code extensions"
        for ext in jdinhlife.gruvbox llvm-vs-code-extensions.vscode-clangd ms-vscode.cmake-tools; do
            run code --install-extension "$ext" --force >/dev/null && echo "    $ext"
        done
    fi
fi

log "Done.${dry:+}"
[[ -d $BACKUP ]] && echo "Backups: $BACKUP"
exit 0
