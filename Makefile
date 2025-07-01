CC = gcc

CFLAGS = -DDEBUG -Wextra -g -I./

TARGET = bin/basm 


BUILD_DIR = build
SRC_DIR = src

INSTR_INPUT = x86/instructions.c
INSTR_OUTPUT = $(BUILD_DIR)/instructions.o

SRCS := $(wildcard $(SRC_DIR)/*.c)
OBJS := $(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/%.o, $(SRCS))

all: $(TARGET)

$(TARGET): $(OBJS) $(INSTR_OUTPUT)
	$(CC) $(OBJS) $(INSTR_OUTPUT) -o $@

# Compile each .c file into .o in build directory
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(INSTR_OUTPUT): $(INSTR_INPUT) x86/types.h 
	$(CC) $(CFLAGS) -Wno-old-style-definition -c $(INSTR_INPUT) -o $(INSTR_OUTPUT)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

clean:
	rm -rf $(BUILD_DIR) $(BIN)
