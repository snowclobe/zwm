/* Runtime appearance config: ~/.config/zovwm/zovwm.conf.
 *
 * Same "key value" per line format as keys.conf (see keyconf.c), same
 * load-with-hardcoded-fallback-and-seed-the-file-if-missing pattern. Keeps
 * `cfg` (declared in zovwm.h) up to date; every file that used to read
 * config.h's GAP, BORDERWIDTH, BARHEIGHT, col_*, barcol_*, default_*
 * constants reads cfg.* instead. */
#define _POSIX_C_SOURCE 200809L

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "zovwm.h"

AppConfig cfg;

static void
setdefaults(void)
{
	cfg.gap = 8;
	cfg.border_width = 2;
	cfg.bar_height = 20;
	cfg.master_count = 1;
	cfg.master_ratio = 0.55;
	/* bstack, not fullscreen: a tiling WM should show multiple windows at
	 * once by default. Fullscreen (Super+t) is an explicit opt-in. */
	cfg.default_layout = LAYOUT_BSTACK;
	snprintf(cfg.color_focus, sizeof cfg.color_focus, "#5e81ac");
	snprintf(cfg.color_unfocus, sizeof cfg.color_unfocus, "#3b4252");
	snprintf(cfg.bar_font, sizeof cfg.bar_font, "fixed");
	snprintf(cfg.bar_color_bg, sizeof cfg.bar_color_bg, "#2e3440");
	snprintf(cfg.bar_color_fg, sizeof cfg.bar_color_fg, "#d8dee9");
	snprintf(cfg.bar_color_cur, sizeof cfg.bar_color_cur, "#88c0d0");
	snprintf(cfg.bar_color_occupied, sizeof cfg.bar_color_occupied, "#a3be8c");
	snprintf(cfg.bar_color_empty, sizeof cfg.bar_color_empty, "#4c566a");
}

static char *
configpath(void)
{
	static char path[512];
	const char *home = getenv("HOME");
	snprintf(path, sizeof path, "%s/.config/zovwm/zovwm.conf", home ? home : "/tmp");
	return path;
}

static LayoutType
parselayout(const char *s, LayoutType fallback)
{
	if (strcmp(s, "fullscreen") == 0)
		return LAYOUT_FULLSCREEN;
	if (strcmp(s, "monocle") == 0)
		return LAYOUT_MONOCLE;
	if (strcmp(s, "bstack") == 0)
		return LAYOUT_BSTACK;
	if (strcmp(s, "grid") == 0)
		return LAYOUT_GRID;
	return fallback;
}

static const char *
layoutname(LayoutType l)
{
	switch (l) {
	case LAYOUT_FULLSCREEN: return "fullscreen";
	case LAYOUT_MONOCLE:    return "monocle";
	case LAYOUT_GRID:       return "grid";
	case LAYOUT_BSTACK:
	default:                 return "bstack";
	}
}

/* Applies one "key value" line to cfg. Unknown keys are ignored (forward
 * compatible with older config files after a field is added/removed). */
static void
applyline(const char *key, const char *val)
{
	if (strcmp(key, "gap") == 0)
		cfg.gap = atoi(val);
	else if (strcmp(key, "border_width") == 0)
		cfg.border_width = atoi(val);
	else if (strcmp(key, "bar_height") == 0)
		cfg.bar_height = atoi(val);
	else if (strcmp(key, "master_count") == 0)
		cfg.master_count = atoi(val);
	else if (strcmp(key, "master_ratio") == 0)
		cfg.master_ratio = atof(val);
	else if (strcmp(key, "default_layout") == 0)
		cfg.default_layout = parselayout(val, cfg.default_layout);
	else if (strcmp(key, "color_focus") == 0)
		snprintf(cfg.color_focus, sizeof cfg.color_focus, "%s", val);
	else if (strcmp(key, "color_unfocus") == 0)
		snprintf(cfg.color_unfocus, sizeof cfg.color_unfocus, "%s", val);
	else if (strcmp(key, "bar_font") == 0)
		snprintf(cfg.bar_font, sizeof cfg.bar_font, "%s", val);
	else if (strcmp(key, "bar_color_bg") == 0)
		snprintf(cfg.bar_color_bg, sizeof cfg.bar_color_bg, "%s", val);
	else if (strcmp(key, "bar_color_fg") == 0)
		snprintf(cfg.bar_color_fg, sizeof cfg.bar_color_fg, "%s", val);
	else if (strcmp(key, "bar_color_cur") == 0)
		snprintf(cfg.bar_color_cur, sizeof cfg.bar_color_cur, "%s", val);
	else if (strcmp(key, "bar_color_occupied") == 0)
		snprintf(cfg.bar_color_occupied, sizeof cfg.bar_color_occupied, "%s", val);
	else if (strcmp(key, "bar_color_empty") == 0)
		snprintf(cfg.bar_color_empty, sizeof cfg.bar_color_empty, "%s", val);
}

void
appconf_reload(void)
{
	FILE *f = fopen(configpath(), "r");
	if (!f)
		return;

	char line[256];
	while (fgets(line, sizeof line, f)) {
		char *p = line;
		while (isspace((unsigned char)*p))
			p++;
		if (*p == '#' || *p == '\0' || *p == '\n')
			continue;

		char key[32] = "", val[64] = "";
		if (sscanf(p, "%31s %63s", key, val) == 2)
			applyline(key, val);
	}
	fclose(f);
}

static void
writedefaults(void)
{
	char *path = configpath();
	char dir[512];
	snprintf(dir, sizeof dir, "%s", path);
	char *slash = strrchr(dir, '/');
	if (slash)
		*slash = '\0';
	for (char *s = dir + 1; *s; s++) {
		if (*s == '/') {
			*s = '\0';
			mkdir(dir, 0755);
			*s = '/';
		}
	}
	mkdir(dir, 0755);

	FILE *f = fopen(path, "w");
	if (!f) {
		fprintf(stderr, "zovwm: cannot write %s\n", path);
		return;
	}
	fprintf(f, "# zovwm appearance config - one \"key value\" per line.\n");
	fprintf(f, "# Reload with Super+Shift+r, or it applies automatically on save.\n");
	fprintf(f, "gap %d\n", cfg.gap);
	fprintf(f, "border_width %d\n", cfg.border_width);
	fprintf(f, "bar_height %d\n", cfg.bar_height);
	fprintf(f, "master_count %d\n", cfg.master_count);
	fprintf(f, "master_ratio %.2f\n", cfg.master_ratio);
	fprintf(f, "default_layout %s\n", layoutname(cfg.default_layout));
	fprintf(f, "color_focus %s\n", cfg.color_focus);
	fprintf(f, "color_unfocus %s\n", cfg.color_unfocus);
	fprintf(f, "bar_font %s\n", cfg.bar_font);
	fprintf(f, "bar_color_bg %s\n", cfg.bar_color_bg);
	fprintf(f, "bar_color_fg %s\n", cfg.bar_color_fg);
	fprintf(f, "bar_color_cur %s\n", cfg.bar_color_cur);
	fprintf(f, "bar_color_occupied %s\n", cfg.bar_color_occupied);
	fprintf(f, "bar_color_empty %s\n", cfg.bar_color_empty);
	fclose(f);
}

void
appconf_load(void)
{
	setdefaults();

	FILE *f = fopen(configpath(), "r");
	if (!f) {
		writedefaults();
		return;
	}
	fclose(f);
	appconf_reload();
}
