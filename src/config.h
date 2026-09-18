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

/* --- programs --- */
static const char *termcmd[]   = { "xterm", NULL };
static const char *launchcmd[] = { "dmenu_run", NULL };
static const char *roficmd[]   = { "rofi", "-show", "drun", NULL };
/* Fetches a random wallpaper from Wallhaven's open API and sets it (see
 * rust/zovwm-wallpaper). Also run once at startup, below in main.c. */
static const char *wallpapercmd[] = { "zovwm-wallpaper", NULL };

/* --- keybindings --- */
#define TAGKEYS(KEY, WSIDX) \
	{ MODKEY,            KEY, view, {.ui = (WSIDX)} }, \
	{ MODKEY|ShiftMask,  KEY, tag,  {.ui = (WSIDX)} },

static const Key keys[] = {
	/* modifier             key          function         argument */
	{ MODKEY,               XK_Return,   spawn,           {.v = termcmd} },
	{ MODKEY,               XK_p,        spawn,           {.v = launchcmd} },
	{ MODKEY,               XK_d,        spawn,           {.v = roficmd} },
	{ MODKEY,               XK_j,        focusstack,      {.i = +1} },
	{ MODKEY,               XK_k,        focusstack,      {.i = -1} },
	{ MODKEY|ShiftMask,     XK_j,        movestack,       {.i = +1} },
	{ MODKEY|ShiftMask,     XK_k,        movestack,       {.i = -1} },
	{ MODKEY,               XK_h,        setmfact,        {.f = -0.05f} },
	{ MODKEY,               XK_l,        setmfact,        {.f = +0.05f} },
	{ MODKEY,               XK_f,        togglefloating,  {0} },
	{ MODKEY,               XK_t,        setlayout,       {.i = LAYOUT_TILE} },
	{ MODKEY,               XK_m,        setlayout,       {.i = LAYOUT_MONOCLE} },
	{ MODKEY,               XK_g,        setlayout,       {.i = LAYOUT_GRID} },
	{ MODKEY,               XK_w,        spawn,           {.v = wallpapercmd} },
	{ MODKEY|ShiftMask,     XK_q,        killclient,      {0} },
	{ MODKEY|ShiftMask,     XK_e,        quit,            {0} },
	TAGKEYS(XK_1, 0)
	TAGKEYS(XK_2, 1)
	TAGKEYS(XK_3, 2)
	TAGKEYS(XK_4, 3)
	TAGKEYS(XK_5, 4)
	TAGKEYS(XK_6, 5)
	TAGKEYS(XK_7, 6)
	TAGKEYS(XK_8, 7)
	TAGKEYS(XK_9, 8)
};

/* --- mouse bindings (floating windows) ---
 * Dragging a tiled window with these also auto-floats it first. */
static const Button buttons[] = {
	/* modifier    button   function     argument */
	{ MODKEY,      Button1, movemouse,   {0} },
	{ MODKEY,      Button3, resizemouse, {0} },
};

#endif /* ZOVWM_CONFIG_H */
