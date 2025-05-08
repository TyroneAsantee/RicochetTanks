CC = gcc
CFLAGS = -Wall -g `sdl2-config --cflags` -I../lib/include
LDFLAGS = `sdl2-config --libs` -lSDL2_net
SRC = src/main.c ../lib/src/tank_server.c ../lib/src/wall.c
OUT = serverspel

all: $(OUT)

$(OUT): $(SRC)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	rm -f $(OUT)
	rm -f $(OUT) Info.plist