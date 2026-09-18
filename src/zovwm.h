#ifndef ZOVWM_H
#define ZOVWM_H

#if defined(__has_include)
#  if __has_include(<X11/Xlib.h>)
#    include <X11/Xlib.h>
#  elif __has_include(<XQuartz/Xlib.h>)
#    include <XQuartz/Xlib.h>
#  elif __has_include("/opt/X11/include/X11/Xlib.h")
#    include "/opt/X11/include/X11/Xlib.h"
#  else
#    error "X11/Xlib.h not found; install X11/XQuartz development headers or configure the compiler include path"
#  endif
#else
#  include <X11/Xlib.h>
#endif

#define WSCOUNT 9 /* number of workspaces ("tags") */
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define LENGTH(x) (sizeof(x) / sizeof((x)[0]))

typedef union {
	int i;
	unsigned int ui;
	float f;
	const void *v;
} Arg;

typedef struct {
	unsigned int mod;
	KeySym keysym;
	void (*func)(const Arg *arg);
	Arg arg;
} Key;

typedef struct {
	unsigned int mod;
	unsigned int button;
	void (*func)(const Arg *arg);
	Arg arg;
} Button;

typedef struct Client Client;
struct Client {
	Window win;
	int x, y, w, h;         /* current geometry */
	int bw;                 /* border width */
	int floating;
	int workspace;          /* 0..WSCOUNT-1 */
	Client *next;           /* global insertion-ordered list; tiling order
	                          * within a workspace follows this order */
};

typedef enum {
	LAYOUT_TILE,    /* dwm-style master-stack */
	LAYOUT_MONOCLE, /* one window fills the area, others stacked behind */
	LAYOUT_GRID,    /* monsterwm/frankenwm-style even grid */
	LAYOUT_COUNT
} LayoutType;

typedef struct {
	double master_ratio; /* fraction of width given to the master column */
	int nmaster;          /* number of clients in the master column */
	LayoutType layout;
} WsState;

/* Runtime appearance config, loaded from ~/.config/zovwm/zovwm.conf (see
 * appconf.c) with these as the hardcoded fallback/seed values. */
typedef struct {
	int gap;
	int border_width;
	int bar_height;
	int master_count;
	double master_ratio;
	LayoutType default_layout;
	char color_focus[16];
	char color_unfocus[16];
	char bar_font[64];
	char bar_color_bg[16];
	char bar_color_fg[16];
	char bar_color_cur[16];
	char bar_color_occupied[16];
	char bar_color_empty[16];
} AppConfig;

extern AppConfig cfg;

typedef struct {
	Display *dpy;
	int screen;
	Window root;
	int sw, sh;           /* screen width/height in pixels */
	Client *clients;      /* head of the global client list */
	Client *focused;      /* focused client, may be NULL */
	int curws;             /* current workspace index */
	WsState ws[WSCOUNT];
	int running;
	Atom wm_protocols, wm_delete_window;
	Cursor cursor_normal;
	Key *keys;             /* loaded from ~/.config/zovwm/keys.conf */
	int nkeys;
} WM;

extern WM wm;

/* client.c */
void manage(Window w);
void unmanage(Client *c, int destroyed);
Client *wintoclient(Window w);
void focus(Client *c);
void unfocus(Client *c, int setfocus);
void resizeclient(Client *c, int x, int y, int w, int h);
void showhideworkspace(void);
void killclient(const Arg *arg);
void togglefloating(const Arg *arg);
void focusstack(const Arg *arg);
void movestack(const Arg *arg);
void setmfact(const Arg *arg);
void view(const Arg *arg);
void tag(const Arg *arg);
void movemouse(const Arg *arg);
void resizemouse(const Arg *arg);
void refreshclients(void); /* reapplies cfg.border_width/color_* to every client, for hot-reload */

/* layout.c */
void arrange(void);
void setlayout(const Arg *arg);

/* bar.c */
void bar_init(void);
void bar_draw(void);
void bar_reload(void); /* re-reads cfg (font/colors/height) without recreating barwin */
void bar_cleanup(void);
Window bar_window(void); /* barwin, for tray.c to reparent icons into */
int bar_right_reserved(void); /* width reserved for the clock, tray icons end here */

/* events.c */
void handleevent(XEvent *ev);
int xerror(Display *dpy, XErrorEvent *ee);
int xerrordummy(Display *dpy, XErrorEvent *ee);
int xerrorstart(Display *dpy, XErrorEvent *ee);

/* keys.c */
void grabkeys(void);

/* keyconf.c */
int keyconf_load(void);          /* 0 = loaded existing file, -1 = no file yet */
void keyconf_seed_defaults(void); /* fills the in-memory bind list from compiled-in defaults */
void keyconf_build_keys(void);    /* (re)builds wm.keys/wm.nkeys from the in-memory bind list */
void keyconf_save(void);          /* writes the in-memory bind list to ~/.config/zovwm/keys.conf */
int keyconf_count(void);
const char *keyconf_combo(int i);
const char *keyconf_label(int i);
void keyconf_set_combo(int i, const char *combo);

/* wizard.c */
void wizard_run(void);

/* appconf.c */
void appconf_load(void);   /* seeds cfg with defaults, then overrides from zovwm.conf; writes the file if missing */
void appconf_reload(void); /* re-reads zovwm.conf into cfg, for hot-reload; does not rewrite the file */

/* tray.c */
void tray_init(void);
void tray_cleanup(void);
void tray_handle_clientmessage(XEvent *ev);
void tray_handle_unmap(Window w);
void tray_handle_destroy(Window w);
int tray_width(void);

/* powermenu.c */
void powermenu_run(const Arg *arg);

/* main.c */
void spawn(const Arg *arg);
void quit(const Arg *arg);
void scan(void);
void die(const char *msg);
void reloadconfig(const Arg *arg);

#endif /* ZOVWM_H */
