# Compiler to use
CC = cc

# Compiler flags:
# -Wall: Enable all standard warnings
# -Wextra: Enable extra warnings
# -s: Strip symbol table (reduces executable size)
CFLAGS = -Wall -Wextra -s

# Check if on macOS and adjust accordingly
UNAME_S := $(shell uname -s)

# Check if on NixOS
IS_NIXOS := $(shell if [ -e /etc/NIXOS ]; then echo "1"; elif [ -d /nix ]; then echo "1"; else echo "0"; fi)

# Determine platform-specific settings
ifeq ($(IS_NIXOS), 1)
    # On NixOS, we need to handle the ncurses library differently
    # NixOS typically has ncurses with different library linking requirements
    LDFLAGS = -ltinfow -lncursesw -lm
    INSTALL_DIR = /run/current-system/sw/bin
else ifeq ($(UNAME_S),Darwin)
    # On macOS
    # Try to locate ncurses, with fallbacks for Homebrew installations
    # On macOS, ncurses is usually available by default, but users with Homebrew
    # might have it in /usr/local or /opt/homebrew
    ifneq ("$(wildcard /opt/homebrew/lib/libncurses.dylib)","")
        # Apple Silicon Macs with Homebrew
        LDFLAGS = -L/opt/homebrew/lib -lncurses -lm
        CPPFLAGS = -I/opt/homebrew/include
    else ifneq ("$(wildcard /usr/local/lib/libncurses.dylib)","")
        # Intel Macs with Homebrew
        LDFLAGS = -L/usr/local/lib -lncurses -lm
        CPPFLAGS = -I/usr/local/include
    else
        # Default macOS ncurses
        LDFLAGS = -lncurses -lm
    endif
    INSTALL_DIR = /usr/local/bin
else
    # On Linux and other systems
    # Some systems have tinfo as a separate library
    ifeq ($(shell pkg-config --exists ncurses && echo 1), 1)
        LDFLAGS = $(shell pkg-config --libs ncurses) -lm
    else ifeq ($(shell pkg-config --exists ncursesw && echo 1), 1)
        LDFLAGS = $(shell pkg-config --libs ncursesw) -lm
    else
        # Default fallback
        LDFLAGS = -ltinfo -lncurses -lm
    endif
    INSTALL_DIR = /usr/local/bin
endif

# Name of the executable
TARGET = nimki

# Source files in src directory
SRCS = src/main.c src/editor.c src/ui.c src/fileops.c src/syntax.c src/input.c src/clipboard.c src/filetree.c src/config.c

# Default target: builds the executable
all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(SRCS) -o $(TARGET) $(CFLAGS) $(LDFLAGS)

# Install target: copies the executable to INSTALL_DIR
install: all
	@echo "Installing $(TARGET) to $(INSTALL_DIR)..."
	@mkdir -p $(INSTALL_DIR)
	@install -m 755 $(TARGET) $(INSTALL_DIR)/$(TARGET)
	@echo "$(TARGET) installed successfully."

# Uninstall target: removes the executable from INSTALL_DIR
uninstall:
	@echo "Uninstalling $(TARGET) from $(INSTALL_DIR)..."
	@rm -f $(INSTALL_DIR)/$(TARGET)
	@echo "$(TARGET) uninstalled."

# Clean target: removes compiled files
clean:
	@echo "Cleaning up..."
	@rm -f $(TARGET)
	@echo "Clean complete."

.PHONY: all install uninstall clean
