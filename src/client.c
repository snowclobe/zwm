/* Client (managed window) lifecycle: manage/unmanage, focus, floating,
 * workspace assignment, and the small stack-reordering helpers used by the
 * keybindings in config.h. */
#include <stdlib.h>
#include <X11/Xutil.h>

#include "zovwm.h"
#include "config.h"

static unsigned long
getcolor(const char *name)
{
	XColor color;
	Colormap cmap = DefaultColormap(wm.dpy, wm.screen);
	if (!XAllocNamedColor(wm.dpy, cmap, name, &color, &color))
		return BlackPixel(wm.dpy, wm.screen);
	return color.pixel;
}

#define MOUSEMASK (ButtonPressMask | ButtonReleaseMask | PointerMotionMask)

static int
getrootptr(int *x, int *y)
{
	Window dw1, dw2;
	int di1, di2;
	unsigned int dui;
	return XQueryPointer(wm.dpy, wm.root, &dw1, &dw2, x, y, &di1, &di2, &dui);
}

static void
grabbuttons(Client *c)
{
	static const unsigned int lockmods[] = {0, LockMask, Mod2Mask, LockMask | Mod2Mask};

	XUngrabButton(wm.dpy, AnyButton, AnyModifier, c->win);
	for (unsigned int i = 0; i < LENGTH(buttons); i++)
		for (unsigned int j = 0; j < LENGTH(lockmods); j++)
			XGrabButton(wm.dpy, buttons[i].button, buttons[i].mod | lockmods[j],
			             c->win, False, ButtonPressMask, GrabModeAsync, GrabModeAsync,
			             None, None);
}

/* Fills buf (capacity cap) with clients on the current workspace, in list
 * order. Returns the number of clients written. */
static int
wsclients(Client **buf, int cap)
{
	int n = 0;
	for (Client *c = wm.clients; c && n < cap; c = c->next)
		if (c->workspace == wm.curws)
			buf[n++] = c;
	return n;
}

static int
sendevent(Client *c, Atom proto)
{
	int n, exists = 0;
	Atom *protocols;

	if (XGetWMProtocols(wm.dpy, c->win, &protocols, &n)) {
		for (int i = 0; i < n && !exists; i++)
			exists = (protocols[i] == proto);
		XFree(protocols);
	}
	if (exists) {
		XEvent ev = {0};
		ev.type = ClientMessage;
		ev.xclient.window = c->win;
		ev.xclient.message_type = wm.wm_protocols;
		ev.xclient.format = 32;
		ev.xclient.data.l[0] = (long)proto;
		ev.xclient.data.l[1] = CurrentTime;
		XSendEvent(wm.dpy, c->win, False, NoEventMask, &ev);
	}
	return exists;
}

Client *
wintoclient(Window w)
{
	for (Client *c = wm.clients; c; c = c->next)
		if (c->win == w)
			return c;
	return NULL;
}

void
manage(Window w)
{
	Client *c;
	XWindowAttributes wa;
	Window trans = None;

	if (!XGetWindowAttributes(wm.dpy, w, &wa))
		return;
	if (wa.override_redirect || wintoclient(w))
		return;

	c = calloc(1, sizeof(Client));
	if (!c)
		die("zovwm: calloc failed");

	c->win = w;
	c->workspace = wm.curws;
	c->bw = cfg.border_width;
	c->x = wa.x;
	c->y = wa.y;
	c->w = wa.width;
	c->h = wa.height;

	if (XGetTransientForHint(wm.dpy, w, &trans) && trans != None)
		c->floating = 1;

	XSetWindowBorderWidth(wm.dpy, w, c->bw);
	XSetWindowBorder(wm.dpy, w, getcolor(cfg.color_unfocus));
	XSelectInput(wm.dpy, w, EnterWindowMask | FocusChangeMask |
	                         PropertyChangeMask | StructureNotifyMask);

	/* Append rather than prepend: tiling order is list order, and slot 0
	 * is always the master. Prepending would make every newly opened
	 * window instantly become master, displacing whatever you were
	 * already working in — appending joins the bottom of the stack
	 * instead, leaving the existing layout undisturbed. The new window
	 * still gets focus() below regardless of where it landed. */
	c->next = NULL;
	if (wm.clients) {
		Client *last = wm.clients;
		while (last->next)
			last = last->next;
		last->next = c;
	} else {
		wm.clients = c;
	}

	grabbuttons(c);
	XMapWindow(wm.dpy, w);
	arrange();
	focus(c);
}

void
unmanage(Client *c, int destroyed)
{
	Client **tc;

	if (!destroyed) {
		XGrabServer(wm.dpy);
		XSetErrorHandler(xerrordummy);
		XSelectInput(wm.dpy, c->win, NoEventMask);
		XUngrabServer(wm.dpy);
	}

	for (tc = &wm.clients; *tc && *tc != c; tc = &(*tc)->next)
		;
	if (*tc)
		*tc = c->next;
	if (wm.focused == c)
		wm.focused = NULL;
	free(c);

	if (!destroyed)
		XSetErrorHandler(xerror);

	arrange();
	focus(NULL);
}

void
resizeclient(Client *c, int x, int y, int w, int h)
{
	c->x = x;
	c->y = y;
	c->w = w;
	c->h = h;
	XMoveResizeWindow(wm.dpy, c->win, x, y,
	                   MAX(w - 2 * c->bw, 1), MAX(h - 2 * c->bw, 1));
}

void
showhideworkspace(void)
{
	/* Windows on other workspaces stay mapped but are parked off-screen;
	 * this sidesteps UnmapNotify bookkeeping entirely (same trick dwm
	 * uses for its tags). */
	for (Client *c = wm.clients; c; c = c->next) {
		if (c->workspace == wm.curws)
			XMoveWindow(wm.dpy, c->win, c->x, c->y);
		else
			XMoveWindow(wm.dpy, c->win, -(c->w + 2 * c->bw) - 100, c->y);
	}
}

void
unfocus(Client *c, int setfocus)
{
	if (!c)
		return;
	XSetWindowBorder(wm.dpy, c->win, getcolor(cfg.color_unfocus));
	if (setfocus)
		XSetInputFocus(wm.dpy, wm.root, RevertToPointerRoot, CurrentTime);
}

void
focus(Client *c)
{
	if (c && c->workspace != wm.curws)
		c = NULL;
	if (!c) {
		for (Client *i = wm.clients; i; i = i->next)
			if (i->workspace == wm.curws) {
				c = i;
				break;
			}
	}
	if (wm.focused && wm.focused != c)
		unfocus(wm.focused, 0);
	if (c) {
		XSetWindowBorder(wm.dpy, c->win, getcolor(cfg.color_focus));
		XSetInputFocus(wm.dpy, c->win, RevertToPointerRoot, CurrentTime);
		if (c->floating)
			XRaiseWindow(wm.dpy, c->win);
	} else {
		XSetInputFocus(wm.dpy, wm.root, RevertToPointerRoot, CurrentTime);
	}
	wm.focused = c;
	bar_draw();
}

void
focusstack(const Arg *arg)
{
	Client *buf[256];
	int n = wsclients(buf, 256);
	int cur = 0, next;

	if (n < 2)
		return;
	for (int i = 0; i < n; i++)
		if (buf[i] == wm.focused) {
			cur = i;
			break;
		}
	next = ((cur + arg->i) % n + n) % n;
	focus(buf[next]);
}

void
movestack(const Arg *arg)
{
	Client *buf[256];
	int n = wsclients(buf, 256);
	int cur = -1, other;
	Client *a, *b;
	Window tw;
	int tf;

	if (n < 2 || !wm.focused)
		return;
	for (int i = 0; i < n; i++)
		if (buf[i] == wm.focused) {
			cur = i;
			break;
		}
	if (cur < 0)
		return;
	other = ((cur + arg->i) % n + n) % n;
	if (other == cur)
		return;

	/* Swap window identity between the two list slots rather than
	 * relinking the list; arrange() recomputes geometry right after. */
	a = buf[cur];
	b = buf[other];
	tw = a->win;
	tf = a->floating;
	a->win = b->win;
	a->floating = b->floating;
	b->win = tw;
	b->floating = tf;
	wm.focused = b;
	arrange();
	focus(wm.focused);
}

void
setmfact(const Arg *arg)
{
	double f = wm.ws[wm.curws].master_ratio + arg->f;
	if (f < 0.1)
		f = 0.1;
	if (f > 0.9)
		f = 0.9;
	wm.ws[wm.curws].master_ratio = f;
	arrange();
}

void
togglefloating(const Arg *arg)
{
	(void)arg;
	if (!wm.focused)
		return;
	wm.focused->floating = !wm.focused->floating;
	arrange();
}

void
killclient(const Arg *arg)
{
	(void)arg;
	if (!wm.focused)
		return;
	if (!sendevent(wm.focused, wm.wm_delete_window)) {
		XGrabServer(wm.dpy);
		XSetErrorHandler(xerrordummy);
		XSetCloseDownMode(wm.dpy, DestroyAll);
		XKillClient(wm.dpy, wm.focused->win);
		XSync(wm.dpy, False);
		XSetErrorHandler(xerror);
		XUngrabServer(wm.dpy);
	}
}

void
view(const Arg *arg)
{
	unsigned int idx = arg->ui;
	if (idx >= WSCOUNT || (int)idx == wm.curws)
		return;
	wm.curws = (int)idx;
	arrange();
	showhideworkspace();
	focus(NULL);
}

void
tag(const Arg *arg)
{
	unsigned int idx = arg->ui;
	if (!wm.focused || idx >= WSCOUNT || (int)idx == wm.curws)
		return;
	wm.focused->workspace = (int)idx;
	arrange();
	showhideworkspace();
	focus(NULL);
}

void
refreshclients(void)
{
	for (Client *c = wm.clients; c; c = c->next) {
		c->bw = cfg.border_width;
		XSetWindowBorderWidth(wm.dpy, c->win, (unsigned int)c->bw);
		XSetWindowBorder(wm.dpy, c->win,
		                   getcolor(c == wm.focused ? cfg.color_focus : cfg.color_unfocus));
	}
}

void
movemouse(const Arg *arg)
{
	(void)arg;
	Client *c = wm.focused;
	int ocx, ocy, ox, oy;
	Time lasttime = 0;
	XEvent ev;

	if (!c)
		return;
	if (!c->floating) {
		c->floating = 1;
		arrange();
	}
	ocx = c->x;
	ocy = c->y;
	if (!getrootptr(&ox, &oy))
		return;
	if (XGrabPointer(wm.dpy, wm.root, False, MOUSEMASK, GrabModeAsync, GrabModeAsync,
	                  None, None, CurrentTime) != GrabSuccess)
		return;

	do {
		XMaskEvent(wm.dpy, MOUSEMASK | SubstructureRedirectMask | SubstructureNotifyMask, &ev);
		if (ev.type == MotionNotify) {
			if (ev.xmotion.time - lasttime <= 1000 / 60)
				continue;
			lasttime = ev.xmotion.time;
			resizeclient(c, ocx + (ev.xmotion.x - ox), ocy + (ev.xmotion.y - oy), c->w, c->h);
		} else if (ev.type == ConfigureRequest || ev.type == MapRequest) {
			handleevent(&ev);
		}
	} while (ev.type != ButtonRelease);
	XUngrabPointer(wm.dpy, CurrentTime);
}

void
resizemouse(const Arg *arg)
{
	(void)arg;
	Client *c = wm.focused;
	Time lasttime = 0;
	XEvent ev;
	int nw, nh;

	if (!c)
		return;
	if (!c->floating) {
		c->floating = 1;
		arrange();
	}
	if (XGrabPointer(wm.dpy, wm.root, False, MOUSEMASK, GrabModeAsync, GrabModeAsync,
	                  None, None, CurrentTime) != GrabSuccess)
		return;
	XWarpPointer(wm.dpy, None, c->win, 0, 0, 0, 0, c->w - 1, c->h - 1);

	do {
		XMaskEvent(wm.dpy, MOUSEMASK | SubstructureRedirectMask | SubstructureNotifyMask, &ev);
		if (ev.type == MotionNotify) {
			if (ev.xmotion.time - lasttime <= 1000 / 60)
				continue;
			lasttime = ev.xmotion.time;
			nw = MAX(ev.xmotion.x - c->x + 1, 1 + 2 * c->bw);
			nh = MAX(ev.xmotion.y - c->y + 1, 1 + 2 * c->bw);
			resizeclient(c, c->x, c->y, nw, nh);
		} else if (ev.type == ConfigureRequest || ev.type == MapRequest) {
			handleevent(&ev);
		}
	} while (ev.type != ButtonRelease);
	XWarpPointer(wm.dpy, None, c->win, 0, 0, 0, 0, c->w - c->bw - 1, c->h - c->bw - 1);
	XUngrabPointer(wm.dpy, CurrentTime);
}

#define CURSOR_STEP 20

void
movecursor(const Arg *arg)
{
	int dx = 0, dy = 0;

	switch (arg->i) {
	case 0: dx = -CURSOR_STEP; break; /* left */
	case 1: dx = +CURSOR_STEP; break; /* right */
	case 2: dy = -CURSOR_STEP; break; /* up */
	case 3: dy = +CURSOR_STEP; break; /* down */
	default: return;
	}
	XWarpPointer(wm.dpy, None, None, 0, 0, 0, 0, dx, dy);
	XFlush(wm.dpy);
}

void
moveclientdir(const Arg *arg)
{
	Client *best = NULL;
	long bestdist = 0;
	int fcx, fcy;

	if (!wm.focused || wm.focused->floating)
		return;
	fcx = wm.focused->x + wm.focused->w / 2;
	fcy = wm.focused->y + wm.focused->h / 2;

	for (Client *c = wm.clients; c; c = c->next) {
		if (c == wm.focused || c->workspace != wm.curws || c->floating)
			continue;
		int ccx = c->x + c->w / 2, ccy = c->y + c->h / 2;
		int dx = ccx - fcx, dy = ccy - fcy;
		int candidate;

		switch (arg->i) {
		case 0: candidate = dx < 0; break; /* left */
		case 1: candidate = dx > 0; break; /* right */
		case 2: candidate = dy < 0; break; /* up */
		case 3: candidate = dy > 0; break; /* down */
		default: return;
		}
		if (!candidate)
			continue;

		long dist = (long)dx * dx + (long)dy * dy;
		if (!best || dist < bestdist) {
			best = c;
			bestdist = dist;
		}
	}
	if (!best)
		return;

	/* Swap window identity between the two slots, like movestack — the
	 * layout recomputes real geometry right after. */
	Window tw = wm.focused->win;
	int tf = wm.focused->floating;
	wm.focused->win = best->win;
	wm.focused->floating = best->floating;
	best->win = tw;
	best->floating = tf;
	wm.focused = best;
	arrange();
	focus(wm.focused);
}
