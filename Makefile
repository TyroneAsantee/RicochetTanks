CC = gcc

# Justera dessa till var du har SDL2 på din dator
SDL_PATH = C:/SDL2

INCLUDES = -I$(SDL_PATH)/include -Isrc -Iinclude
CFLAGS = -Wall -Wextra -std=c11 $(INCLUDES)

LDFLAGS = -L$(SDL_PATH)/lib -lmingw32 -lSDL2main -lSDL2 -lSDL2_image

SRC = $(wildcard src/*.c)
OUT = main.exe

all: $(OUT)

$(OUT): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(OUT) $(LDFLAGS)

clean:
	del /Q $(OUT)