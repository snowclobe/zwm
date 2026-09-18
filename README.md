# zwm

*([Читать на русском](README.ru.md))*

A minimal tiling window manager for X11/Xorg. The core is C (Xlib, no
reparenting or window decorations, dwm-style); tiling geometry is Rust.
No compositing, no Wayland — just a small binary and a sane set of
keybindings.

## Build and run

### 1. Dependencies

- Xlib (headers and library): Debian/Ubuntu — `libx11-dev`, Arch —
  `libx11`, Fedora — `libX11-devel`.
- `pkg-config`.
- A C11-capable compiler (gcc/clang).
- Rust toolchain (`rustc`/`cargo`).

```bash
# Debian/Ubuntu
sudo apt install build-essential libx11-dev pkg-config
```

```bash
# Arch
sudo pacman -S base-devel libx11 pkgconf
```

If you don't have `cargo`/`rustc` yet:

```bash
curl https://sh.rustup.rs -sSf | sh
source "$HOME/.cargo/env"
```

### 2. Build

From the project root:

```bash
make
```

Builds `rust/zovwm-layout` into a static library and links the C core —
the result is the `./zovwm` binary.

```bash
make test   # layout geometry unit tests (rust/zovwm-layout, no X11 needed)
make clean  # removes object files, the binary, and cargo artifacts
```

### 3. Install

```bash
sudo install -Dm755 zovwm /usr/local/bin/zovwm
sudo install -Dm755 rust/target/release/zovwm-wallpaper /usr/local/bin/zovwm-wallpaper
```

The wallpaper manager (`Super+w`, see below) also needs `xwallpaper` (or
`feh`):

```bash
sudo apt install xwallpaper
```

### 4. Run

`zovwm` is an X11 window manager, not a regular app: it has to be launched
as the WM for an X session. Three options, from quickest to most "real":

**Nested X server (Xephyr)** — the fastest way to try it without touching
your current desktop:

```bash
sudo apt install xserver-xephyr xterm
```

```bash
Xephyr :1 -screen 1280x800 &
DISPLAY=:1 zovwm &
DISPLAY=:1 xterm &   # run something inside it
```

**`startx` from a plain text console** — a real X session with no display
manager:

```bash
printf 'exec /usr/local/bin/zovwm\n' > ~/.xinitrc
```

(`zovwm` launches `zovwm-wallpaper` itself on startup — no need to add it
to `.xinitrc` separately.)

Then, from a text console (not on top of an already-running GNOME/XFCE —
switch to a TTY first, e.g. `Ctrl+Alt+F3`, or stop the display manager
service: `sudo systemctl stop gdm3`):

```bash
startx
```

**Selectable session in GDM/LightDM/SDDM** — a permanent install alongside
your regular desktop environment:

```bash
sudo install -Dm644 packaging/zovwm.desktop /usr/share/xsessions/zovwm.desktop
```

A "zovwm" entry will show up on the login screen. Note:
`SubstructureRedirectMask` can only be held by one WM at a time, so the
regular WM (GNOME/XFCE, etc.) must not be running at that point — either
stop it, or pick zovwm explicitly on the login screen instead of your
usual session.

> macOS has no native X11 server — building and `cargo test` work there
> (via Homebrew's `libx11`), but an actual interactive WM run needs to be
> tested on Linux with an Xorg session (or via XQuartz+Xephyr).

## Keybindings (MVP)

Default modifier is `Super` (Mod4). Change it in [`src/config.h`](src/config.h).

| Key                | Action                                       |
|--------------------|-----------------------------------------------|
| `Super+Return`     | launch a terminal (`xterm`)                   |
| `Super+p`          | launch a launcher (`dmenu_run`)               |
| `Super+d`          | launch `rofi -show drun`                      |
| `Super+j` / `k`    | move focus forward/backward through the stack |
| `Super+Shift+j/k`  | move the window forward/backward in the stack |
| `Super+h` / `l`    | shrink/grow the master column                 |
| `Super+f`          | toggle floating for the focused window        |
| `Super+t`          | tile layout (master-stack)                    |
| `Super+m`          | monocle layout (one window fills the screen)  |
| `Super+g`          | grid layout                                   |
| `Super+Shift+q`    | close the focused window                      |
| `Super+1..9`       | switch to workspace 1..9                      |
| `Super+Shift+1..9` | move the focused window to workspace 1..9     |
| `Super+Shift+e`    | quit zovwm                                    |
| `Super`+drag LMB   | move a floating window (tiled ones auto-float)|
| `Super`+drag RMB   | resize a floating window                      |
| `Super+w`          | fetch and set a new random wallpaper          |

Launch commands (terminal, launcher), border colors, the gap between
windows (`GAP`), and the keybinding/mouse-binding tables themselves are
constants in `src/config.h`; a real config file is a later milestone (see
roadmap).

## Status bar

A minimal dwm-style bar, built into the WM itself (not a separate process),
drawn with bare Xlib (`XDrawString` — no Xft/Pango, so non-Latin window
titles, e.g. Cyrillic, may not render correctly with the default core font;
workspace numbers and the clock are always fine). Shows: 9 workspace
indicators (current one highlighted, occupied ones in a different color),
the focused window's title, and a clock. Height, font, and colors are in
`src/config.h` (`BARHEIGHT`, `barfont`, `barcol_*`).

## Wallpaper manager

`zovwm-wallpaper` is a separate Rust binary (`rust/zovwm-wallpaper/`), not
linked into the WM core — it runs as a regular program via `spawn()` (like
the terminal or rofi). It fetches a random image from
[Wallhaven](https://wallhaven.cc)'s open API (no key, no signup required),
caches it at `~/.cache/zovwm/wallpaper.*`, and sets it via `xwallpaper
--zoom` (or `feh --bg-fill` if `xwallpaper` isn't installed). Networking
goes through the system `curl` binary rather than a Rust TLS stack, to
avoid an extra dependency.

Wallhaven's `general` category + `purity=sfw` alone don't guarantee
appropriate content on their own (the general category turned up fairly
explicit character-art renders in testing) — so the query is additionally
pinned to a topic (`q=`), picked at random on each run from a list in
`rust/zovwm-wallpaper/src/main.rs` (`nature`, `landscape`, `mountains`,
`space`, `architecture`, `minimal`, `ocean`, `forest`). That list isn't
set in stone — extend or change it to taste.

If there's no network or Wallhaven is unreachable, it falls back to
whatever's already cached instead of leaving a blank screen.

Built and installed the same way as `zovwm` — `make` builds the whole Rust
workspace in one call (`cargo build --release --workspace`), and the
binary is installed alongside it:

```bash
sudo install -Dm755 rust/target/release/zovwm-wallpaper /usr/local/bin/zovwm-wallpaper
```

Runs automatically on WM startup (see `setup()` in `src/main.c`), and
manually via `Super+w`.

## Architecture

- `src/` — the C core: the X11 event loop (`main.c`, `events.c`), client
  and focus management (`client.c`), keybinding dispatch (`keys.c`),
  layouts and switching between them (`layout.c`), the status bar
  (`bar.c`).
- `rust/zovwm-layout/` — pure layout geometry (master-stack and grid;
  monocle is a trivial case computed directly in C), no X11, no side
  effects, built as a static library and called from `layout.c` over FFI
  (`src/zov_layout.h`).
- `rust/zovwm-wallpaper/` — a separate executable Rust crate (not FFI),
  the wallpaper manager — see the section above.

## Roadmap

Done (MVP): tile/monocle/grid layouts (`Super+t/m/g`), focus (keyboard +
hover), floating toggle, moving/resizing floating windows with the mouse,
9 workspaces, launching apps (including rofi), a built-in status bar with
a layout indicator, a wallpaper manager (`Super+w`), single monitor.

Next:
- A Rust (TOML) config file instead of the static `config.h`, reloadable
  on a signal.
- An IPC socket for external control (`i3-msg`-style).
- An unbounded-canvas movement mode — an idea borrowed from driftwm:
  floating windows live on a large virtual canvas, and a hotkey switches
  into panning that canvas instead of fixed workspaces.
- Multi-monitor support via RandR.
