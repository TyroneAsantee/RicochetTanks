CC = clang
CFLAGS = -Wall -Wextra -std=c11 -I/opt/homebrew/include/SDL2 -Iinclude
LDFLAGS = -L/opt/homebrew/lib -lSDL2 -lSDL2_image

SRC = $(wildcard src/*.c)
OUT = main

all: $(OUT)

$(OUT): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(OUT) $(LDFLAGS)

clean:
	rm -f $(OUT)
