/* Runtime monitor (resolution + refresh rate) config: ~/.config/zovwm/monitor.conf,
 * one "output mode rate" line per configured output, e.g. "eDP-1 1920x1080 60".
 * Applied by shelling out to xrandr(1) rather than reimplementing RandR
 * CRTC/mode-ID matching ourselves — xrandr is a standard Xorg package and
 * already handles the edge cases correctly. */
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

#include "zovwm.h"

void
monitorconf_set(const char *output, const char *mode, const char *rate)
{
	pid_t pid = fork();

	if (pid == 0) {
		execlp("xrandr", "xrandr", "--output", output, "--mode", mode, "--rate", rate, NULL);
		_exit(1);
	} else if (pid > 0) {
		int status;
		waitpid(pid, &status, 0);
	}
}

/* Must be called before our own XOpenDisplay: xrandr(1) talks to the X
 * server over $DISPLAY on its own, so applying the saved mode before we
 * connect means the screen size Xlib caches at connect time is already
 * correct — no need to reopen the display on every normal startup. */
int
monitorconf_apply(void)
{
	char path[512], line[256];
	const char *home = getenv("HOME");
	FILE *f;

	snprintf(path, sizeof path, "%s/.config/zovwm/monitor.conf", home ? home : "/tmp");
	f = fopen(path, "r");
	if (!f)
		return -1;

	while (fgets(line, sizeof line, f)) {
		char output[64], mode[32], rate[16];
		if (sscanf(line, "%63s %31s %15s", output, mode, rate) == 3)
			monitorconf_set(output, mode, rate);
	}
	fclose(f);
	return 0;
}
