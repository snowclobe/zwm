/* First-run monitor setup wizard: for every connected output, lists its
 * available resolution+refresh-rate modes (queried via RandR) and lets the
 * user pick one, applying it immediately via xrandr(1) so the pick is seen
 * before it's saved. Same bare-Xlib list-menu approach as wizard.c (the
 * keybinding wizard). Runs once; see monitorconf.c for how the pick is
 * re-applied on every later startup. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <X11/keysym.h>
#include <X11/XKBlib.h>
#include <X11/extensions/Xrandr.h>

#include "zovwm.h"

#define ROWH 20
#define MARGIN 16
#define WINW 520
#define MAXMODES 64
#define MAXOUTPUTS 16

typedef struct {
	char label[48]; /* "1920x1080 @ 60Hz", shown in the list */
	char mode[32];  /* "1920x1080", for xrandr --mode */
	char rate[16];  /* "60", for xrandr --rate */
} ModeChoice;

static Window win;
static GC gc;
static XFontStruct *font;
static unsigned long col_bg, col_fg, col_cur, col_hint;
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

/* Standard xrandr refresh-rate formula: dotClock (Hz) / (hTotal * vTotal),
 * adjusted for interlace/doublescan. */
static double
moderate(const XRRModeInfo *m)
{
	double rate;

	if (!m->hTotal || !m->vTotal)
		return 0.0;
	rate = (double)m->dotClock / ((double)m->hTotal * (double)m->vTotal);
	if (m->modeFlags & RR_DoubleScan)
		rate /= 2.0;
	if (m->modeFlags & RR_Interlace)
		rate *= 2.0;
	return rate;
}

/* Fills choices[] with every unique "WxH @ rate" this output supports,
 * highest resolution and refresh rate first. Returns the count. */
static int
outputmodes(XRRScreenResources *sr, XRROutputInfo *oi, ModeChoice *choices)
{
	int n = 0;

	for (int i = 0; i < oi->nmode && n < MAXMODES; i++) {
		XRRModeInfo *mi = NULL;
		for (int j = 0; j < sr->nmode; j++)
			if (sr->modes[j].id == oi->modes[i]) {
				mi = &sr->modes[j];
				break;
			}
		if (!mi)
			continue;

		int rrate = (int)(moderate(mi) + 0.5);
		char modestr[32];
		snprintf(modestr, sizeof modestr, "%ux%u", mi->width, mi->height);

		int dup = 0;
		for (int k = 0; k < n; k++)
			if (strcmp(choices[k].mode, modestr) == 0 && atoi(choices[k].rate) == rrate) {
				dup = 1;
				break;
			}
		if (dup)
			continue;

		snprintf(choices[n].mode, sizeof choices[n].mode, "%s", modestr);
		snprintf(choices[n].rate, sizeof choices[n].rate, "%d", rrate);
		snprintf(choices[n].label, sizeof choices[n].label, "%s @ %dHz", modestr, rrate);
		n++;
	}

	for (int i = 0; i < n; i++)
		for (int j = i + 1; j < n; j++) {
			unsigned wi, hi, wj, hj;
			sscanf(choices[i].mode, "%ux%u", &wi, &hi);
			sscanf(choices[j].mode, "%ux%u", &wj, &hj);
			long ai = (long)wi * hi, aj = (long)wj * hj;
			if (ai < aj || (ai == aj && atoi(choices[i].rate) < atoi(choices[j].rate))) {
				ModeChoice t = choices[i];
				choices[i] = choices[j];
				choices[j] = t;
			}
		}
	return n;
}

static void
draw(const char *outputname, ModeChoice *choices, int n, int sel)
{
	char buf[96];
	int ty = MARGIN + font->ascent;

	XSetForeground(wm.dpy, gc, col_bg);
	XFillRectangle(wm.dpy, win, gc, 0, 0, WINW, (unsigned int)winheight);

	XSetForeground(wm.dpy, gc, col_fg);
	snprintf(buf, sizeof buf, "zovwm setup - display %s", outputname);
	XDrawString(wm.dpy, win, gc, MARGIN, ty, buf, (int)strlen(buf));
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
		XDrawString(wm.dpy, win, gc, MARGIN, rowy, choices[i].label, (int)strlen(choices[i].label));
	}

	int footy = ty + n * ROWH + ROWH;
	XSetForeground(wm.dpy, gc, col_hint);
	const char *hint1 = "Up/Down or j/k: select    Enter: apply and continue";
	const char *hint2 = "Esc: skip this display, keep its current mode";
	XDrawString(wm.dpy, win, gc, MARGIN, footy, hint1, (int)strlen(hint1));
	XDrawString(wm.dpy, win, gc, MARGIN, footy + ROWH, hint2, (int)strlen(hint2));

	XFlush(wm.dpy);
}

static void
writemonitorconf(char lines[][160], int n)
{
	char dir[512], path[512];
	const char *home = getenv("HOME");
	FILE *f;

	snprintf(dir, sizeof dir, "%s/.config/zovwm", home ? home : "/tmp");
	mkdir(dir, 0755); /* EEXIST is fine */
	snprintf(path, sizeof path, "%s/.config/zovwm/monitor.conf", home ? home : "/tmp");

	f = fopen(path, "w");
	if (!f)
		return;
	for (int i = 0; i < n; i++)
		fprintf(f, "%s\n", lines[i]);
	fclose(f);
}

int
monitorwizard_run(void)
{
	int rr_event_base, rr_error_base;
	XRRScreenResources *sr;
	char changedlines[MAXOUTPUTS][160];
	int nchanged = 0, anychange = 0;

	font = XLoadQueryFont(wm.dpy, cfg.bar_font);
	if (!font)
		font = XLoadQueryFont(wm.dpy, "fixed");
	if (!font || !XRRQueryExtension(wm.dpy, &rr_event_base, &rr_error_base)) {
		if (font)
			XFreeFont(wm.dpy, font);
		writemonitorconf(NULL, 0); /* no font or no RandR: skip silently, don't retry every boot */
		return 0;
	}

	sr = XRRGetScreenResources(wm.dpy, wm.root);
	if (!sr || sr->noutput == 0) {
		if (sr)
			XRRFreeScreenResources(sr);
		XFreeFont(wm.dpy, font);
		writemonitorconf(NULL, 0);
		return 0;
	}

	col_bg = getcolor(cfg.bar_color_bg);
	col_fg = getcolor(cfg.bar_color_fg);
	col_cur = getcolor(cfg.bar_color_cur);
	col_hint = getcolor(cfg.bar_color_occupied);

	XSetWindowAttributes wa;
	wa.background_pixel = col_bg;
	wa.event_mask = KeyPressMask | ExposureMask;
	win = XCreateWindow(wm.dpy, wm.root, 0, 0, WINW, ROWH * 4, 1,
	                      DefaultDepth(wm.dpy, wm.screen), CopyFromParent,
	                      DefaultVisual(wm.dpy, wm.screen),
	                      CWBackPixel | CWEventMask, &wa);
	XSetWindowBorder(wm.dpy, win, col_cur);
	gc = XCreateGC(wm.dpy, win, 0, NULL);
	XSetFont(wm.dpy, gc, font->fid);

	for (int oidx = 0; oidx < sr->noutput; oidx++) {
		XRROutputInfo *oi = XRRGetOutputInfo(wm.dpy, sr, sr->outputs[oidx]);
		if (!oi)
			continue;
		if (oi->connection != RR_Connected || oi->nmode == 0) {
			XRRFreeOutputInfo(oi);
			continue;
		}

		ModeChoice choices[MAXMODES];
		int n = outputmodes(sr, oi, choices);
		if (n == 0) {
			XRRFreeOutputInfo(oi);
			continue;
		}

		winheight = MARGIN * 2 + ROWH + 6 + n * ROWH + ROWH + 2 * ROWH;
		int x = (wm.sw - WINW) / 2;
		int y = (wm.sh - winheight) / 2;
		if (x < 0) x = 0;
		if (y < 0) y = 0;
		XMoveResizeWindow(wm.dpy, win, x, y, WINW, (unsigned int)winheight);
		XMapRaised(wm.dpy, win);
		XSetInputFocus(wm.dpy, win, RevertToPointerRoot, CurrentTime);

		int sel = 0, done = 0, picked = 0;
		draw(oi->name, choices, n, sel);
		XEvent ev;
		while (!done) {
			XNextEvent(wm.dpy, &ev);
			if (ev.type == Expose) {
				draw(oi->name, choices, n, sel);
				continue;
			}
			if (ev.type != KeyPress)
				continue;
			KeySym keysym = XkbKeycodeToKeysym(wm.dpy, (KeyCode)ev.xkey.keycode, 0, 0);
			switch (keysym) {
			case XK_Up:
			case XK_k:
				sel = (sel - 1 + n) % n;
				draw(oi->name, choices, n, sel);
				break;
			case XK_Down:
			case XK_j:
				sel = (sel + 1) % n;
				draw(oi->name, choices, n, sel);
				break;
			case XK_Return:
				monitorconf_set(oi->name, choices[sel].mode, choices[sel].rate);
				picked = 1;
				done = 1;
				break;
			case XK_Escape:
				done = 1;
				break;
			default:
				break;
			}
		}

		if (picked && nchanged < MAXOUTPUTS) {
			snprintf(changedlines[nchanged], sizeof changedlines[nchanged], "%s %s %s",
			         oi->name, choices[sel].mode, choices[sel].rate);
			nchanged++;
			anychange = 1;
		}

		XRRFreeOutputInfo(oi);
	}

	XDestroyWindow(wm.dpy, win);
	XFreeGC(wm.dpy, gc);
	XFreeFont(wm.dpy, font);
	XRRFreeScreenResources(sr);
	XSync(wm.dpy, False);

	writemonitorconf(changedlines, nchanged);
	return anychange;
}
