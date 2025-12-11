# Makefile for NETTF File Transfer Tool
# This Makefile provides cross-platform build automation for Linux/macOS systems
# It supports compilation, cleaning, and installation of the NETTF file transfer tool

# Compiler configuration
CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -O2

# Directory and file configuration
TARGET = nettf
SRCDIR = src
OBJDIR = obj

# File discovery using pattern matching
SOURCES = $(wildcard $(SRCDIR)/*.c)
OBJECTS = $(SOURCES:$(SRCDIR)/%.c=$(OBJDIR)/%.o)

# Default target: Build the complete project
all: $(TARGET)

# Linking step: Create executable from object files
$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $(TARGET)

# Compilation rule: Create object files from source files
$(OBJDIR)/%.o: $(SRCDIR)/%.c
	@mkdir -p $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Clean target: Remove all generated files
clean:
	rm -rf $(OBJDIR) $(TARGET)

# Install target: Copy executable to system path (requires sudo)
install: $(TARGET)
	cp $(TARGET) /usr/local/bin/

# Uninstall target: Remove executable from system path
uninstall:
	rm -f /usr/local/bin/$(TARGET)

# Phony targets: These don't correspond to actual files
.PHONY: all clean install uninstall