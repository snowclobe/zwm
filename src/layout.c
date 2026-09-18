/* Re-tiles the current workspace's non-floating clients. The tile/grid
 * geometry math lives in the Rust zovwm-layout crate (see zov_layout.h);
 * monocle is trivial enough to do directly in C. */
#include "zovwm.h"
#include "zov_layout.h"
#include "config.h"

#define MAXTILED 256

void
arrange(void)
{
	Client *tiled[MAXTILED];
	ZovRect rects[MAXTILED];
	unsigned int n = 0;
	int sy = BARHEIGHT, sh = wm.sh - BARHEIGHT;

	for (Client *c = wm.clients; c && n < MAXTILED; c = c->next)
		if (c->workspace == wm.curws && !c->floating)
			tiled[n++] = c;

	if (n == 0)
		return;

	switch (wm.ws[wm.curws].layout) {
	case LAYOUT_MONOCLE:
		for (unsigned int i = 0; i < n; i++)
			resizeclient(tiled[i], GAP, sy + GAP, wm.sw - 2 * GAP, sh - 2 * GAP);
		if (wm.focused)
			for (unsigned int i = 0; i < n; i++)
				if (tiled[i] == wm.focused) {
					XRaiseWindow(wm.dpy, wm.focused->win);
					break;
				}
		break;

	case LAYOUT_GRID: {
		unsigned int written = zov_layout_grid(n, 0, sy, wm.sw, sh, GAP, rects, n);
		for (unsigned int i = 0; i < written; i++)
			resizeclient(tiled[i], rects[i].x, rects[i].y, rects[i].w, rects[i].h);
		break;
	}

	case LAYOUT_TILE:
	default: {
		unsigned int written = zov_layout_master_stack(
		    n, 0, sy, wm.sw, sh, GAP,
		    (float)wm.ws[wm.curws].master_ratio,
		    (unsigned int)wm.ws[wm.curws].nmaster,
		    rects, n);
		for (unsigned int i = 0; i < written; i++)
			resizeclient(tiled[i], rects[i].x, rects[i].y, rects[i].w, rects[i].h);
		break;
	}
	}
}

void
setlayout(const Arg *arg)
{
	wm.ws[wm.curws].layout = (LayoutType)arg->i;
	arrange();
	bar_draw();
}
