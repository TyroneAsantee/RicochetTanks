CC = gcc
CFLAGS = -Wall -Wextra -I../lib/include `sdl2-config --cflags`
LDFLAGS = `sdl2-config --libs` -lSDL2_image -lSDL2_net -lSDL2_ttf
SRC = $(wildcard ../lib/src/*.c)
OBJ = $(SRC:.c=.o)
TARGET = spel

all: $(TARGET)

$(TARGET): main.o $(OBJ)
	$(CC) -o $@ $^ $(LDFLAGS)

main.o: src/main.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) main.o ../lib/src/*.o