/* Autostart: reads ~/.config/zovwm/autostart.conf and spawns each command
 * listed in it once during WM startup. One command per line; blank lines
 * and lines starting with '#' are ignored. The rest of the line is split
 * on whitespace into an argv and fork()+exec()'d, same as the "spawn"
 * keybinding action. If the file doesn't exist yet, a commented template
 * is written so the user knows where to add entries. */
#define _POSIX_C_SOURCE 200809L

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <signal.h>

#include "zovwm.h"

static char *
configpath(void)
{
	static char path[512];
	const char *home = getenv("HOME");
	snprintf(path, sizeof path, "%s/.config/zovwm/autostart.conf", home ? home : "/tmp");
	return path;
}

static void
mkdirp(const char *filepath)
{
	char dir[512];
	snprintf(dir, sizeof dir, "%s", filepath);
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
}

static void
writetemplate(void)
{
	char *path = configpath();
	mkdirp(path);

	FILE *f = fopen(path, "w");
	if (!f) {
		fprintf(stderr, "zovwm: cannot write %s\n", path);
		return;
	}
	fprintf(f, "# zovwm autostart configuration\n");
	fprintf(f, "# One command per line. Blank lines and lines starting with '#' are ignored.\n");
	fprintf(f, "# Commands are spawned once when zovwm starts.\n");
	fprintf(f, "#\n");
	fprintf(f, "# Examples:\n");
	fprintf(f, "# picom\n");
	fprintf(f, "# dunst\n");
	fprintf(f, "# nm-applet\n");
	fprintf(f, "# volumeicon\n");
	fprintf(f, "# setxkbmap -layout us,ru -option grp:alt_shift_toggle\n");
	fclose(f);
}

static void
spawnline(const char *line)
{
	/* Split the line on whitespace into a NULL-terminated argv. */
	char *copy = strdup(line);
	if (!copy)
		return;

	char *argv[64];
	int argc = 0;
	char *save = NULL;
	char *tok = strtok_r(copy, " \t", &save);
	while (tok && argc < 63) {
		argv[argc++] = tok;
		tok = strtok_r(NULL, " \t", &save);
	}
	if (argc == 0) {
		free(copy);
		return;
	}
	argv[argc] = NULL;

	if (fork() == 0) {
		if (wm.dpy)
			close(ConnectionNumber(wm.dpy));
		setsid();
		signal(SIGCHLD, SIG_DFL);
		execvp(argv[0], argv);
		fprintf(stderr, "zovwm: autostart: execvp %s failed\n", argv[0]);
		_exit(1);
	}
	free(copy);
}

void
autostart_run(void)
{
	char *path = configpath();
	FILE *f = fopen(path, "r");
	if (!f) {
		writetemplate();
		return;
	}

	char line[512];
	while (fgets(line, sizeof line, f)) {
		char *p = line;
		while (isspace((unsigned char)*p))
			p++;
		if (*p == '#' || *p == '\0' || *p == '\n')
			continue;
		/* Strip trailing newline */
		char *nl = strchr(p, '\n');
		if (nl)
			*nl = '\0';
		spawnline(p);
	}
	fclose(f);
}
