# ~/.zshenv: read by every zsh (login, interactive, scripts).
# Keep this minimal: environment only.

# ~/.local/bin first (user scripts, claude, pipx, ...). typeset -U drops duplicates.
typeset -U path PATH
path=("$HOME/.local/bin" $path)
export PATH

export EDITOR='code --wait'
export VISUAL='code --wait'
export PAGER=less
export LESS='-R --mouse'

# Plan 9 cursor everywhere (X apps that don't read Xresources/GTK settings)
export XCURSOR_THEME=theDesk-M3
export XCURSOR_SIZE=24
