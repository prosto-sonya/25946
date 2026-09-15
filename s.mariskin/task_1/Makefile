CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -Wpedantic -O2

TARGET = options

.PHONY: all clean

all: $(TARGET)

$(TARGET): options.c
	$(CC) $(CFLAGS) -o $(TARGET) options.c

clean:
	rm -f $(TARGET)
