#!/bin/sh
# Installs the zovwm dotfiles into $HOME: .xinitrc, .Xresources, and a
# snippet appended to .bash_profile that auto-starts X on login from the
# first virtual terminal. Any existing file that differs is backed up with
# a .bak suffix rather than overwritten silently. Safe to re-run.
set -e

DIR=$(cd "$(dirname "$0")" && pwd)

install_file() {
	src="$DIR/$1"
	dst="$HOME/$2"
	if [ -e "$dst" ] && ! cmp -s "$src" "$dst"; then
		cp "$dst" "$dst.bak"
		echo "Backed up existing $dst -> $dst.bak"
	fi
	cp "$src" "$dst"
	echo "Installed $dst"
}

install_file xinitrc .xinitrc
install_file Xresources .Xresources
chmod +x "$HOME/.xinitrc"

PROFILE="$HOME/.bash_profile"
MARKER="# zovwm dotfiles: auto-startx on tty1"
if [ -f "$PROFILE" ] && grep -qF "$MARKER" "$PROFILE" 2>/dev/null; then
	echo "$PROFILE already has the auto-startx snippet, skipping"
else
	{
		echo ""
		echo "$MARKER"
		cat "$DIR/bash_profile_snippet"
	} >> "$PROFILE"
	echo "Appended auto-startx snippet to $PROFILE"
fi

echo
echo "Done. Log out to a text console and back in on tty1 (or reboot) to"
echo "auto-start X into zovwm. Everything else (keybindings, appearance,"
echo "monitor mode) is set up by zovwm's own first-run wizards."
