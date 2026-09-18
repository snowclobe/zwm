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

/* layout.c */
void arrange(void);
void setlayout(const Arg *arg);

/* bar.c */
void bar_init(void);
void bar_draw(void);
void bar_cleanup(void);

/* events.c */
void handleevent(XEvent *ev);
int xerror(Display *dpy, XErrorEvent *ee);
int xerrordummy(Display *dpy, XErrorEvent *ee);
int xerrorstart(Display *dpy, XErrorEvent *ee);

/* keys.c */
void grabkeys(void);

/* main.c */
void spawn(const Arg *arg);
void quit(const Arg *arg);
void scan(void);
void die(const char *msg);

#endif /* ZOVWM_H */
