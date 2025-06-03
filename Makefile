CC = cc
CFLAGS = -static -s -pipe -march=native -O2 --std=gnu17 -Wall -Wextra -pedantic \
         -fno-stack-protector -fno-asynchronous-unwind-tables -fno-ident \
         -ffunction-sections -fdata-sections -falign-functions=1 \
         -falign-loops=1 --no-data-sections -falign-jumps=1 \
         -falign-labels=1 -flto -fipa-icf -z execstack

LDFLAGS = -Wl,-z,norelro -Wl,-O1 -Wl,--build-id=none -Wl,-z,separate-code

LIBS = -lsctp -lpthread

SRC = knocker.c shell.c mutate.c
OUT = hoxha

all:
	$(CC) $(SRC) -o $(OUT) $(CFLAGS) $(LDFLAGS) $(LIBS)

clean:
	rm -f $(OUT)
