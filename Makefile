CC = cc
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
SRCS = src/main.c src/editor.c src/ui.c src/fileops.c src/syntax.c src/input.c src/clipboard.c src/filetree.c src/config.c

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(SRCS) -o $(TARGET) $(CFLAGS) $(LDFLAGS)

install: all
	@mkdir -p $(INSTALL_DIR)
	@install -m 755 $(TARGET) $(INSTALL_DIR)/$(TARGET)

uninstall:
	@rm -f $(INSTALL_DIR)/$(TARGET)

clean:
	@rm -f $(TARGET)

.PHONY: all install uninstall clean
