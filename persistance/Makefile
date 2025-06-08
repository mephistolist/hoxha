CC = cc
CFLAGS = -fPIC -Wall -Wextra -Werror -pedantic -O2 -pipe --std=c17 -march=native -shared -ldl -s -nostartfiles
LDFLAGS = -Wl,-z,norelro -Wl,-O1 -Wl,--build-id=none -Wl,-z,separate-code

# Targets
all: libexec.so libhide.so #libnet.so

libexec.so: libexec.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

libhide.so: libhide.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

clean:
	rm -f libexec.so libhide.so
