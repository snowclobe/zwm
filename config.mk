CC ?= cc
PKG_CONFIG ?= pkg-config

X11_CFLAGS = $(shell $(PKG_CONFIG) --cflags x11)
X11_LIBS   = $(shell $(PKG_CONFIG) --libs x11)

CFLAGS  = -std=c11 -Wall -Wextra -Wpedantic -O2 $(X11_CFLAGS)
LDFLAGS = $(X11_LIBS) -lpthread -ldl -lm
