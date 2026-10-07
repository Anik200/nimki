ifeq ($(OS),Windows_NT)
    CC = gcc
    TARGET = nimki.exe
    PDCURSES_DIR = vendor/pdcurses
    CFLAGS = -Wall -Wextra -s -Isrc -I$(PDCURSES_DIR)
    LDFLAGS = $(PDCURSES_DIR)/wincon/pdcurses.a
else
    CC ?= cc
    CFLAGS = -Wall -Wextra -s -Isrc
    LDFLAGS = -lncurses -lm
    TARGET = nimki
    INSTALL_DIR = /usr/local/bin
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
