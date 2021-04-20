CFLAGS = -Wall -Wpedantic

all: cswave

cswave: cswave.c

cswave.exe: export CC = x86_64-w64-mingw32-gcc
cswave.exe: cswave.c
	$(CC) $(CFLAGS) $(LDFLAGS) $< $(OBJS) $(LDLIBS) -o $@

clean:
	$(RM) *.o
	$(RM) cswave
	$(RM) cswave.exe
