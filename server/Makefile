CC = gcc

SRC = src/main.c ../lib/src/tank_server.c ../lib/src/wall.c 
CFLAGS = -Wall -g `sdl2-config --cflags` -I../lib/include
LDFLAGS = `sdl2-config --libs` -lSDL2_net -lSDL2_ttf
OUT = serverspel

all: $(OUT)

$(OUT): $(SRC)
	$(CC) $(CFLAGS) -o $(OUT) $(SRC) $(LDFLAGS)

clean:
	rm -f $(OUT)
	find . -name "*.o" -delete
	find . -name "*.dSYM" -exec rm -rf {} +
	find . -name ".DS_Store" -delete
	find . -name "*.dll" -delete