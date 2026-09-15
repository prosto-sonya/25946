CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -Wpedantic -O2

TARGET = options
SRC = options.c

.PHONY: all clean test

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

test: $(TARGET)
	./$(TARGET) -i -p -d

clean:
	rm -f $(TARGET)

