/* Entry point: X11 setup, existing-window scan, and the main event loop. */
#define _POSIX_C_SOURCE 200809L

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __linux__
#include <sys/inotify.h> /* Linux-only: auto-reload on config save. The
                           * Super+Shift+r hotkey reload works everywhere;
                           * only the automatic on-save trigger needs this. */
#endif
#include <sys/select.h>
#include <time.h>
#include <unistd.h>
#include <X11/cursorfont.h>

#include "zovwm.h"
#include "config.h"

WM wm;
static int inotifyfd = -1;

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

/* Re-reads all three config files and reapplies everything without
 * restarting the WM: bound to "reload" (default Super+Shift+r) and fired
 * automatically when main()'s event loop sees keys.conf/zovwm.conf/
 * monitor.conf change on disk (see the inotify handling below).
 * Note: if monitor.conf now names a different mode, xrandr is re-run but
 * wm.sw/wm.sh stay at whatever they were read as on startup (see setup())
 * — Xlib only refreshes its cached screen size on reconnect. Editing
 * monitor.conf by hand mid-session still needs a WM restart to retile to
 * the new size; the wizard-driven first-run path handles this itself. */
void
reloadconfig(const Arg *arg)
{
	(void)arg;

	keyconf_load();
	keyconf_build_keys();
	grabkeys();

	appconf_reload();
	monitorconf_apply();
	refreshclients();
	bar_reload();
	arrange();
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
watchconfigdir(void)
{
#ifdef __linux__
	const char *home = getenv("HOME");
	char path[512];
	snprintf(path, sizeof path, "%s/.config/zovwm", home ? home : "/tmp");

	inotifyfd = inotify_init1(IN_NONBLOCK);
	if (inotifyfd < 0)
		return;
	/* IN_CLOSE_WRITE covers a plain write; IN_MOVED_TO covers the
	 * write-to-temp-then-rename pattern most editors actually use. */
	if (inotify_add_watch(inotifyfd, path, IN_CLOSE_WRITE | IN_MOVED_TO) < 0) {
		close(inotifyfd);
		inotifyfd = -1;
	}
#endif
}

static void
setup(void)
{
	/* Re-assert any saved monitor mode before we even connect: xrandr(1)
	 * talks to the X server over $DISPLAY on its own, and doing this first
	 * means the screen size Xlib caches at connect time (DisplayWidth/
	 * DisplayHeight below) is already correct. Returns -1 on a first run
	 * (no monitor.conf yet) — remembered so the setup wizard can run once
	 * we have a display to draw on. */
	int monitor_firstrun = monitorconf_apply() != 0;

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

	appconf_load();

	if (monitor_firstrun && monitorwizard_run()) {
		/* The mode changed: reopen so DisplayWidth/DisplayHeight (cached
		 * by Xlib at connect time) reflect the new geometry. Nothing else
		 * has been created on the display yet, so this is safe here. */
		XCloseDisplay(wm.dpy);
		wm.dpy = XOpenDisplay(NULL);
		if (!wm.dpy)
			die("zovwm: cannot open display");
		wm.screen = DefaultScreen(wm.dpy);
		wm.root = RootWindow(wm.dpy, wm.screen);
		wm.sw = DisplayWidth(wm.dpy, wm.screen);
		wm.sh = DisplayHeight(wm.dpy, wm.screen);
	}

	/* Restore keyboard layouts from a previous session before any
	 * wizard touches the keyboard. If no saved config exists yet,
	 * show the keyboard-layout wizard once (first run). */
	if (kblayout_conf_exists())
		kblayout_apply_saved();
	else
		kbwizard_run();

	for (int i = 0; i < WSCOUNT; i++) {
		wm.ws[i].master_ratio = cfg.master_ratio;
		wm.ws[i].nmaster = cfg.master_count;
		wm.ws[i].layout = cfg.default_layout;
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
	tray_init();
	watchconfigdir();
	{
		Arg wp = {.v = wallpapercmd};
		spawn(&wp);
	}
	scan();
	autostart_run();
}

static void
cleanup(void)
{
	if (inotifyfd >= 0)
		close(inotifyfd);
	tray_cleanup();
	bar_cleanup();
	XUngrabKey(wm.dpy, AnyKey, AnyModifier, wm.root);
	XFreeCursor(wm.dpy, wm.cursor_normal);
	XSync(wm.dpy, False);
	XCloseDisplay(wm.dpy);
}

#ifdef __linux__
static void
handleconfigchange(void)
{
	_Alignas(struct inotify_event) char buf[4096];
	ssize_t len;

	while ((len = read(inotifyfd, buf, sizeof buf)) > 0) {
		ssize_t off = 0;
		while (off < len) {
			struct inotify_event *ie = (struct inotify_event *)(buf + off);
			if (ie->len > 0 &&
			    (strcmp(ie->name, "keys.conf") == 0 || strcmp(ie->name, "zovwm.conf") == 0 ||
			     strcmp(ie->name, "monitor.conf") == 0))
				reloadconfig(NULL);
			off += (ssize_t)(sizeof(struct inotify_event) + ie->len);
		}
	}
}
#endif

static void
printusage(void)
{
	printf("usage: zovwm [--list-keys] [--help]\n");
	printf("  --list-keys   print every keybinding action (and your current\n");
	printf("                binds, if configured yet), then exit\n");
	printf("  --help, -h    print this message and exit\n");
}

/* `zovwm --list-keys`: a standing, no-X-needed reference for hand-editing
 * ~/.config/zovwm/keys.conf — every action keys.conf understands, plus
 * whatever's already bound, so extending your own config doesn't require
 * re-running the first-run wizard or reading the source. See also
 * examples/keys.conf.example. */
static void
listkeys(void)
{
	keyconf_print_actions();
	printf("\n");
	if (keyconf_load() == 0) {
		printf("Your current binds (~/.config/zovwm/keys.conf):\n\n");
		int n = keyconf_count();
		for (int i = 0; i < n; i++) {
			const char *arg = keyconf_arg(i);
			if (arg[0])
				printf("  %-20s %s %s\n", keyconf_combo(i), keyconf_action(i), arg);
			else
				printf("  %-20s %s\n", keyconf_combo(i), keyconf_action(i));
		}
	} else {
		printf("No ~/.config/zovwm/keys.conf yet - it's created the first time zovwm runs.\n");
	}
}

int
main(int argc, char *argv[])
{
	XEvent ev;
	int xfd;
	time_t lastclock = 0;

	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "--list-keys") == 0) {
			listkeys();
			return 0;
		}
		if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
			printusage();
			return 0;
		}
		fprintf(stderr, "zovwm: unknown option '%s'\n", argv[i]);
		printusage();
		return 1;
	}

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
		int maxfd = xfd;
		FD_ZERO(&fds);
		FD_SET(xfd, &fds);
		if (inotifyfd >= 0) {
			FD_SET(inotifyfd, &fds);
			if (inotifyfd > maxfd)
				maxfd = inotifyfd;
		}
		select(maxfd + 1, &fds, NULL, NULL, &tv);

#ifdef __linux__
		if (inotifyfd >= 0 && FD_ISSET(inotifyfd, &fds))
			handleconfigchange();
#endif

		time_t now = time(NULL);
		if (now != lastclock) {
			lastclock = now;
			bar_draw();
		}
	}
	cleanup();
	return 0;
}
