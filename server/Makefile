CC = gcc

CFLAGS = -Wall -I../lib/include
LDFLAGS = -LC:/msys64/mingw64/lib -lmingw32 -lSDL2main -lSDL2 -lSDL2_net 

SRC = src/main.c ../lib/src/tank_server.c ../lib/src/wall.c
OUT = server.exe

all: $(OUT)

$(OUT): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(OUT) $(LDFLAGS)

clean:
	del /Q $(OUT)   