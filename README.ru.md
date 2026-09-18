# zovwm

*([Read this in English](README.md))*

Минималистичный тайловый оконный менеджер для X11/Xorg. Ядро — на C (Xlib,
без reparenting и декораций окон, в духе dwm), геометрия тайлинга — на Rust.
Никакого композитинга, никакого Wayland — только маленький бинарник и
разумный набор хоткеев.

## Сборка и запуск

### 1. Зависимости

- Xlib (заголовки и библиотека): Debian/Ubuntu — `libx11-dev`, Arch —
  `libx11`, Fedora — `libX11-devel`.
- `pkg-config`.
- C-компилятор с поддержкой C11 (gcc/clang).
- Rust toolchain (`rustc`/`cargo`).

```bash
# Debian/Ubuntu
sudo apt install build-essential libx11-dev pkg-config
```

```bash
# Arch
sudo pacman -S base-devel libx11 pkgconf
```

Если ещё нет `cargo`/`rustc`:

```bash
curl https://sh.rustup.rs -sSf | sh
source "$HOME/.cargo/env"
```

### 2. Сборка

Из корня проекта:

```bash
make
```

Соберёт `rust/zovwm-layout` в статическую библиотеку и слинкует C-ядро —
на выходе бинарник `./zovwm`.

```bash
make test   # юнит-тесты геометрии раскладок (rust/zovwm-layout, без X11)
make clean  # чистит объектники, бинарник и cargo-артефакты
```

### 3. Установка

```bash
sudo install -Dm755 zovwm /usr/local/bin/zovwm
sudo install -Dm755 rust/target/release/zovwm-wallpaper /usr/local/bin/zovwm-wallpaper
```

Для менеджера обоев (`Super+w`, см. ниже) нужен ещё `xwallpaper` (или `feh`):

```bash
sudo apt install xwallpaper
```

### 4. Запуск

`zovwm` — это X11 window manager, не обычное приложение: он должен быть
запущен как WM для X-сессии. Три варианта, от самого простого до самого
«настоящего»:

**Вложенный X-сервер (Xephyr)** — быстрее всего проверить, не трогая
текущий рабочий стол:

```bash
sudo apt install xserver-xephyr xterm
```

```bash
Xephyr :1 -screen 1280x800 &
DISPLAY=:1 zovwm &
DISPLAY=:1 xterm &   # запустить что-нибудь внутри
```

**`startx` с отдельной текстовой консоли** — реальная X-сессия без
display-менеджера:

```bash
printf 'exec /usr/local/bin/zovwm\n' > ~/.xinitrc
```

(`zovwm` сам запускает `zovwm-wallpaper` при старте — отдельно в `.xinitrc`
дописывать не нужно.)

Затем с текстовой консоли (не поверх уже запущенного GNOME/XFCE — нужно
выйти в TTY, например `Ctrl+Alt+F3`, либо остановить сервис самого
display-менеджера: `sudo systemctl stop gdm3`):

```bash
startx
```

**Выбираемая сессия в GDM/LightDM/SDDM** — постоянная установка рядом со
штатным окружением:

```bash
sudo install -Dm644 packaging/zovwm.desktop /usr/share/xsessions/zovwm.desktop
```

После этого на экране логина появится пункт «zovwm». Учтите:
`SubstructureRedirectMask` может держать только один WM одновременно, так
что штатный WM (GNOME/XFCE и т.п.) в этот момент не должен быть запущен —
либо выберите zovwm явно на экране входа вместо обычной сессии.

> На macOS нет родного X11-сервера — сборка и `cargo test` там работают
> (через Homebrew `libx11`), но реальный интерактивный запуск WM нужно
> проверять на Linux с Xorg-сессией (или через XQuartz+Xephyr).

## Хоткеи (MVP)

Модификатор по умолчанию — `Super` (Mod4). Меняется в [`src/config.h`](src/config.h).

| Хоткей            | Действие                                   |
|-------------------|---------------------------------------------|
| `Super+Return`    | запустить терминал (`xterm`)                |
| `Super+p`         | запустить лаунчер (`dmenu_run`)             |
| `Super+d`         | запустить `rofi -show drun`                 |
| `Super+j` / `k`   | переключить фокус вперёд/назад по стеку     |
| `Super+Shift+j/k` | переместить окно в стеке вперёд/назад       |
| `Super+h` / `l`   | уменьшить/увеличить master-колонку          |
| `Super+f`         | переключить floating для фокусного окна     |
| `Super+t`         | раскладка tile (master-stack)               |
| `Super+m`         | раскладка monocle (одно окно на весь экран) |
| `Super+g`         | раскладка grid (сетка)                      |
| `Super+Shift+q`   | закрыть фокусное окно                       |
| `Super+1..9`      | переключиться на workspace 1..9             |
| `Super+Shift+1..9`| перенести фокусное окно на workspace 1..9   |
| `Super+Shift+e`   | выйти из zovwm                              |
| `Super`+ЛКМ (drag)| двигать floating-окно (тайловое авто-флоатится) |
| `Super`+ПКМ (drag)| ресайзить floating-окно                     |
| `Super+w`         | скачать и поставить новые случайные обои    |

Команды запуска (терминал, лаунчер), цвета рамки, зазор между окнами (`GAP`)
и сама таблица хоткеев/кнопок мыши — константы в `src/config.h`; конфиг-файл
появится позже (см. roadmap).

## Статус-бар

Минимальный бар в духе dwm, встроен в сам WM (не отдельный процесс), рисуется
голым Xlib (`XDrawString`, без Xft/Pango — поэтому заголовки на кириллице
могут отображаться некорректно с дефолтным core-шрифтом, цифры workspace'ов
и часы всегда ок). Показывает: 9 workspace-индикаторов (подсветка текущего,
отдельный цвет для занятых), заголовок фокусного окна, часы. Высота, шрифт
и цвета — в `src/config.h` (`BARHEIGHT`, `barfont`, `barcol_*`).

## Менеджер обоев

`zovwm-wallpaper` — отдельный Rust-бинарник (`rust/zovwm-wallpaper/`), не
слинкован в само ядро WM, запускается как обычная программа через `spawn()`
(как терминал или rofi). Тянет случайную картинку с открытого API
[Wallhaven](https://wallhaven.cc) (без ключа, без регистрации), кэширует в
`~/.cache/zovwm/wallpaper.*` и ставит через `xwallpaper --zoom` (либо `feh
--bg-fill`, если `xwallpaper` не найден). Сеть — через системный `curl`, а
не Rust-TLS-стек, чтобы не тащить лишнюю зависимость.

Категория `general` + `purity=sfw` на Wallhaven сами по себе не гарантируют
адекватный контент (в общей категории попадается и весьма откровенный
рендер-арт персонажей) — поэтому запрос дополнительно закреплён темой
(`q=`), которая на каждый вызов случайно берётся из списка в
`rust/zovwm-wallpaper/src/main.rs` (`nature`, `landscape`, `mountains`,
`space`, `architecture`, `minimal`, `ocean`, `forest`). Список — не
хардкод-на-века, дополняйте/меняйте под себя.

Если сети нет или Wallhaven недоступен — используются уже скачанные обои
из кэша, а не чёрный экран.

Ставится и запускается точно так же, как `zovwm` — `make` собирает весь
Rust-workspace одним вызовом (`cargo build --release --workspace`), бинарник
устанавливается рядом:

```bash
sudo install -Dm755 rust/target/release/zovwm-wallpaper /usr/local/bin/zovwm-wallpaper
```

Запускается автоматически при старте WM (см. `setup()` в `src/main.c`) и по
`Super+w` вручную.

## Архитектура

- `src/` — C-ядро: цикл событий X11 (`main.c`, `events.c`), управление
  клиентами и фокусом (`client.c`), диспетч хоткеев (`keys.c`), раскладки
  и переключение между ними (`layout.c`), статус-бар (`bar.c`).
- `rust/zovwm-layout/` — чистая геометрия раскладок (master-stack и grid;
  monocle — тривиальный случай, посчитан прямо в C), без X11, без
  сайд-эффектов, собирается в статическую библиотеку и вызывается из
  `layout.c` через FFI (`src/zov_layout.h`).
- `rust/zovwm-wallpaper/` — отдельный исполняемый Rust-крейт (не FFI),
  менеджер обоев — см. раздел выше.

## Roadmap

Реализовано (MVP): раскладки tile/monocle/grid (`Super+t/m/g`), фокус
(клавиатура + hover), floating toggle, перемещение/ресайз floating окон
мышью, 9 workspaces, запуск приложений (включая rofi), встроенный
статус-бар с индикатором раскладки, менеджер обоев (`Super+w`), один
монитор.

Дальше:
- Конфиг-файл на Rust (TOML) вместо статического `config.h`, с перечитыванием
  по сигналу.
- IPC-сокет для внешнего управления (в духе `i3-msg`).
- Режим безграничного перемещения по столу — идея, заимствованная у
  driftwm: плавающие окна живут на большом виртуальном холсте, а хоткей
  переключает в режим панорамирования этого холста вместо фиксированных
  workspaces.
- Мультимониторность через RandR.
