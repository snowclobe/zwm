/* Entry point: X11 setup, existing-window scan, and the main event loop. */
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/select.h>
#include <time.h>
#include <unistd.h>
#include <X11/cursorfont.h>

#include "zovwm.h"
#include "config.h"

WM wm;

/* Fetches a random wallpaper from Wallhaven's open API and sets it (see
 * rust/zovwm-wallpaper). Spawned once, unconditionally, below in setup(). */
static const char *wallpapercmd[] = { "zovwm-wallpaper", NULL };

void
die(const char *msg)
{
	fprintf(stderr, "%s\n", msg);
	exit(1);
}

void
spawn(const Arg *arg)
{
	char **argv = (char **)arg->v;

	if (fork() == 0) {
		if (wm.dpy)
			close(ConnectionNumber(wm.dpy));
		setsid();
		/* SIGCHLD=SIG_IGN (set below in setup(), to auto-reap our own
		 * children) survives fork+exec. A spawned program that itself
		 * forks and wait()s on a child — e.g. zovwm-wallpaper running
		 * curl — would otherwise get ECHILD, since the kernel discards
		 * exit status immediately when SIGCHLD is ignored. Reset it so
		 * spawned processes get normal signal semantics. */
		signal(SIGCHLD, SIG_DFL);
		execvp(argv[0], argv);
		fprintf(stderr, "zovwm: execvp %s failed\n", argv[0]);
		_exit(1);
	}
}

void
quit(const Arg *arg)
{
	(void)arg;
	wm.running = 0;
}

void
scan(void)
{
	unsigned int i, num;
	Window d1, d2, *wins = NULL;
	XWindowAttributes wa;

	if (!XQueryTree(wm.dpy, wm.root, &d1, &d2, &wins, &num))
		return;
	for (i = 0; i < num; i++) {
		if (!XGetWindowAttributes(wm.dpy, wins[i], &wa))
			continue;
		if (wa.override_redirect || wa.map_state != IsViewable)
			continue;
		manage(wins[i]);
	}
	if (wins)
		XFree(wins);
}

static void
setup(void)
{
	wm.dpy = XOpenDisplay(NULL);
	if (!wm.dpy)
		die("zovwm: cannot open display");

	wm.screen = DefaultScreen(wm.dpy);
	wm.root = RootWindow(wm.dpy, wm.screen);
	wm.sw = DisplayWidth(wm.dpy, wm.screen);
	wm.sh = DisplayHeight(wm.dpy, wm.screen);
	wm.clients = NULL;
	wm.focused = NULL;
	wm.curws = 0;
	wm.running = 1;

	for (int i = 0; i < WSCOUNT; i++) {
		wm.ws[i].master_ratio = default_mfact;
		wm.ws[i].nmaster = default_nmaster;
		wm.ws[i].layout = default_layout;
	}

	wm.wm_protocols = XInternAtom(wm.dpy, "WM_PROTOCOLS", False);
	wm.wm_delete_window = XInternAtom(wm.dpy, "WM_DELETE_WINDOW", False);

	/* SubstructureRedirectMask can only be held by one client at a time;
	 * if another WM already holds it the server raises BadAccess, which
	 * xerrorstart turns into a clean exit instead of a crash. */
	XSetErrorHandler(xerrorstart);
	XSelectInput(wm.dpy, wm.root, SubstructureRedirectMask | SubstructureNotifyMask);
	XSync(wm.dpy, False);
	XSetErrorHandler(xerror);
	XSync(wm.dpy, False);

	wm.cursor_normal = XCreateFontCursor(wm.dpy, XC_left_ptr);
	XDefineCursor(wm.dpy, wm.root, wm.cursor_normal);

	signal(SIGCHLD, SIG_IGN);

	if (keyconf_load() != 0) {
		/* No ~/.config/zovwm/keys.conf yet: first run. Show the
		 * compiled-in defaults in the wizard, let the user remap
		 * anything, then persist whatever they end up with (defaults
		 * or edits) so this only ever happens once. */
		keyconf_seed_defaults();
		wizard_run();
	}
	keyconf_build_keys();

	grabkeys();
	bar_init();
	{
		Arg wp = {.v = wallpapercmd};
		spawn(&wp);
	}
	scan();
}

static void
cleanup(void)
{
	bar_cleanup();
	XUngrabKey(wm.dpy, AnyKey, AnyModifier, wm.root);
	XFreeCursor(wm.dpy, wm.cursor_normal);
	XSync(wm.dpy, False);
	XCloseDisplay(wm.dpy);
}

int
main(void)
{
	XEvent ev;
	int xfd;
	time_t lastclock = 0;

	setup();
	xfd = ConnectionNumber(wm.dpy);

	while (wm.running) {
		while (wm.running && XPending(wm.dpy)) {
			XNextEvent(wm.dpy, &ev);
			handleevent(&ev);
		}
		if (!wm.running)
			break;

		fd_set fds;
		struct timeval tv = {.tv_sec = 1, .tv_usec = 0};
		FD_ZERO(&fds);
		FD_SET(xfd, &fds);
		select(xfd + 1, &fds, NULL, NULL, &tv);

		time_t now = time(NULL);
		if (now != lastclock) {
			lastclock = now;
			bar_draw();
		}
	}
	cleanup();
	return 0;
}
