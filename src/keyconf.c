/* Runtime keybinding config: ~/.config/zovwm/keys.conf.
 *
 * File format, one bind per line, blank lines and lines starting with '#'
 * ignored:
 *
 *   Mod+Mod+Key action [arg...]
 *
 * e.g. "Super+Shift+j move_next" or "Super+Return spawn xterm". Modifier
 * names are Super/Shift/Ctrl/Alt; the key name is whatever
 * XStringToKeysym() accepts (the same names XKeysymToString() prints, so
 * anything grabkeys() can grab round-trips through this file). "spawn"
 * takes the rest of the line as a whitespace-split argv; "view"/"tag" take
 * a 1-indexed workspace number; every other action is a fixed, no-argument
 * call into the existing client.c/layout.c/main.c functions.
 *
 * On first run (no file yet) the compiled-in defaults below seed both the
 * in-memory bind list shown by the first-run wizard (wizard.c) and, once
 * the wizard is done, the file itself.
 */
/* strdup/strtok_r/mkdir/strcasecmp are POSIX, not ISO C11; make sure
 * they're declared under -std=c11 on every libc (glibc, musl, ...). */
#define _POSIX_C_SOURCE 200809L

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <X11/keysym.h>

#include "zovwm.h"
#include "config.h"

#define MAXBINDS 64
#define MAXARG 192

typedef enum { KIND_FIXED, KIND_SPAWN, KIND_WORKSPACE } ActionKind;

typedef struct {
	const char *name;
	void (*func)(const Arg *arg);
	ActionKind kind;
	Arg fixedarg; /* used verbatim when kind == KIND_FIXED */
} ActionDef;

static const ActionDef actions[] = {
	{"spawn",            spawn,          KIND_SPAWN,     {0}},
	{"focus_next",       focusstack,     KIND_FIXED,     {.i = +1}},
	{"focus_prev",       focusstack,     KIND_FIXED,     {.i = -1}},
	{"move_next",        movestack,      KIND_FIXED,     {.i = +1}},
	{"move_prev",        movestack,      KIND_FIXED,     {.i = -1}},
	{"mfact_inc",        setmfact,       KIND_FIXED,     {.f = +0.05f}},
	{"mfact_dec",        setmfact,       KIND_FIXED,     {.f = -0.05f}},
	{"toggle_floating",  togglefloating, KIND_FIXED,     {0}},
	{"layout_tile",      setlayout,      KIND_FIXED,     {.i = LAYOUT_TILE}},
	{"layout_monocle",   setlayout,      KIND_FIXED,     {.i = LAYOUT_MONOCLE}},
	{"layout_grid",      setlayout,      KIND_FIXED,     {.i = LAYOUT_GRID}},
	{"kill",             killclient,     KIND_FIXED,     {0}},
	{"quit",             quit,           KIND_FIXED,     {0}},
	{"reload",           reloadconfig,   KIND_FIXED,     {0}},
	{"power_menu",       powermenu_run,  KIND_FIXED,     {0}},
	{"view",             view,           KIND_WORKSPACE, {0}},
	{"tag",              tag,            KIND_WORKSPACE, {0}},
};

typedef struct {
	char combo[32];
	const char *action;
	char arg[MAXARG];
	const char *label;
} BindEntry;

static BindEntry entries[MAXBINDS];
static int nentries;

/* Compiled-in defaults: also what the first-run wizard shows before any
 * remapping. view/tag 1..9 are appended in a loop below instead of being
 * spelled out here. */
static const struct {
	const char *combo, *action, *arg, *label;
} defaults[] = {
	{"Super+Return",     "spawn",           "xterm",              "Open terminal"},
	{"Super+p",           "spawn",           "dmenu_run",          "Open dmenu launcher"},
	{"Super+d",           "spawn",           "rofi -show drun",    "Open rofi launcher"},
	{"Super+w",           "spawn",           "zovwm-wallpaper",    "Fetch a new random wallpaper"},
	{"Super+j",           "focus_next",      "",                   "Focus next window"},
	{"Super+k",           "focus_prev",      "",                   "Focus previous window"},
	{"Super+Shift+j",     "move_next",       "",                   "Move window down the stack"},
	{"Super+Shift+k",     "move_prev",       "",                   "Move window up the stack"},
	{"Super+h",           "mfact_dec",       "",                   "Shrink master column"},
	{"Super+l",           "mfact_inc",       "",                   "Grow master column"},
	{"Super+f",           "toggle_floating", "",                   "Toggle floating"},
	{"Super+t",           "layout_tile",     "",                   "Tile layout"},
	{"Super+m",           "layout_monocle",  "",                   "Monocle layout"},
	{"Super+g",           "layout_grid",     "",                   "Grid layout"},
	{"Super+Shift+q",     "kill",            "",                   "Close focused window"},
	{"Super+Shift+r",     "reload",          "",                   "Reload configuration"},
	{"Super+Shift+p",     "power_menu",      "",                   "Power menu (reboot/shutdown/sleep/logout)"},
	{"Super+Shift+e",     "quit",            "",                   "Quit zovwm"},
};

static const ActionDef *
findaction(const char *name)
{
	for (unsigned int i = 0; i < LENGTH(actions); i++)
		if (strcmp(actions[i].name, name) == 0)
			return &actions[i];
	return NULL;
}

static char *
configpath(void)
{
	static char path[512];
	const char *home = getenv("HOME");
	snprintf(path, sizeof path, "%s/.config/zovwm/keys.conf", home ? home : "/tmp");
	return path;
}

void
keyconf_seed_defaults(void)
{
	nentries = 0;
	for (unsigned int i = 0; i < LENGTH(defaults) && nentries < MAXBINDS; i++, nentries++) {
		snprintf(entries[nentries].combo, sizeof entries[nentries].combo, "%s", defaults[i].combo);
		entries[nentries].action = defaults[i].action;
		snprintf(entries[nentries].arg, sizeof entries[nentries].arg, "%s", defaults[i].arg);
		entries[nentries].label = defaults[i].label;
	}
	for (int ws = 1; ws <= WSCOUNT && nentries + 1 < MAXBINDS; ws++) {
		static char viewlabels[WSCOUNT][24], taglabels[WSCOUNT][32];
		snprintf(entries[nentries].combo, sizeof entries[nentries].combo, "Super+%d", ws);
		entries[nentries].action = "view";
		snprintf(entries[nentries].arg, sizeof entries[nentries].arg, "%d", ws);
		snprintf(viewlabels[ws - 1], sizeof viewlabels[ws - 1], "Switch to workspace %d", ws);
		entries[nentries].label = viewlabels[ws - 1];
		nentries++;

		snprintf(entries[nentries].combo, sizeof entries[nentries].combo, "Super+Shift+%d", ws);
		entries[nentries].action = "tag";
		snprintf(entries[nentries].arg, sizeof entries[nentries].arg, "%d", ws);
		snprintf(taglabels[ws - 1], sizeof taglabels[ws - 1], "Move window to workspace %d", ws);
		entries[nentries].label = taglabels[ws - 1];
		nentries++;
	}
}

static unsigned int
parsecombo(const char *combo, KeySym *keysym)
{
	char buf[32];
	unsigned int mod = 0;

	snprintf(buf, sizeof buf, "%s", combo);
	char *save = NULL;
	char *tok = strtok_r(buf, "+", &save);
	char last[32] = "";
	while (tok) {
		snprintf(last, sizeof last, "%s", tok);
		if (strcasecmp(tok, "Super") == 0)
			mod |= Mod4Mask;
		else if (strcasecmp(tok, "Shift") == 0)
			mod |= ShiftMask;
		else if (strcasecmp(tok, "Ctrl") == 0 || strcasecmp(tok, "Control") == 0)
			mod |= ControlMask;
		else if (strcasecmp(tok, "Alt") == 0)
			mod |= Mod1Mask;
		tok = strtok_r(NULL, "+", &save);
	}
	*keysym = XStringToKeysym(last);
	return mod;
}

void
keyconf_build_keys(void)
{
	free(wm.keys);
	wm.keys = calloc((size_t)nentries, sizeof(Key));
	wm.nkeys = 0;
	if (!wm.keys)
		return;

	for (int i = 0; i < nentries; i++) {
		const ActionDef *a = findaction(entries[i].action);
		if (!a)
			continue;
		KeySym keysym;
		unsigned int mod = parsecombo(entries[i].combo, &keysym);
		if (keysym == NoSymbol)
			continue;

		Key *k = &wm.keys[wm.nkeys];
		k->mod = mod;
		k->keysym = keysym;
		k->func = a->func;

		switch (a->kind) {
		case KIND_FIXED:
			k->arg = a->fixedarg;
			break;
		case KIND_WORKSPACE: {
			int n = atoi(entries[i].arg);
			k->arg.ui = (unsigned int)(n > 0 ? n - 1 : 0);
			break;
		}
		case KIND_SPAWN: {
			/* Split entries[i].arg on whitespace into a NULL-terminated
			 * argv; on success, leaked intentionally (argv[] entries
			 * point into `copy`, and both live for the WM's lifetime,
			 * same as the static command arrays config.h used to hold). */
			char *copy = strdup(entries[i].arg);
			char **argv = calloc(MAXARG / 2 + 1, sizeof(char *));
			int n = 0;
			if (copy && argv) {
				char *save = NULL;
				char *tok = strtok_r(copy, " \t", &save);
				while (tok && n < MAXARG / 2) {
					argv[n++] = tok;
					tok = strtok_r(NULL, " \t", &save);
				}
			}
			if (n == 0) {
				/* Malformed "spawn" line (e.g. hand-edited config with
				 * no command) — execvp(NULL, ...) would crash the
				 * spawned child, so skip this bind entirely instead. */
				free(copy);
				free(argv);
				continue;
			}
			argv[n] = NULL;
			k->arg.v = argv;
			break;
		}
		}
		wm.nkeys++;
	}
}

int
keyconf_load(void)
{
	FILE *f = fopen(configpath(), "r");
	if (!f)
		return -1;

	nentries = 0;
	char line[256];
	while (fgets(line, sizeof line, f) && nentries < MAXBINDS) {
		char *p = line;
		while (isspace((unsigned char)*p))
			p++;
		if (*p == '#' || *p == '\0' || *p == '\n')
			continue;

		char combo[32] = "", action[24] = "";
		int consumed = 0;
		if (sscanf(p, "%31s %23s%n", combo, action, &consumed) < 2)
			continue;

		p += consumed;
		while (isspace((unsigned char)*p))
			p++;
		char *nl = strchr(p, '\n');
		if (nl)
			*nl = '\0';

		snprintf(entries[nentries].combo, sizeof entries[nentries].combo, "%s", combo);
		/* action must be one we know, so we can hand back a stable
		 * pointer (findaction returns into the static actions[] table). */
		const ActionDef *a = findaction(action);
		if (!a)
			continue;
		entries[nentries].action = a->name;
		snprintf(entries[nentries].arg, sizeof entries[nentries].arg, "%s", p);
		entries[nentries].label = "";
		nentries++;
	}
	fclose(f);
	return 0;
}

void
keyconf_save(void)
{
	char *path = configpath();
	char dir[512];
	snprintf(dir, sizeof dir, "%s", path);
	char *slash = strrchr(dir, '/');
	if (slash)
		*slash = '\0';

	/* mkdir -p equivalent for the (at most two-level) ~/.config/zovwm path */
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
	fprintf(f, "# zovwm keybindings — one per line, \"Mod+Mod+Key action [arg]\".\n");
	fprintf(f, "# Delete this file to see the first-run wizard again on next login.\n");
	for (int i = 0; i < nentries; i++) {
		if (entries[i].arg[0])
			fprintf(f, "%s %s %s\n", entries[i].combo, entries[i].action, entries[i].arg);
		else
			fprintf(f, "%s %s\n", entries[i].combo, entries[i].action);
	}
	fclose(f);
}

int
keyconf_count(void)
{
	return nentries;
}

const char *
keyconf_combo(int i)
{
	return entries[i].combo;
}

const char *
keyconf_label(int i)
{
	return entries[i].label[0] ? entries[i].label : entries[i].action;
}

void
keyconf_set_combo(int i, const char *combo)
{
	snprintf(entries[i].combo, sizeof entries[i].combo, "%s", combo);
}
