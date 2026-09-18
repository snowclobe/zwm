/* Power menu: Reboot / Shutdown / Sleep / Logout. Same bare-Xlib modal
 * window approach as wizard.c, reusing cfg's bar font/colors for visual
 * consistency. Bound to "power_menu" in keyconf.c (default Super+Shift+p). */
#include <string.h>
#include <X11/XKBlib.h>
#include <X11/keysym.h>

#include "zovwm.h"

#define ROWH 22
#define MARGIN 20
#define WINW 260

typedef struct {
	const char *label;
	const char *const *argv; /* NULL => logout (call quit() directly) */
} PowerItem;

static const char *rebootcmd[]   = {"systemctl", "reboot", NULL};
static const char *poweroffcmd[] = {"systemctl", "poweroff", NULL};
static const char *suspendcmd[]  = {"systemctl", "suspend", NULL};

static const PowerItem items[] = {
	{"Reboot",   rebootcmd},
	{"Shutdown", poweroffcmd},
	{"Sleep",    suspendcmd},
	{"Logout",   NULL},
};

static Window win;
static GC gc;
static XFontStruct *font;
static unsigned long col_bg, col_fg, col_cur;

static unsigned long
getcolor(const char *name)
{
	XColor color;
	Colormap cmap = DefaultColormap(wm.dpy, wm.screen);
	if (!XAllocNamedColor(wm.dpy, cmap, name, &color, &color))
		return BlackPixel(wm.dpy, wm.screen);
	return color.pixel;
}

static void
draw(int sel, int winh)
{
	int ty = MARGIN + font->ascent;

	XSetForeground(wm.dpy, gc, col_bg);
	XFillRectangle(wm.dpy, win, gc, 0, 0, WINW, (unsigned int)winh);

	for (unsigned int i = 0; i < LENGTH(items); i++) {
		int rowy = ty + (int)i * ROWH;
		if ((int)i == sel) {
			XSetForeground(wm.dpy, gc, col_cur);
			XFillRectangle(wm.dpy, win, gc, MARGIN - 6, rowy - font->ascent - 2,
			                 WINW - 2 * (MARGIN - 6), ROWH);
			XSetForeground(wm.dpy, gc, col_bg);
		} else {
			XSetForeground(wm.dpy, gc, col_fg);
		}
		XDrawString(wm.dpy, win, gc, MARGIN, rowy, items[i].label, (int)strlen(items[i].label));
	}

	XFlush(wm.dpy);
}

void
powermenu_run(const Arg *arg)
{
	(void)arg;
	int n = (int)LENGTH(items);
	int winh = MARGIN * 2 + n * ROWH;
	XSetWindowAttributes wa;

	font = XLoadQueryFont(wm.dpy, cfg.bar_font);
	if (!font)
		font = XLoadQueryFont(wm.dpy, "fixed");
	if (!font)
		return;

	col_bg = getcolor(cfg.bar_color_bg);
	col_fg = getcolor(cfg.bar_color_fg);
	col_cur = getcolor(cfg.bar_color_cur);

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
	XGrabKeyboard(wm.dpy, win, True, GrabModeAsync, GrabModeAsync, CurrentTime);

	int sel = 3; /* default to Logout, the least destructive option */
	int done = 0;
	XEvent ev;
	while (!done) {
		XNextEvent(wm.dpy, &ev);
		if (ev.type == Expose) {
			draw(sel, winh);
			continue;
		}
		if (ev.type != KeyPress)
			continue;
		KeySym keysym = XkbKeycodeToKeysym(wm.dpy, (KeyCode)ev.xkey.keycode, 0, 0);
		switch (keysym) {
		case XK_Up:
		case XK_k:
			sel = (sel - 1 + n) % n;
			draw(sel, winh);
			break;
		case XK_Down:
		case XK_j:
			sel = (sel + 1) % n;
			draw(sel, winh);
			break;
		case XK_Return:
			if (items[sel].argv) {
				Arg a = {.v = items[sel].argv};
				spawn(&a);
			} else {
				quit(NULL);
			}
			done = 1;
			break;
		case XK_Escape:
			done = 1;
			break;
		default:
			break;
		}
	}

	XUngrabKeyboard(wm.dpy, CurrentTime);
	XDestroyWindow(wm.dpy, win);
	XFreeGC(wm.dpy, gc);
	XFreeFont(wm.dpy, font);
	XFlush(wm.dpy);
}
