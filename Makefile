CC = gcc

CFLAGS = -DDEBUG -Wextra -g -Wno-old-style-definition

TARGET = bin/basm 

SRC = assembler.c main.c util.c objectgen.c

# Default rule
all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET)

