# Compiler
CC := gcc

# Project
TARGET := ConwayGoLRGSMLC
SRC_DIR := src
OBJ_DIR := obj
INPUT_DIR := inputs

# Kissat
KISSAT_DIR := kissat
KISSAT_INCLUDE := $(KISSAT_DIR)/src
KISSAT_LIB := $(KISSAT_DIR)/build/libkissat.a

# Compilation flags
CPPFLAGS := -I$(KISSAT_INCLUDE)
CFLAGS := -std=c11 -Wall -Wextra -Wpedantic -O2 -MMD -MP
LDLIBS := $(KISSAT_LIB) -lm

# Source, object and dependency files
SRCS := $(wildcard $(SRC_DIR)/*.c)
OBJS := $(SRCS:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)
DEPS := $(OBJS:.o=.d)

.PHONY: all clean clean_kissat install_kissat run

all: $(TARGET)

# Link executable
$(TARGET): $(OBJS) $(KISSAT_LIB)
	$(CC) $(OBJS) -o $@ $(LDLIBS)

# Compile source files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

# Create object directory
$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

# Ensure Kissat is available
$(KISSAT_LIB):
	@echo "Error: Kissat is not installed."
	@echo "Run 'make install_kissat' first."
	@false

# Clone and build Kissat
install_kissat:
	@if [ ! -d "$(KISSAT_DIR)" ]; then \
		git clone https://github.com/arminbiere/kissat.git $(KISSAT_DIR); \
	else \
		echo "Kissat source already exists."; \
	fi
	cd $(KISSAT_DIR) && ./configure && $(MAKE)

# Run the example input
run: $(TARGET)
	./$(TARGET) < $(INPUT_DIR)/inputEnunciado.txt

# Remove project build artifacts
clean:
	rm -rf $(OBJ_DIR)
	rm -f $(TARGET)

# Clean Kissat build artifacts
clean_kissat:
	@if [ -d "$(KISSAT_DIR)" ]; then \
		cd $(KISSAT_DIR) && $(MAKE) clean; \
	fi

# Automatically generated header dependencies
-include $(DEPS)