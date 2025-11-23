# Compiler to use
CC = cc

# Compiler flags:
# -Wall: Enable all standard warnings
# -Wextra: Enable extra warnings
# -s: Strip symbol table (reduces executable size)
CFLAGS = -Wall -Wextra -s

# Check if on macOS and adjust accordingly
UNAME_S := $(shell uname -s)

# Determine platform-specific settings
ifeq ($(UNAME_S),Darwin)
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
    LDFLAGS = -lncurses -lm
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
