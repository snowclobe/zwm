/* Grabs every keybinding loaded into wm.keys (see keyconf.c) on the root
 * window. Dispatch happens in events.c's keypress handler. */
#include "zovwm.h"
#include "config.h"

void
grabkeys(void)
{
	/* Grab each binding under every combination of the lock modifiers
	 * (CapsLock, NumLock) so the shortcut still fires regardless of
	 * their state; events.c's CLEANMASK() strips them back out again. */
	static const unsigned int lockmods[] = {0, LockMask, Mod2Mask, LockMask | Mod2Mask};

	XUngrabKey(wm.dpy, AnyKey, AnyModifier, wm.root);
	for (int i = 0; i < wm.nkeys; i++) {
		KeyCode code = XKeysymToKeycode(wm.dpy, wm.keys[i].keysym);
		if (!code)
			continue;
		for (unsigned int j = 0; j < LENGTH(lockmods); j++)
			XGrabKey(wm.dpy, code, wm.keys[i].mod | lockmods[j], wm.root,
			          True, GrabModeAsync, GrabModeAsync);
	}
}
