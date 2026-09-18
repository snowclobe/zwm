# zovwm

*([Читать на русском](README.ru.md))*

A minimal tiling window manager for X11/Xorg. The core is C (Xlib, no
reparenting or window decorations, dwm-style); tiling geometry is Rust.
No compositing, no Wayland — just a small binary and a sane set of
keybindings.

## Build and run

### 1. Dependencies

- Xlib (headers and library).
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

```bash
# Fedora
sudo dnf install gcc make pkgconf-pkg-config libX11-devel
```

```bash
# openSUSE
sudo zypper install gcc make pkgconf-pkg-config libX11-devel
```

If you don't have `cargo`/`rustc` yet:

```bash
curl https://sh.rustup.rs -sSf | sh
source "$HOME/.cargo/env"
```

**Portability**: the C core and both Rust crates are plain C11 + Xlib +
POSIX and stable-channel Rust — there's no architecture- or distro-specific
code anywhere (no `#ifdef __aarch64__`, no hardcoded paths beyond the
standard FHS ones like `/usr/local/bin` and `~/.config`). `make` on x86_64
follows the exact same steps as above; the build has only actually been
*run and tested* on arm64 Debian so far (that's the only machine available
while developing it), so if you build on x86_64 or another distro, a
`make && make test` sanity check is worth doing once.

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

## First-run setup wizard

Keybindings are no longer compile-time only. The first time `zovwm` runs
with no `~/.config/zovwm/keys.conf` yet, it opens a small graphical wizard
(own Xlib window, same bare-font drawing as the status bar — see
`src/wizard.c`) listing every default bind. `Up`/`Down` (or `j`/`k`)
selects a row, `Enter` waits for you to press a replacement key
combination (`Esc` cancels just that one rebind), `S` saves and continues,
`Esc` at the top level keeps the defaults and continues. Either way a
`keys.conf` gets written, so the wizard only ever appears once — delete
that file to see it again on the next login.

The file itself is plain text, one bind per line
(`Mod+Mod+Key action [arg]`, see `src/keyconf.c` for the full action list),
so it's just as easy to hand-edit afterwards as it was to compile-edit
`config.h` before — and edits apply live (see Hot-reload below), so
there's no need to reopen the wizard just to change a bind later.

## Appearance config and hot-reload

`~/.config/zovwm/zovwm.conf` — `key value` lines, same idea as
`keys.conf` — covers everything that used to be compile-time in
`config.h`: `gap`, `border_width`, `bar_height`, `bar_font`,
`bar_color_bg`/`fg`/`cur`/`occupied`/`empty`, `color_focus`/
`color_unfocus`, `master_ratio`, `master_count`, `default_layout`. It's
written out with the current defaults on first run, same as `keys.conf`.
`src/appconf.c` owns loading/parsing it into the global `cfg` (declared in
`zovwm.h`), which every file that used to read `config.h`'s constants
reads from instead.

**Both** config files apply without restarting zovwm:
- `Super+Shift+r` reloads on demand.
- Saving either file from an editor applies it automatically, within
  about a second — `main.c` watches `~/.config/zovwm/` with `inotify`
  (`IN_CLOSE_WRITE`/`IN_MOVED_TO`, so both a plain write and the
  write-to-temp-then-rename pattern most editors use are caught) and
  calls the same reload path. This is Linux-specific; the hotkey works
  everywhere zovwm runs, the automatic trigger doesn't build in on
  non-Linux (it's `#ifdef __linux__`'d out, which only matters for the
  macOS compile-test environment this was developed alongside — the
  runtime target has only ever been Linux/Xorg).

## Keybindings (defaults)

Default modifier is `Super` (Mod4); change it in
[`src/config.h`](src/config.h) (`MODKEY`). The bindings below are the
defaults `keys.conf` is seeded with — remap any of them in the wizard
above, or by editing `~/.config/zovwm/keys.conf` directly.

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
| `Super+Shift+r`    | reload `keys.conf`/`zovwm.conf`               |
| `Super+Shift+p`    | power menu (reboot/shutdown/sleep/logout)     |

Mouse bindings (floating window move/resize) aren't wizard/config-file
driven yet — they're still constants in `src/config.h` (`buttons[]`).

## Status bar

A minimal dwm-style bar, built into the WM itself (not a separate process),
drawn with bare Xlib (`XDrawString` — no Xft/Pango, so non-Latin window
titles, e.g. Cyrillic, may not render correctly with the default core font;
workspace numbers and the clock are always fine). Shows: 9 workspace
indicators (current one highlighted, occupied ones in a different color),
the layout indicator, the focused window's title, docked tray icons, and a
clock. Height, font, and colors come from `cfg` (see Appearance config
above) and reload live along with everything else in it.

## System tray

The bar hosts a standards-compliant `_NET_SYSTEM_TRAY` (XEmbed) tray —
`src/tray.c` — so apps like a NetworkManager or volume applet can dock an
icon into it, the same way they would with any other WM's tray. It reuses
the bar's own window as the tray manager (no extra window), positions
docked icons just left of the clock, and undocks cleanly when the owning
app exits or unmaps its icon.

Tested against a real tray client (`volumeicon-alsa`) end to end: it docks,
renders, and cleanly disappears on quit. XEmbed itself is a real but fiddly
protocol and even mature window managers don't work with every tray-icon
app in the wild — this implements the spec correctly rather than
special-casing individual apps' quirks. If another program already owns
the tray selection (another WM/panel is already acting as the tray host),
zovwm logs that and simply skips the feature rather than fighting over it.

## Power menu

`Super+Shift+p` opens a small graphical menu (`src/powermenu.c`, same
bare-Xlib approach and `cfg` styling as the wizard) with four options:

| Item     | Action                          |
|----------|----------------------------------|
| Reboot   | `systemctl reboot`               |
| Shutdown | `systemctl poweroff`             |
| Sleep    | `systemctl suspend`              |
| Logout   | exits zovwm (ends the X session) |

`Up`/`Down`/`j`/`k` to move, `Enter` to act, `Esc` to cancel. Logout is the
default selection, since it's the least destructive of the four.

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
  and focus management (`client.c`), keybinding grabbing/dispatch
  (`keys.c`), layouts and switching between them (`layout.c`), the status
  bar (`bar.c`), the system tray (`tray.c`), the power menu
  (`powermenu.c`), the runtime keybinding config — action registry,
  defaults, `keys.conf` load/save (`keyconf.c`) — the runtime appearance
  config (`appconf.c`), and the first-run wizard (`wizard.c`).
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
a layout indicator and a system tray, a wallpaper manager (`Super+w`), a
power menu (`Super+Shift+p`), runtime keybinding and appearance config
with a first-run graphical wizard and live hot-reload, single monitor.

Next:
- Mouse-binding remapping (currently still compile-time `config.h`).
- An IPC socket for external control (`i3-msg`-style).
- An unbounded-canvas movement mode — an idea borrowed from driftwm:
  floating windows live on a large virtual canvas, and a hotkey switches
  into panning that canvas instead of fixed workspaces.
- Multi-monitor support via RandR.
