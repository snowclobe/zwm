/* Keyboard-layout (XKB group) switching. zovwm doesn't configure layouts
 * itself — that's a session-level `setxkbmap -layout us,ru` call (see
 * dotfiles/xinitrc for an example) — this just cycles between whatever
 * layouts are already active, and reports the current one for the bar.
 *
 * Plain Xlib/XKB (X11/XKBlib.h, part of libX11 itself — no extra library
 * or link flag needed, same as the XkbKeycodeToKeysym() calls elsewhere
 * in this project). The layout names come straight from the root
 * window's _XKB_RULES_NAMES property rather than linking libxkbfile just
 * for that: it's a single STRING property holding five NUL-separated
 * fields — rules, model, layout, variant, options — written by
 * setxkbmap/loadkeys-style tools, and "layout" is a comma-separated list
 * whose Nth entry names XKB group N (e.g. "us,ru" for two groups). */
#define _POSIX_C_SOURCE 200809L

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <X11/Xatom.h>
#include <X11/XKBlib.h>

#include "zovwm.h"

#define MAXGROUPS 4
#define NAMELEN 16

static int xkb_available = -1; /* -1 = not checked yet */

static int
havexkb(void)
{
	if (xkb_available < 0) {
		int opcode, event, error;
		int major = XkbMajorVersion, minor = XkbMinorVersion;
		xkb_available = XkbQueryExtension(wm.dpy, &opcode, &event, &error, &major, &minor) ? 1 : 0;
	}
	return xkb_available;
}

/* Splits _XKB_RULES_NAMES's "layout" field on ','. Returns the number of
 * layouts found (0 if the property isn't set at all, e.g. no setxkbmap
 * call happened this session — a perfectly normal single-layout setup). */
static int
readlayouts(char layouts[][NAMELEN])
{
	Atom rules_atom, actual_type;
	int actual_format, n = 0;
	unsigned long nitems, bytes_after;
	unsigned char *prop = NULL;

	rules_atom = XInternAtom(wm.dpy, "_XKB_RULES_NAMES", False);
	if (XGetWindowProperty(wm.dpy, wm.root, rules_atom, 0, 1024, False,
	                         XA_STRING, &actual_type, &actual_format,
	                         &nitems, &bytes_after, &prop) != Success || !prop)
		return 0;

	/* rules, model, layout, variant, options — in that order */
	const char *fields[5] = {0};
	int fi = 0;
	const char *p = (const char *)prop;
	const char *end = (const char *)prop + nitems;
	while (p < end && fi < 5) {
		fields[fi++] = p;
		p += strlen(p) + 1;
	}

	if (fi >= 3 && fields[2] && *fields[2]) {
		char buf[256];
		snprintf(buf, sizeof buf, "%s", fields[2]);
		char *save = NULL;
		char *tok = strtok_r(buf, ",", &save);
		while (tok && n < MAXGROUPS) {
			snprintf(layouts[n], NAMELEN, "%s", tok);
			n++;
			tok = strtok_r(NULL, ",", &save);
		}
	}
	XFree(prop);
	return n;
}

void
kblayout_next(const Arg *arg)
{
	(void)arg;
	char layouts[MAXGROUPS][NAMELEN];
	XkbStateRec state;
	int n;

	if (!havexkb())
		return;
	n = readlayouts(layouts);
	if (n < 2)
		return; /* only one (or no) layout configured: nothing to switch to */
	if (!XkbGetState(wm.dpy, XkbUseCoreKbd, &state))
		return;

	XkbLockGroup(wm.dpy, XkbUseCoreKbd, (state.group + 1) % n);
	bar_draw(); /* reflect the change immediately, don't wait for the next redraw */
}

/* Fills buf with the current layout's short code, uppercased (e.g. "US",
 * "RU"), for the status bar. Empty string if XKB, the rules property, or
 * a real multi-layout setup isn't available — bar.c just skips drawing
 * it then, same as it already does for an unfocused/titleless window. */
void
kblayout_current(char *buf, size_t bufsz)
{
	char layouts[MAXGROUPS][NAMELEN];
	XkbStateRec state;
	int n, group;

	buf[0] = '\0';
	if (!havexkb())
		return;
	n = readlayouts(layouts);
	if (n < 2)
		return;
	if (!XkbGetState(wm.dpy, XkbUseCoreKbd, &state))
		return;

	group = state.group;
	if (group < 0 || group >= n)
		group = 0;

	snprintf(buf, bufsz, "%s", layouts[group]);
	for (char *c = buf; *c; c++)
		*c = (char)toupper((unsigned char)*c);
}
