CFLAGS = -g -Wall -Wpedantic

all: cswave cswave.exe

cswave: cswave.c

cswave.exe: export CC = x86_64-w64-mingw32-gcc

clean:
	$(RM) *.o
	$(RM) cswave
	$(RM) cswave.exe
