/* zovwm static configuration — dwm-style compile-time config.
 * Edit and `make` to apply; a real config file is a later milestone
 * (see README roadmap). */
#ifndef ZOVWM_CONFIG_H
#define ZOVWM_CONFIG_H

#include <X11/keysym.h>
#include "zovwm.h"

/* --- appearance / layout ---
 * GAP, border width/colors, bar height/font/colors, and the default
 * layout/master-ratio are no longer compile-time: see src/appconf.c for
 * the defaults and ~/.config/zovwm/zovwm.conf for the loaded/persisted
 * result (the global `cfg`, declared in zovwm.h). MODKEY and the mouse
 * bindings below stay compile-time — remapping those wasn't asked for. */
#define MODKEY Mod4Mask /* Super */

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
