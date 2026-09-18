/* Minimal dwm-style status bar: workspace indicators, the focused window's
 * title, and a clock. Drawn with core Xlib text (XDrawString) — no Xft or
 * Pango, so non-Latin window titles (e.g. Cyrillic) won't render correctly
 * with the default core font; workspace numbers and the clock are ASCII and
 * always fine. */
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "zovwm.h"
#include "config.h"

static Window barwin;
static GC gc;
static XFontStruct *font;
static unsigned long col_bg, col_fg, col_cur, col_occupied, col_empty;

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
workspace_occupied(int idx)
{
	for (Client *c = wm.clients; c; c = c->next)
		if (c->workspace == idx)
			return 1;
	return 0;
}

void
bar_init(void)
{
	XSetWindowAttributes wa;

	font = XLoadQueryFont(wm.dpy, barfont);
	if (!font)
		font = XLoadQueryFont(wm.dpy, "fixed");
	if (!font)
		die("zovwm: cannot load a core X font for the bar");

	col_bg       = getcolor(barcol_bg);
	col_fg       = getcolor(barcol_fg);
	col_cur      = getcolor(barcol_cur);
	col_occupied = getcolor(barcol_occupied);
	col_empty    = getcolor(barcol_empty);

	wa.override_redirect = True;
	wa.background_pixel = col_bg;
	wa.event_mask = ExposureMask;
	barwin = XCreateWindow(wm.dpy, wm.root, 0, 0, (unsigned int)wm.sw, BARHEIGHT, 0,
	                         DefaultDepth(wm.dpy, wm.screen), CopyFromParent,
	                         DefaultVisual(wm.dpy, wm.screen),
	                         CWOverrideRedirect | CWBackPixel | CWEventMask, &wa);
	gc = XCreateGC(wm.dpy, barwin, 0, NULL);
	XSetFont(wm.dpy, gc, font->fid);
	XMapRaised(wm.dpy, barwin);
	bar_draw();
}

void
bar_cleanup(void)
{
	XFreeGC(wm.dpy, gc);
	if (font)
		XFreeFont(wm.dpy, font);
	XDestroyWindow(wm.dpy, barwin);
}

void
bar_draw(void)
{
	char label[8], clockbuf[16];
	int x = 0, ty = (BARHEIGHT + font->ascent - font->descent) / 2;
	time_t t;
	struct tm *tmv;

	XSetForeground(wm.dpy, gc, col_bg);
	XFillRectangle(wm.dpy, barwin, gc, 0, 0, (unsigned int)wm.sw, BARHEIGHT);

	for (int i = 0; i < WSCOUNT; i++) {
		int segw = BARHEIGHT;
		unsigned long bg, fg;

		if (i == wm.curws) {
			bg = col_cur;
			fg = col_bg;
		} else if (workspace_occupied(i)) {
			bg = col_bg;
			fg = col_occupied;
		} else {
			bg = col_bg;
			fg = col_empty;
		}

		XSetForeground(wm.dpy, gc, bg);
		XFillRectangle(wm.dpy, barwin, gc, x, 0, (unsigned int)segw, BARHEIGHT);
		snprintf(label, sizeof label, "%d", i + 1);
		XSetForeground(wm.dpy, gc, fg);
		int lw = XTextWidth(font, label, (int)strlen(label));
		XDrawString(wm.dpy, barwin, gc, x + (segw - lw) / 2, ty, label, (int)strlen(label));
		x += segw;
	}
	x += 8;

	{
		static const char *symbols[LAYOUT_COUNT] = {
			[LAYOUT_TILE] = "[]=", [LAYOUT_MONOCLE] = "[M]", [LAYOUT_GRID] = "###",
		};
		const char *sym = symbols[wm.ws[wm.curws].layout];
		XSetForeground(wm.dpy, gc, col_fg);
		XDrawString(wm.dpy, barwin, gc, x, ty, sym, (int)strlen(sym));
		x += XTextWidth(font, sym, (int)strlen(sym)) + 8;
	}

	if (wm.focused) {
		char *name = NULL;
		if (XFetchName(wm.dpy, wm.focused->win, &name) && name) {
			XSetForeground(wm.dpy, gc, col_fg);
			XDrawString(wm.dpy, barwin, gc, x, ty, name, (int)strlen(name));
			XFree(name);
		}
	}

	t = time(NULL);
	tmv = localtime(&t);
	strftime(clockbuf, sizeof clockbuf, "%H:%M:%S", tmv);
	int cw = XTextWidth(font, clockbuf, (int)strlen(clockbuf));
	XSetForeground(wm.dpy, gc, col_fg);
	XDrawString(wm.dpy, barwin, gc, wm.sw - cw - 8, ty, clockbuf, (int)strlen(clockbuf));

	XFlush(wm.dpy);
}
