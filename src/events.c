/* X11 event dispatch. No reparenting/decorations: clients are tiled by
 * moving/resizing their top-level window directly, so the event set we care
 * about is small. */
#include <stdio.h>
#include <X11/Xatom.h>
#include <X11/XKBlib.h>

#include "zovwm.h"
#include "config.h"

#define CLEANMASK(mask) ((mask) & ~(LockMask | Mod2Mask))

static void
maprequest(XEvent *e)
{
	manage(e->xmaprequest.window);
}

static void
unmapnotify(XEvent *e)
{
	/* We never XUnmapWindow() a managed client ourselves (hidden
	 * workspaces are parked off-screen instead, see showhideworkspace),
	 * so any UnmapNotify we see means the client is going away. */
	Client *c = wintoclient(e->xunmap.window);
	if (c)
		unmanage(c, 0);
}

static void
destroynotify(XEvent *e)
{
	Client *c = wintoclient(e->xdestroywindow.window);
	if (c)
		unmanage(c, 1);
}

static void
configurerequest(XEvent *e)
{
	XConfigureRequestEvent *ev = &e->xconfigurerequest;
	Client *c = wintoclient(ev->window);
	XWindowChanges wc;

	if (c) {
		if (c->floating) {
			if (ev->value_mask & CWX)
				c->x = ev->x;
			if (ev->value_mask & CWY)
				c->y = ev->y;
			if (ev->value_mask & CWWidth)
				c->w = ev->width;
			if (ev->value_mask & CWHeight)
				c->h = ev->height;
			wc.x = c->x;
			wc.y = c->y;
			wc.width = c->w;
			wc.height = c->h;
			wc.border_width = c->bw;
			XConfigureWindow(wm.dpy, c->win,
			                  ev->value_mask & (CWX | CWY | CWWidth | CWHeight | CWBorderWidth),
			                  &wc);
		} else {
			/* Tiled: our layout owns the geometry. Reassert it so the
			 * client's request doesn't silently take effect. */
			XConfigureEvent ce = {0};
			ce.type = ConfigureNotify;
			ce.display = wm.dpy;
			ce.event = c->win;
			ce.window = c->win;
			ce.x = c->x;
			ce.y = c->y;
			ce.width = MAX(c->w - 2 * c->bw, 1);
			ce.height = MAX(c->h - 2 * c->bw, 1);
			ce.border_width = c->bw;
			ce.above = None;
			ce.override_redirect = False;
			XSendEvent(wm.dpy, c->win, False, StructureNotifyMask, (XEvent *)&ce);
		}
	} else {
		wc.x = ev->x;
		wc.y = ev->y;
		wc.width = ev->width;
		wc.height = ev->height;
		wc.border_width = ev->border_width;
		wc.sibling = ev->above;
		wc.stack_mode = ev->detail;
		XConfigureWindow(wm.dpy, ev->window, (unsigned int)ev->value_mask, &wc);
	}
	XSync(wm.dpy, False);
}

static void
enternotify(XEvent *e)
{
	XCrossingEvent *ev = &e->xcrossing;
	Client *c;

	if ((ev->mode != NotifyNormal || ev->detail == NotifyInferior) && ev->window != wm.root)
		return;
	c = wintoclient(ev->window);
	if (c)
		focus(c);
}

static void
keypress(XEvent *e)
{
	XKeyEvent *ev = &e->xkey;
	KeySym keysym = XkbKeycodeToKeysym(wm.dpy, (KeyCode)ev->keycode, 0, 0);

	for (unsigned int i = 0; i < LENGTH(keys); i++)
		if (keysym == keys[i].keysym &&
		    CLEANMASK(keys[i].mod) == CLEANMASK(ev->state) &&
		    keys[i].func)
			keys[i].func(&keys[i].arg);
}

static void
propertynotify(XEvent *e)
{
	XPropertyEvent *ev = &e->xproperty;
	Client *c;

	if (ev->atom != XA_WM_NAME)
		return;
	c = wintoclient(ev->window);
	if (c && c == wm.focused)
		bar_draw();
}

static void
buttonpress(XEvent *e)
{
	XButtonEvent *ev = &e->xbutton;
	Client *c = wintoclient(ev->window);

	if (c)
		focus(c);
	for (unsigned int i = 0; i < LENGTH(buttons); i++)
		if (buttons[i].button == ev->button &&
		    CLEANMASK(buttons[i].mod) == CLEANMASK(ev->state) &&
		    buttons[i].func)
			buttons[i].func(&buttons[i].arg);
}

void
handleevent(XEvent *ev)
{
	switch (ev->type) {
	case MapRequest:
		maprequest(ev);
		break;
	case UnmapNotify:
		unmapnotify(ev);
		break;
	case DestroyNotify:
		destroynotify(ev);
		break;
	case ConfigureRequest:
		configurerequest(ev);
		break;
	case EnterNotify:
		enternotify(ev);
		break;
	case KeyPress:
		keypress(ev);
		break;
	case ButtonPress:
		buttonpress(ev);
		break;
	case PropertyNotify:
		propertynotify(ev);
		break;
	case Expose:
		if (ev->xexpose.count == 0)
			bar_draw();
		break;
	case MappingNotify:
		XRefreshKeyboardMapping(&ev->xmapping);
		if (ev->xmapping.request == MappingKeyboard)
			grabkeys();
		break;
	default:
		break;
	}
}

int
xerrorstart(Display *dpy, XErrorEvent *ee)
{
	(void)dpy;
	(void)ee;
	die("zovwm: another window manager is already running");
	return -1;
}

int
xerrordummy(Display *dpy, XErrorEvent *ee)
{
	(void)dpy;
	(void)ee;
	return 0;
}

int
xerror(Display *dpy, XErrorEvent *ee)
{
	char msg[256];

	/* A WM must not die from X errors caused by a client racing us (e.g.
	 * the window is destroyed between our request and the server
	 * processing it) — log and keep running. */
	XGetErrorText(dpy, ee->error_code, msg, sizeof msg);
	fprintf(stderr, "zovwm: X error: %s (request %d, resource 0x%lx)\n",
	        msg, ee->request_code, ee->resourceid);
	return 0;
}
