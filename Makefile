CC = gcc
TARGET = octemu
CFLAGS = -Werror -Wall

.PHONY: clean

$(TARGET): core.c octemu.c
	$(CC) $(CFLAGS) -O3 -g0 core.c octemu.c -lSDL3 -o $@

$(TARGET)-dbg: core.c octemu.c
	$(CC) $(CFLAGS) -Og -g -DOCTEMU_DEBUG core.c octemu.c -lSDL3 -o $@

clean:
	rm -f $(TARGET) $(TARGET)-dbg
