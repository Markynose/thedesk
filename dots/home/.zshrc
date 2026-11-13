# ~/.zshrc: interactive zsh. Prompt: starship (~/.config/starship.toml)

# --- History -----------------------------------------------------------------
HISTFILE=${XDG_STATE_HOME:-$HOME/.local/state}/zsh/history
mkdir -p "${HISTFILE:h}"
HISTSIZE=50000
SAVEHIST=50000
setopt hist_ignore_all_dups hist_ignore_space hist_reduce_blanks share_history extended_history

# --- Behaviour ---------------------------------------------------------------
setopt auto_cd auto_pushd pushd_ignore_dups interactive_comments no_beep
bindkey -e                                  # emacs keys (Ctrl+A/E/R, ...)
bindkey '^[[1;5C' forward-word              # Ctrl+Right
bindkey '^[[1;5D' backward-word             # Ctrl+Left
bindkey '^[[3~' delete-char                 # Delete
bindkey '^[[H' beginning-of-line            # Home
bindkey '^[[F' end-of-line                  # End

# --- Completion --------------------------------------------------------------
fpath=(/usr/share/zsh/site-functions $fpath)
autoload -Uz compinit
compinit -d "${XDG_CACHE_HOME:-$HOME/.cache}/zcompdump"
zstyle ':completion:*' menu select
zstyle ':completion:*' matcher-list 'm:{a-z}={A-Z}'      # case-insensitive
zstyle ':completion:*' list-colors "${(s.:.)LS_COLORS}"

# --- Aliases -----------------------------------------------------------------
alias ls='ls --color=auto --group-directories-first'
alias ll='ls -lh'
alias la='ls -lAh'
alias grep='grep --color=auto'
alias diff='diff --color=auto'
alias ip='ip -color=auto'
alias ..='cd ..'
alias ...='cd ../..'
alias g='git'
alias rc='rc-service'                       # OpenRC: rc <service> <start|stop|status>

# theDesk: show first-time setup again at the next login
onboarding-reset() {
    local conf=${XDG_CONFIG_HOME:-$HOME/.config}/theSuite/theDesk.conf
    if grep -q '^lastOnboarding=' "$conf" 2>/dev/null; then
        sed -i 's/^lastOnboarding=.*/lastOnboarding=0/' "$conf"
    else
        printf '\n[Onboarding]\nlastOnboarding=0\n' >>"$conf"
    fi
    print "Onboarding will run at your next login."
}

# --- Plugins (repo packages) -------------------------------------------------
[[ -r /usr/share/zsh/plugins/zsh-autosuggestions/zsh-autosuggestions.zsh ]] &&
    source /usr/share/zsh/plugins/zsh-autosuggestions/zsh-autosuggestions.zsh
ZSH_AUTOSUGGEST_HIGHLIGHT_STYLE='fg=8'          # Outline (kitty's colour 8)

# --- Prompt ------------------------------------------------------------------
# starship in Material 3 (~/.config/starship.toml). In a theDesk session the
# copy in ~/.cache/thedesk has the session's colours (it follows the living
# horizon); use it while it's newer than the template. Checked every prompt,
# so a recolour shows at the next one.
_thedesk_starship_config() {
    local themed=${XDG_CACHE_HOME:-$HOME/.cache}/thedesk/starship.toml
    local template=${XDG_CONFIG_HOME:-$HOME/.config}/starship.toml
    if [[ $themed -nt $template ]]; then
        export STARSHIP_CONFIG=$themed
    else
        unset STARSHIP_CONFIG
    fi
}
if command -v starship >/dev/null; then
    autoload -Uz add-zsh-hook
    add-zsh-hook precmd _thedesk_starship_config
    _thedesk_starship_config
    eval "$(starship init zsh)"
fi

# Syntax highlighting must be sourced last. Colours by number: kitty's 16
# colours are theDesk's Material 3 terminal palette (they follow the horizon,
# even in lines already typed).
if [[ -r /usr/share/zsh/plugins/zsh-syntax-highlighting/zsh-syntax-highlighting.zsh ]]; then
    source /usr/share/zsh/plugins/zsh-syntax-highlighting/zsh-syntax-highlighting.zsh
    ZSH_HIGHLIGHT_STYLES[unknown-token]='fg=1'
    ZSH_HIGHLIGHT_STYLES[reserved-word]='fg=5'
    ZSH_HIGHLIGHT_STYLES[alias]='fg=2'
    ZSH_HIGHLIGHT_STYLES[suffix-alias]='fg=2,underline'
    ZSH_HIGHLIGHT_STYLES[global-alias]='fg=2'
    ZSH_HIGHLIGHT_STYLES[builtin]='fg=2'
    ZSH_HIGHLIGHT_STYLES[function]='fg=2'
    ZSH_HIGHLIGHT_STYLES[command]='fg=2'
    ZSH_HIGHLIGHT_STYLES[precommand]='fg=2,italic'
    ZSH_HIGHLIGHT_STYLES[commandseparator]='fg=8'
    ZSH_HIGHLIGHT_STYLES[path]='underline'
    ZSH_HIGHLIGHT_STYLES[globbing]='fg=5'
    ZSH_HIGHLIGHT_STYLES[history-expansion]='fg=5'
    ZSH_HIGHLIGHT_STYLES[single-hyphen-option]='fg=6'
    ZSH_HIGHLIGHT_STYLES[double-hyphen-option]='fg=6'
    ZSH_HIGHLIGHT_STYLES[single-quoted-argument]='fg=3'
    ZSH_HIGHLIGHT_STYLES[double-quoted-argument]='fg=3'
    ZSH_HIGHLIGHT_STYLES[dollar-quoted-argument]='fg=3'
    ZSH_HIGHLIGHT_STYLES[dollar-double-quoted-argument]='fg=4'
    ZSH_HIGHLIGHT_STYLES[back-quoted-argument]='fg=4'
    ZSH_HIGHLIGHT_STYLES[redirection]='fg=4'
    ZSH_HIGHLIGHT_STYLES[assign]='fg=4'
    ZSH_HIGHLIGHT_STYLES[comment]='fg=8,italic'
fi
