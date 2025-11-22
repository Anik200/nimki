# Compiler to use
CC = cc

# Compiler flags:
# -Wall: Enable all standard warnings
# -Wextra: Enable extra warnings
# -s: Strip symbol table (reduces executable size)
CFLAGS = -Wall -Wextra -s

# Linker flags:
# -lncurses: Link against the ncurses library
LDFLAGS = -lncurses -lm

# Name of the executable
TARGET = nimki

# Source files
SRCS = nimki.c

# Directory where the executable will be installed
# /usr/local/bin is a common location for locally installed software
INSTALL_DIR = /usr/local/bin

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
