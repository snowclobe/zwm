/* zovwm static configuration — dwm-style compile-time config.
 * Edit and `make` to apply; a real config file is a later milestone
 * (see README roadmap). */
#ifndef ZOVWM_CONFIG_H
#define ZOVWM_CONFIG_H

#include <X11/keysym.h>
#include "zovwm.h"

/* --- appearance / layout --- */
#define MODKEY        Mod4Mask /* Super */
#define BORDERWIDTH   2
#define GAP           8
static const double default_mfact   = 0.55; /* master column width fraction */
static const int    default_nmaster = 1;
static const LayoutType default_layout = LAYOUT_TILE;
static const char col_focus[]   = "#5e81ac";
static const char col_unfocus[] = "#3b4252";

/* --- status bar --- */
#define BARHEIGHT 20
static const char barfont[]         = "fixed"; /* core X font, always present */
static const char barcol_bg[]       = "#2e3440";
static const char barcol_fg[]       = "#d8dee9";
static const char barcol_cur[]      = "#88c0d0"; /* current workspace */
static const char barcol_occupied[] = "#a3be8c"; /* has clients, not current */
static const char barcol_empty[]    = "#4c566a"; /* no clients */

/* Default terminal/launcher/rofi/wallpaper commands for keybindings now
 * live in src/keyconf.c's defaults table. The one unconditional startup
 * spawn (the wallpaper fetch) has its own command constant in main.c.
 *
 * Keyboard bindings are no longer compile-time: see src/keyconf.c for the
 * default table (also shown/editable in the first-run wizard, src/wizard.c)
 * and ~/.config/zovwm/keys.conf for the loaded/persisted result. */

/* --- mouse bindings (floating windows) ---
 * Dragging a tiled window with these also auto-floats it first. */
static const Button buttons[] = {
	/* modifier    button   function     argument */
	{ MODKEY,      Button1, movemouse,   {0} },
	{ MODKEY,      Button3, resizemouse, {0} },
};

#endif /* ZOVWM_CONFIG_H */
