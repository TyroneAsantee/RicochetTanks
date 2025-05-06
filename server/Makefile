CC = gcc
CFLAGS = -Wall -I../lib/include
LDFLAGS = -lSDL2 -lSDL2_net

SRC = src/main.c ../lib/src/server.c ../lib/src/tank_server.c
OUT = server

all: $(OUT)

$(OUT): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(OUT) $(LDFLAGS)

clean:
	rm -f $(OUT)