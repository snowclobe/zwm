/* Grabs every keybinding from the static table in config.h on the root
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
	for (unsigned int i = 0; i < LENGTH(keys); i++) {
		KeyCode code = XKeysymToKeycode(wm.dpy, keys[i].keysym);
		if (!code)
			continue;
		for (unsigned int j = 0; j < LENGTH(lockmods); j++)
			XGrabKey(wm.dpy, code, keys[i].mod | lockmods[j], wm.root,
			          True, GrabModeAsync, GrabModeAsync);
	}
}
