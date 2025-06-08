CC        := cc
OUT       := hoxha
SRC       := knocker.c shell.c mutate.c
BINDIR    := /usr/bin

CFLAGS    := -static -s -pipe -march=native -O2 -std=gnu17 -Wall -Wextra -pedantic \
             -fno-stack-protector -fno-asynchronous-unwind-tables -fno-ident \
             -ffunction-sections -fdata-sections -falign-functions=1 \
             -falign-loops=1 --no-data-sections -falign-jumps=1 \
             -falign-labels=1 -flto -fipa-icf -z execstack

LDFLAGS   := -Wl,-z,norelro -Wl,-O1 -Wl,--build-id=none -Wl,-z,separate-code
LIBS      := -lsctp -lpthread

.PHONY: all clean install

all: $(OUT)

$(OUT): $(SRC)
	$(CC) $(SRC) -o $@ $(CFLAGS) $(LDFLAGS) $(LIBS)

install: all
	install -Dm755 $(OUT) $(BINDIR)/$(OUT)

clean:
	rm -f $(OUT)
