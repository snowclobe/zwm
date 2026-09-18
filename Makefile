include config.mk

BIN = zovwm
SRC = src/main.c src/client.c src/layout.c src/events.c src/keys.c src/bar.c src/keyconf.c src/wizard.c src/appconf.c src/tray.c src/powermenu.c
OBJ = $(SRC:.c=.o)
HDR = src/zovwm.h src/config.h src/zov_layout.h

RUSTDIR       = rust
RUSTLIB       = $(RUSTDIR)/target/release/libzovwm_layout.a
WALLPAPER_BIN = $(RUSTDIR)/target/release/zovwm-wallpaper

.PHONY: all rust test clean

all: $(BIN) $(WALLPAPER_BIN)

$(BIN): $(OBJ) $(RUSTLIB)
	$(CC) -o $@ $(OBJ) $(RUSTLIB) $(LDFLAGS)

%.o: %.c $(HDR)
	$(CC) $(CFLAGS) -c $< -o $@

# Always defer to cargo for freshness checks; a no-op `cargo build` is cheap.
$(RUSTLIB) $(WALLPAPER_BIN): rust
rust:
	cd $(RUSTDIR) && cargo build --release --workspace

test:
	cd $(RUSTDIR) && cargo test --workspace

clean:
	rm -f $(OBJ) $(BIN)
	cd $(RUSTDIR) && cargo clean
