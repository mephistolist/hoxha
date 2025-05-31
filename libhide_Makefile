CC = cc
CFLAGS = -fPIC -Wall -Wextra -Werror -pedantic -O2 -pipe --std=c17 -march=native
LDFLAGS = -shared -ldl -s -nostartfiles
TARGET = libhide.so
SRC = multi.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

clean:
	rm -f $(TARGET)
