CC = gcc
TARGET = octemu
CFLAGS = -Werror -Wall -O3

.PHONY: clean

$(TARGET): core.c octemu.c
	$(CC) $(CFLAGS) core.c octemu.c -lSDL3 -o $@

clean:
	rm -f $(TARGET)
