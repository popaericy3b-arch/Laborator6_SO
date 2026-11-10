CC     = gcc
CFLAGS = -Wall -Wextra -O2

all: linux/contor

linux/contor: linux/contor.c
	$(CC) $(CFLAGS) $< -o $@ -pthread

windows: windows/contor.c
	x86_64-w64-mingw32-gcc $(CFLAGS) $< -o windows/contor.exe

clean:
	rm -f linux/contor windows/contor.exe

.PHONY: all windows clean
