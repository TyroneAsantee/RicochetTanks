# Kompilerare
CC = gcc

# Flags
CFLAGS = -Wall -std=c99 -I/mingw64/include/SDL2 -Iinclude
LDFLAGS = -L/mingw64/lib -lmingw32 -lSDL2main -lSDL2 -lSDL2_image

# Filer
SRC = src/main.c src/tank.c src/timer.c src/bullet.c
OBJ = $(SRC:.c=.o)
TARGET = spel.exe

# Bygg-regel
all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Rensa
clean:
	del /Q src\\*.o spel.exe 2>nul || exit 0

