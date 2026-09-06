ifeq ($(OS),Windows_NT)
    CC = gcc
    TARGET = nimki.exe
    PDCURSES_DIR = vendor/pdcurses
    CFLAGS = -Wall -Wextra -s -Isrc -I$(PDCURSES_DIR)
    LDFLAGS = $(PDCURSES_DIR)/wincon/pdcurses.a
else
    CC ?= cc
    CFLAGS = -Wall -Wextra -s
    UNAME_S := $(shell uname -s)
    IS_NIXOS := $(shell if [ -e /etc/NIXOS ]; then echo "1"; elif [ -d /nix ]; then echo "1"; else echo "0"; fi)

    ifeq ($(IS_NIXOS), 1)
        LDFLAGS = -ltinfow -lncursesw -lm
        INSTALL_DIR = /run/current-system/sw/bin
    else ifeq ($(UNAME_S),Darwin)
        ifneq ("$(wildcard /opt/homebrew/lib/libncurses.dylib)","")
            LDFLAGS = -L/opt/homebrew/lib -lncurses -lm
            CPPFLAGS = -I/opt/homebrew/include
        else ifneq ("$(wildcard /usr/local/lib/libncurses.dylib)","")
            LDFLAGS = -L/usr/local/lib -lncurses -lm
            CPPFLAGS = -I/usr/local/include
        else
            LDFLAGS = -lncurses -lm
        endif
        INSTALL_DIR = /usr/local/bin
    else
        ifeq ($(shell pkg-config --exists ncurses && echo 1), 1)
            LDFLAGS = $(shell pkg-config --libs ncurses) -lm
        else ifeq ($(shell pkg-config --exists ncursesw && echo 1), 1)
            LDFLAGS = $(shell pkg-config --libs ncursesw) -lm
        else
            LDFLAGS = -ltinfo -lncurses -lm
        endif
        INSTALL_DIR = /usr/local/bin
    endif
    TARGET = nimki
endif

SRCS = src/main.c src/editor.c src/ui.c src/fileops.c src/syntax.c src/input.c src/clipboard.c src/filetree.c src/config.c

all: $(TARGET)

ifeq ($(OS),Windows_NT)
$(PDCURSES_DIR)/wincon/pdcurses.a:
	@if not exist vendor mkdir vendor
	@if not exist $(PDCURSES_DIR) git clone --depth 1 https://github.com/wmcbrine/PDCurses.git $(PDCURSES_DIR)
	$(MAKE) -C $(PDCURSES_DIR)/wincon WIDE=N

$(TARGET): $(PDCURSES_DIR)/wincon/pdcurses.a $(SRCS)
	$(CC) $(SRCS) -o $(TARGET) $(CFLAGS) $(LDFLAGS)

clean:
	@if exist $(TARGET) del /f $(TARGET)
else
$(TARGET): $(SRCS)
	$(CC) $(SRCS) -o $(TARGET) $(CFLAGS) $(LDFLAGS)

install: all
	@mkdir -p $(INSTALL_DIR)
	@install -m 755 $(TARGET) $(INSTALL_DIR)/$(TARGET)

uninstall:
	@rm -f $(INSTALL_DIR)/$(TARGET)

clean:
	@rm -f $(TARGET)
endif

.PHONY: all install uninstall clean
