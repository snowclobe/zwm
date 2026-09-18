/* Re-tiles the current workspace's non-floating clients. The tile/grid
 * geometry math lives in the Rust zovwm-layout crate (see zov_layout.h);
 * monocle is trivial enough to do directly in C. */
#include "zovwm.h"
#include "zov_layout.h"

#define MAXTILED 256

/* Raises wm.focused above its siblings (and, since it's a real XRaiseWindow
 * on the shared root stacking order, above the override-redirect bar too —
 * used by both LAYOUT_MONOCLE and LAYOUT_FULLSCREEN, which stack every
 * tiled client on top of each other and need the focused one visible). */
static void
raisefocused(Client **tiled, unsigned int n)
{
	if (!wm.focused)
		return;
	for (unsigned int i = 0; i < n; i++)
		if (tiled[i] == wm.focused) {
			XRaiseWindow(wm.dpy, wm.focused->win);
			return;
		}
}

void
arrange(void)
{
	Client *tiled[MAXTILED];
	ZovRect rects[MAXTILED];
	unsigned int n = 0;
	int sy = cfg.bar_height, sh = wm.sh - cfg.bar_height;

	for (Client *c = wm.clients; c && n < MAXTILED; c = c->next)
		if (c->workspace == wm.curws && !c->floating)
			tiled[n++] = c;

	if (n == 0)
		return;

	switch (wm.ws[wm.curws].layout) {
	case LAYOUT_FULLSCREEN:
		/* Fullscreen below the bar: fills the entire work area with no
		 * gaps, but leaves the bar visible at the top. */
		for (unsigned int i = 0; i < n; i++)
			resizeclient(tiled[i], 0, cfg.bar_height, wm.sw, wm.sh - cfg.bar_height);
		raisefocused(tiled, n);
		break;

	case LAYOUT_MONOCLE:
		for (unsigned int i = 0; i < n; i++)
			resizeclient(tiled[i], cfg.gap, sy + cfg.gap, wm.sw - 2 * cfg.gap, sh - 2 * cfg.gap);
		raisefocused(tiled, n);
		break;

	case LAYOUT_GRID: {
		unsigned int written = zov_layout_grid(n, 0, sy, wm.sw, sh, cfg.gap, rects, n);
		for (unsigned int i = 0; i < written; i++)
			resizeclient(tiled[i], rects[i].x, rects[i].y, rects[i].w, rects[i].h);
		break;
	}

	case LAYOUT_BSTACK:
	default: {
		unsigned int written = zov_layout_bstack(
		    n, 0, sy, wm.sw, sh, cfg.gap,
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
