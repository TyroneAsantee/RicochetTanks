CC = gcc
CFLAGS = -Wall -Wextra -Iinclude -IC:/msys64/mingw64/include/SDL2
LDFLAGS = -lmingw32 -lSDL2main -lSDL2 -lSDL2_image

SRC = $(wildcard src/*.c)
OBJ = $(SRC:src/%.c=src/%.o)
TARGET = spel.exe

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) -o $@ $^ $(LDFLAGS)

clean:
	del /Q $(OBJ) $(TARGET)
