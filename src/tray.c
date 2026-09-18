/* XEmbed system tray host, docked into the right side of the status bar
 * (bar.c's barwin, reused as the tray's manager window — no extra window
 * needed). Implements the standard freedesktop system tray protocol:
 * https://specifications.freedesktop.org/systemtray-spec/systemtray-spec-latest.html
 *
 * This is the one place in zovwm that reparents a window — XEmbed
 * fundamentally requires it (the icon becomes a child of the tray), unlike
 * every regular client window, which zovwm tiles in place without ever
 * reparenting it.
 *
 * Best-effort: XEmbed is a real but fiddly protocol, and even mature tray
 * implementations don't work with every possible tray-icon app. This
 * implements the protocol correctly; it does not attempt to special-case
 * every app's quirks. */
#include <stdio.h>
#include <X11/Xatom.h>

#include "zovwm.h"

#define MAXTRAY 16
#define ICONGAP 4

#define SYSTEM_TRAY_REQUEST_DOCK 0
#define XEMBED_EMBEDDED_NOTIFY 0
#define XEMBED_VERSION 0

static Atom netsystemtray, systemtray_opcode, manager_atom, xembed_atom;
static Window icons[MAXTRAY];
static int nicons;
static int active; /* 1 once we actually own the tray selection */

static int
iconsize(void)
{
	return cfg.bar_height > 4 ? cfg.bar_height - 4 : cfg.bar_height;
}

static void
tray_arrange(void)
{
	int size = iconsize();
	int rightedge = wm.sw - bar_right_reserved() - ICONGAP;
	int x = rightedge - nicons * (size + ICONGAP);
	int y = (cfg.bar_height - size) / 2;

	for (int i = 0; i < nicons; i++) {
		XMoveResizeWindow(wm.dpy, icons[i], x, y, (unsigned int)size, (unsigned int)size);
		x += size + ICONGAP;
	}
}

int
tray_width(void)
{
	if (nicons == 0)
		return 0;
	return nicons * (iconsize() + ICONGAP) + ICONGAP;
}

void
tray_init(void)
{
	char selname[32];
	XClientMessageEvent manager_notify = {0};

	snprintf(selname, sizeof selname, "_NET_SYSTEM_TRAY_S%d", wm.screen);
	netsystemtray = XInternAtom(wm.dpy, selname, False);
	systemtray_opcode = XInternAtom(wm.dpy, "_NET_SYSTEM_TRAY_OPCODE", False);
	manager_atom = XInternAtom(wm.dpy, "MANAGER", False);
	xembed_atom = XInternAtom(wm.dpy, "_XEMBED", False);

	if (XGetSelectionOwner(wm.dpy, netsystemtray) != None) {
		fprintf(stderr, "zovwm: another system tray is already running, skipping tray support\n");
		return;
	}

	XSetSelectionOwner(wm.dpy, netsystemtray, bar_window(), CurrentTime);
	if (XGetSelectionOwner(wm.dpy, netsystemtray) != bar_window())
		return; /* lost a race for the selection; give up quietly */

	/* No need to select SubstructureNotifyMask on the bar window itself:
	 * dock() selects StructureNotifyMask directly on each icon window,
	 * which is enough to catch its own UnmapNotify/DestroyNotify. Doing
	 * it here too would risk clobbering the ExposureMask bar_init()
	 * already set (XChangeWindowAttributes replaces, not ORs). */

	long orientation = 0; /* horizontal */
	XChangeProperty(wm.dpy, bar_window(),
	                 XInternAtom(wm.dpy, "_NET_SYSTEM_TRAY_ORIENTATION", False),
	                 XA_CARDINAL, 32, PropModeReplace, (unsigned char *)&orientation, 1);
	long visualid = (long)XVisualIDFromVisual(DefaultVisual(wm.dpy, wm.screen));
	XChangeProperty(wm.dpy, bar_window(),
	                 XInternAtom(wm.dpy, "_NET_SYSTEM_TRAY_VISUAL", False),
	                 XA_VISUALID, 32, PropModeReplace, (unsigned char *)&visualid, 1);

	manager_notify.type = ClientMessage;
	manager_notify.window = wm.root;
	manager_notify.message_type = manager_atom;
	manager_notify.format = 32;
	manager_notify.data.l[0] = (long)CurrentTime;
	manager_notify.data.l[1] = (long)netsystemtray;
	manager_notify.data.l[2] = (long)bar_window();
	XSendEvent(wm.dpy, wm.root, False, StructureNotifyMask, (XEvent *)&manager_notify);

	active = 1;
}

void
tray_cleanup(void)
{
	if (!active)
		return;
	XSetSelectionOwner(wm.dpy, netsystemtray, None, CurrentTime);
}

static void
dock(Window icon)
{
	if (nicons >= MAXTRAY)
		return;

	XSelectInput(wm.dpy, icon, StructureNotifyMask | PropertyChangeMask);
	XReparentWindow(wm.dpy, icon, bar_window(), 0, 0);
	XMapWindow(wm.dpy, icon);

	XEvent embedded = {0};
	embedded.xclient.type = ClientMessage;
	embedded.xclient.window = icon;
	embedded.xclient.message_type = xembed_atom;
	embedded.xclient.format = 32;
	embedded.xclient.data.l[0] = (long)CurrentTime;
	embedded.xclient.data.l[1] = XEMBED_EMBEDDED_NOTIFY;
	embedded.xclient.data.l[2] = 0;
	embedded.xclient.data.l[3] = (long)bar_window();
	embedded.xclient.data.l[4] = XEMBED_VERSION;
	XSendEvent(wm.dpy, icon, False, NoEventMask, &embedded);

	icons[nicons++] = icon;
	tray_arrange();
	bar_draw();
}

void
tray_handle_clientmessage(XEvent *ev)
{
	XClientMessageEvent *cm = &ev->xclient;

	if (!active || cm->message_type != systemtray_opcode)
		return;
	if (cm->data.l[1] == SYSTEM_TRAY_REQUEST_DOCK)
		dock((Window)cm->data.l[2]);
}

static int
indexof(Window w)
{
	for (int i = 0; i < nicons; i++)
		if (icons[i] == w)
			return i;
	return -1;
}

static void
undock(int i)
{
	for (int j = i; j < nicons - 1; j++)
		icons[j] = icons[j + 1];
	nicons--;
	tray_arrange();
	bar_draw();
}

void
tray_handle_unmap(Window w)
{
	int i = indexof(w);
	if (i >= 0)
		undock(i);
}

void
tray_handle_destroy(Window w)
{
	int i = indexof(w);
	if (i >= 0)
		undock(i);
}
