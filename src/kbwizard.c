/* Keyboard-layout wizard: first-run interactive window that lets the user
 * pick which keyboard layouts to use (e.g. us, ru, de, fr) and which key
 * combination toggles between them (Alt+Shift, Ctrl+Shift, etc.).
 *
 * Saves the choice to ~/.config/zovwm/kblayout.conf and applies it via
 * setxkbmap(1). On subsequent starts, kblayout_apply_saved() (in
 * kblayout.c) reads and re-applies the saved config automatically — the
 * wizard only runs once.
 *
 * Same bare-Xlib drawing style as wizard.c and monitorwizard.c. */
#define _POSIX_C_SOURCE 200809L

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <X11/XKBlib.h>
#include <X11/keysym.h>

#include "zovwm.h"

#define ROWH 20
#define MARGIN 16
#define WINW 640

/* Available layouts the wizard offers. */
static const struct {
	const char *code;
	const char *name;
} avail_layouts[] = {
	{"us", "English (US)"},
	{"ru", "Russian"},
	{"de", "German"},
	{"fr", "French"},
	{"es", "Spanish"},
	{"it", "Italian"},
	{"pt", "Portuguese"},
	{"ua", "Ukrainian"},
	{"kz", "Kazakh"},
	{"by", "Belarusian"},
	{"pl", "Polish"},
	{"cz", "Czech"},
	{"tr", "Turkish"},
	{"jp", "Japanese"},
	{"cn", "Chinese"},
	{"kr", "Korean"},
	{"ar", "Arabic"},
	{"gb", "English (UK)"},
	{"br", "Portuguese (Brazil)"},
	{"se", "Swedish"},
};
#define NLAYOUTS ((int)(sizeof(avail_layouts) / sizeof(avail_layouts[0])))
#define MAXSEL 4 /* max layouts the user can pick */

/* Available toggle options. */
static const struct {
	const char *label;
	const char *xkb_option;
} toggle_options[] = {
	{"Alt+Shift",    "grp:alt_shift_toggle"},
	{"Ctrl+Shift",   "grp:ctrl_shift_toggle"},
	{"Super+Space",  "grp:win_space_toggle"},
	{"CapsLock",     "grp:caps_toggle"},
};
#define NTOGGLES ((int)(sizeof(toggle_options) / sizeof(toggle_options[0])))

static Window win;
static GC gc;
static XFontStruct *font;
static unsigned long col_bg, col_fg, col_cur, col_hint, col_sel;

/* State */
static int phase; /* 0 = pick layouts, 1 = pick toggle, 2 = done */
static int cursor; /* cursor position within the current list */
static int selected[NLAYOUTS]; /* 1 if layout is selected */
static int nselected;
static int toggle_choice; /* index into toggle_options */
static int winheight;
static int scroll_offset; /* for layout list scrolling */

static unsigned long
getcolor(const char *name)
{
	XColor color;
	Colormap cmap = DefaultColormap(wm.dpy, wm.screen);
	if (!XAllocNamedColor(wm.dpy, cmap, name, &color, &color))
		return BlackPixel(wm.dpy, wm.screen);
	return color.pixel;
}

/* How many layout rows fit on screen at once */
static int
visible_rows(void)
{
	int avail = winheight - MARGIN * 2 - ROWH - 6 - ROWH * 3; /* title + footer */
	int rows = avail / ROWH;
	if (rows < 5) rows = 5;
	if (rows > NLAYOUTS) rows = NLAYOUTS;
	return rows;
}

static void
draw(void)
{
	char buf[128];
	int ty = MARGIN + font->ascent;

	XSetForeground(wm.dpy, gc, col_bg);
	XFillRectangle(wm.dpy, win, gc, 0, 0, WINW, (unsigned int)winheight);

	if (phase == 0) {
		/* Phase 0: pick layouts */
		XSetForeground(wm.dpy, gc, col_fg);
		const char *title = "zovwm setup - choose keyboard layouts (select 2 or more)";
		XDrawString(wm.dpy, win, gc, MARGIN, ty, title, (int)strlen(title));
		ty += ROWH + 6;

		int vrows = visible_rows();
		/* Adjust scroll so cursor is visible */
		if (cursor < scroll_offset)
			scroll_offset = cursor;
		if (cursor >= scroll_offset + vrows)
			scroll_offset = cursor - vrows + 1;

		for (int vi = 0; vi < vrows && (scroll_offset + vi) < NLAYOUTS; vi++) {
			int i = scroll_offset + vi;
			int rowy = ty + vi * ROWH;

			if (i == cursor) {
				XSetForeground(wm.dpy, gc, col_cur);
				XFillRectangle(wm.dpy, win, gc, MARGIN - 4, rowy - font->ascent - 2,
				                 WINW - 2 * (MARGIN - 4), ROWH);
				XSetForeground(wm.dpy, gc, col_bg);
			} else {
				XSetForeground(wm.dpy, gc, col_fg);
			}

			snprintf(buf, sizeof buf, "[%c] %-4s  %s",
			         selected[i] ? 'x' : ' ',
			         avail_layouts[i].code,
			         avail_layouts[i].name);
			XDrawString(wm.dpy, win, gc, MARGIN, rowy, buf, (int)strlen(buf));
		}

		int footy = ty + vrows * ROWH + ROWH;
		XSetForeground(wm.dpy, gc, col_hint);
		snprintf(buf, sizeof buf, "Selected: %d    Up/Down: navigate    Space/Enter: toggle    S: save & continue", nselected);
		XDrawString(wm.dpy, win, gc, MARGIN, footy, buf, (int)strlen(buf));
		if (scroll_offset > 0 || scroll_offset + vrows < NLAYOUTS) {
			snprintf(buf, sizeof buf, "(%d-%d of %d)    Esc: skip",
			         scroll_offset + 1, scroll_offset + vrows > NLAYOUTS ? NLAYOUTS : scroll_offset + vrows, NLAYOUTS);
			XDrawString(wm.dpy, win, gc, MARGIN, footy + ROWH, buf, (int)strlen(buf));
		} else {
			const char *hint2 = "Esc: skip (no layout change)";
			XDrawString(wm.dpy, win, gc, MARGIN, footy + ROWH, hint2, (int)strlen(hint2));
		}
	} else if (phase == 1) {
		/* Phase 1: pick toggle key */
		XSetForeground(wm.dpy, gc, col_fg);
		const char *title = "zovwm setup - choose layout toggle key";
		XDrawString(wm.dpy, win, gc, MARGIN, ty, title, (int)strlen(title));
		ty += ROWH + 6;

		/* Show selected layouts */
		XSetForeground(wm.dpy, gc, col_hint);
		snprintf(buf, sizeof buf, "Layouts: ");
		int first = 1;
		for (int i = 0; i < NLAYOUTS; i++) {
			if (!selected[i]) continue;
			if (!first) strncat(buf, ", ", sizeof(buf) - strlen(buf) - 1);
			strncat(buf, avail_layouts[i].code, sizeof(buf) - strlen(buf) - 1);
			first = 0;
		}
		XDrawString(wm.dpy, win, gc, MARGIN, ty, buf, (int)strlen(buf));
		ty += ROWH + 4;

		for (int i = 0; i < NTOGGLES; i++) {
			int rowy = ty + i * ROWH;
			if (i == cursor) {
				XSetForeground(wm.dpy, gc, col_cur);
				XFillRectangle(wm.dpy, win, gc, MARGIN - 4, rowy - font->ascent - 2,
				                 WINW - 2 * (MARGIN - 4), ROWH);
				XSetForeground(wm.dpy, gc, col_bg);
			} else {
				XSetForeground(wm.dpy, gc, col_fg);
			}
			snprintf(buf, sizeof buf, "  %s  (%s)",
			         toggle_options[i].label,
			         toggle_options[i].xkb_option);
			XDrawString(wm.dpy, win, gc, MARGIN, rowy, buf, (int)strlen(buf));
		}

		int footy = ty + NTOGGLES * ROWH + ROWH;
		XSetForeground(wm.dpy, gc, col_hint);
		const char *hint1 = "Up/Down: select    Enter/S: apply    Esc: skip";
		XDrawString(wm.dpy, win, gc, MARGIN, footy, hint1, (int)strlen(hint1));
	}

	XFlush(wm.dpy);
}

static char *
kblayout_configpath(void)
{
	static char path[512];
	const char *home = getenv("HOME");
	snprintf(path, sizeof path, "%s/.config/zovwm/kblayout.conf", home ? home : "/tmp");
	return path;
}

static void
save_and_apply(void)
{
	/* Build layout string like "us,ru" */
	char layouts[128] = "";
	int first = 1;
	for (int i = 0; i < NLAYOUTS; i++) {
		if (!selected[i]) continue;
		if (!first) strncat(layouts, ",", sizeof(layouts) - strlen(layouts) - 1);
		strncat(layouts, avail_layouts[i].code, sizeof(layouts) - strlen(layouts) - 1);
		first = 0;
	}

	const char *toggle = toggle_options[toggle_choice].xkb_option;

	/* Save to kblayout.conf */
	char *path = kblayout_configpath();

	/* mkdir -p */
	char dir[512];
	snprintf(dir, sizeof dir, "%s", path);
	char *slash = strrchr(dir, '/');
	if (slash)
		*slash = '\0';
	for (char *s = dir + 1; *s; s++) {
		if (*s == '/') {
			*s = '\0';
			mkdir(dir, 0755);
			*s = '/';
		}
	}
	mkdir(dir, 0755);

	FILE *f = fopen(path, "w");
	if (f) {
		fprintf(f, "# zovwm keyboard layout configuration\n");
		fprintf(f, "# Applied automatically on startup via setxkbmap.\n");
		fprintf(f, "# Delete this file to see the wizard again.\n");
		fprintf(f, "layouts %s\n", layouts);
		fprintf(f, "toggle %s\n", toggle);
		fclose(f);
	}

	/* Apply immediately via setxkbmap — execlp with a split argv rather
	 * than a shell string, matching kblayout_apply_saved()'s reasoning
	 * (kblayout.c) even though layouts/toggle are compile-time constants
	 * here, not file-sourced. */
	if (fork() == 0) {
		setsid();
		execlp("setxkbmap", "setxkbmap", "-layout", layouts,
		       "-option", "", "-option", toggle, (char *)NULL);
		_exit(1);
	}
}

int
kbwizard_run(void)
{
	XSetWindowAttributes wa;

	font = XLoadQueryFont(wm.dpy, cfg.bar_font);
	if (!font)
		font = XLoadQueryFont(wm.dpy, "fixed");
	if (!font)
		return 0;

	col_bg   = getcolor(cfg.bar_color_bg);
	col_fg   = getcolor(cfg.bar_color_fg);
	col_cur  = getcolor(cfg.bar_color_cur);
	col_hint = getcolor(cfg.bar_color_occupied);
	col_sel  = getcolor(cfg.bar_color_cur);

	/* Reset state */
	phase = 0;
	cursor = 0;
	nselected = 0;
	toggle_choice = 0;
	scroll_offset = 0;
	memset(selected, 0, sizeof selected);

	int winh = MARGIN * 2 + ROWH + 6 + NLAYOUTS * ROWH + ROWH * 3;
	if (winh > wm.sh - 40)
		winh = wm.sh - 40;
	winheight = winh;

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

	int done = 0, configured = 0;
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

		if (phase == 0) {
			/* Layout selection phase */
			switch (keysym) {
			case XK_Up:
			case XK_k:
				cursor = (cursor - 1 + NLAYOUTS) % NLAYOUTS;
				draw();
				break;
			case XK_Down:
			case XK_j:
				cursor = (cursor + 1) % NLAYOUTS;
				draw();
				break;
			case XK_space:
			case XK_Return:
				if (selected[cursor]) {
					selected[cursor] = 0;
					nselected--;
				} else if (nselected < MAXSEL) {
					selected[cursor] = 1;
					nselected++;
				}
				draw();
				break;
			case XK_s:
			case XK_S:
				if (nselected >= 2) {
					phase = 1;
					cursor = 0;
					draw();
				} else {
					XBell(wm.dpy, 0); /* need at least 2 */
				}
				break;
			case XK_Escape:
				done = 1;
				break;
			default:
				break;
			}
		} else if (phase == 1) {
			/* Toggle key selection phase */
			switch (keysym) {
			case XK_Up:
			case XK_k:
				cursor = (cursor - 1 + NTOGGLES) % NTOGGLES;
				draw();
				break;
			case XK_Down:
			case XK_j:
				cursor = (cursor + 1) % NTOGGLES;
				draw();
				break;
			case XK_Return:
			case XK_s:
			case XK_S:
				toggle_choice = cursor;
				save_and_apply();
				configured = 1;
				done = 1;
				break;
			case XK_Escape:
				done = 1;
				break;
			default:
				break;
			}
		}
	}

	XDestroyWindow(wm.dpy, win);
	XFreeGC(wm.dpy, gc);
	XFreeFont(wm.dpy, font);
	XSync(wm.dpy, False);
	return configured;
}
