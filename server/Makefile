CC = gcc
CFLAGS = -Wall -g -I../lib/include -IC:/msys64/mingw64/include/SDL2
LDFLAGS = -LC:/msys64/mingw64/lib -lmingw32 -lSDL2main -lSDL2 -lSDL2_net

SRC = src/main.c ../lib/src/tank_server.c ../lib/src/wall.c ../lib/src/bullet_server.c ../lib/src/collision.c
OUT = serverspel.exe

all: $(OUT)

$(OUT): $(SRC)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	del /Q $(OUT)