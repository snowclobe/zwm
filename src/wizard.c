/* First-run keybinding wizard: shows every default bind and lets the user
 * remap any of them before zovwm grabs keys for real. Runs once, blocking,
 * from main.c's setup() — before bar_init()/grabkeys()/scan(), so there's
 * no WM behavior yet to interfere with. Drawn with the same bare-Xlib core
 * font approach as bar.c; no new dependencies. */
#include <stdio.h>
#include <string.h>
#include <X11/XKBlib.h>
#include <X11/keysym.h>

#include "zovwm.h"
#include "config.h"

#define ROWH 20
#define MARGIN 16
#define WINW 640

static Window win;
static GC gc;
static XFontStruct *font;
static unsigned long col_bg, col_fg, col_cur, col_hint;
static int sel;
static int capturing; /* 1 while waiting for the user to press a new combo */
static int winheight;

static unsigned long
getcolor(const char *name)
{
	XColor color;
	Colormap cmap = DefaultColormap(wm.dpy, wm.screen);
	if (!XAllocNamedColor(wm.dpy, cmap, name, &color, &color))
		return BlackPixel(wm.dpy, wm.screen);
	return color.pixel;
}

static int
issuperkeysym(KeySym ks)
{
	switch (ks) {
	case XK_Shift_L: case XK_Shift_R:
	case XK_Control_L: case XK_Control_R:
	case XK_Alt_L: case XK_Alt_R:
	case XK_Super_L: case XK_Super_R:
	case XK_Caps_Lock: case XK_Num_Lock:
	case XK_ISO_Level3_Shift:
		return 1;
	default:
		return 0;
	}
}

static void
draw(void)
{
	char buf[64];
	int n = keyconf_count();
	int ty = MARGIN + font->ascent;

	XSetForeground(wm.dpy, gc, col_bg);
	XFillRectangle(wm.dpy, win, gc, 0, 0, WINW, (unsigned int)winheight);

	XSetForeground(wm.dpy, gc, col_fg);
	const char *title = "zovwm setup - review your keybindings";
	XDrawString(wm.dpy, win, gc, MARGIN, ty, title, (int)strlen(title));
	ty += ROWH + 6;

	for (int i = 0; i < n; i++) {
		int rowy = ty + i * ROWH;
		if (i == sel) {
			XSetForeground(wm.dpy, gc, col_cur);
			XFillRectangle(wm.dpy, win, gc, MARGIN - 4, rowy - font->ascent - 2,
			                 WINW - 2 * (MARGIN - 4), ROWH);
			XSetForeground(wm.dpy, gc, col_bg);
		} else {
			XSetForeground(wm.dpy, gc, col_fg);
		}
		snprintf(buf, sizeof buf, "%-20s", keyconf_combo(i));
		XDrawString(wm.dpy, win, gc, MARGIN, rowy, buf, (int)strlen(buf));
		XDrawString(wm.dpy, win, gc, MARGIN + 220, rowy, keyconf_label(i), (int)strlen(keyconf_label(i)));
	}

	int footy = ty + n * ROWH + ROWH;
	XSetForeground(wm.dpy, gc, col_hint);
	if (capturing) {
		snprintf(buf, sizeof buf, "Press the new key combination for \"%s\"... (Esc to cancel)",
		         keyconf_label(sel));
		XDrawString(wm.dpy, win, gc, MARGIN, footy, buf, (int)strlen(buf));
	} else {
		const char *hint1 = "Up/Down or j/k: select    Enter: rebind selected";
		const char *hint2 = "S: save and continue    Esc: keep defaults and continue";
		XDrawString(wm.dpy, win, gc, MARGIN, footy, hint1, (int)strlen(hint1));
		XDrawString(wm.dpy, win, gc, MARGIN, footy + ROWH, hint2, (int)strlen(hint2));
	}

	XFlush(wm.dpy);
}

/* Blocks until a real (non-modifier) key is pressed, or Escape cancels.
 * Returns 1 and fills *combo on success, 0 if cancelled. */
static int
capturecombo(char *combo, size_t combosz)
{
	XEvent ev;

	capturing = 1;
	draw();

	XGrabKeyboard(wm.dpy, win, True, GrabModeAsync, GrabModeAsync, CurrentTime);
	for (;;) {
		XNextEvent(wm.dpy, &ev);
		if (ev.type == Expose) {
			draw();
			continue;
		}
		if (ev.type != KeyPress)
			continue;
		KeySym keysym = XkbKeycodeToKeysym(wm.dpy, (KeyCode)ev.xkey.keycode, 0, 0);
		if (issuperkeysym(keysym))
			continue;
		if (keysym == XK_Escape) {
			XUngrabKeyboard(wm.dpy, CurrentTime);
			capturing = 0;
			return 0;
		}
		const char *keyname = XKeysymToString(keysym);
		if (!keyname) {
			XBell(wm.dpy, 0);
			continue;
		}
		char mods[32] = "";
		if (ev.xkey.state & Mod4Mask)
			strcat(mods, "Super+");
		if (ev.xkey.state & ControlMask)
			strcat(mods, "Ctrl+");
		if (ev.xkey.state & Mod1Mask)
			strcat(mods, "Alt+");
		if (ev.xkey.state & ShiftMask)
			strcat(mods, "Shift+");
		snprintf(combo, combosz, "%s%s", mods, keyname);
		XUngrabKeyboard(wm.dpy, CurrentTime);
		capturing = 0;
		return 1;
	}
}

void
wizard_run(void)
{
	int n = keyconf_count();
	int winh = MARGIN * 2 + ROWH + 6 + n * ROWH + ROWH + 2 * ROWH;
	winheight = winh;
	XSetWindowAttributes wa;

	font = XLoadQueryFont(wm.dpy, barfont);
	if (!font)
		font = XLoadQueryFont(wm.dpy, "fixed");
	if (!font)
		return; /* no core font at all: skip the wizard, keep compiled defaults */

	col_bg = getcolor(barcol_bg);
	col_fg = getcolor(barcol_fg);
	col_cur = getcolor(barcol_cur);
	col_hint = getcolor(barcol_occupied);

	int x = (wm.sw - WINW) / 2;
	int y = (wm.sh - winh) / 2;
	if (x < 0) x = 0;
	if (y < 0) y = 0;

	wa.background_pixel = col_bg;
	wa.event_mask = KeyPressMask | ExposureMask;
	win = XCreateWindow(wm.dpy, wm.root, x, y, WINW, (unsigned int)winh, 1,
	                      DefaultDepth(wm.dpy, wm.screen), CopyFromParent,
	                      DefaultVisual(wm.dpy, wm.screen),
	                      CWBackPixel | CWEventMask, &wa);
	XSetWindowBorder(wm.dpy, win, col_cur);
	gc = XCreateGC(wm.dpy, win, 0, NULL);
	XSetFont(wm.dpy, gc, font->fid);
	XMapRaised(wm.dpy, win);
	XSetInputFocus(wm.dpy, win, RevertToPointerRoot, CurrentTime);

	sel = 0;
	int done = 0, save = 0;
	XEvent ev;
	while (!done) {
		XNextEvent(wm.dpy, &ev);
		if (ev.type == Expose) {
			draw();
			continue;
		}
		if (ev.type != KeyPress)
			continue;
		KeySym keysym = XkbKeycodeToKeysym(wm.dpy, (KeyCode)ev.xkey.keycode, 0, 0);
		switch (keysym) {
		case XK_Up:
		case XK_k:
			sel = (sel - 1 + n) % n;
			draw();
			break;
		case XK_Down:
		case XK_j:
			sel = (sel + 1) % n;
			draw();
			break;
		case XK_Return: {
			char combo[32];
			if (capturecombo(combo, sizeof combo))
				keyconf_set_combo(sel, combo);
			draw();
			break;
		}
		case XK_s:
		case XK_S:
			save = 1;
			done = 1;
			break;
		case XK_Escape:
			save = 0;
			done = 1;
			break;
		default:
			break;
		}
	}

	if (!save)
		keyconf_seed_defaults(); /* discard any in-progress remaps */
	keyconf_save();

	XDestroyWindow(wm.dpy, win);
	XFreeGC(wm.dpy, gc);
	XFreeFont(wm.dpy, font);
	XSync(wm.dpy, False);
}
