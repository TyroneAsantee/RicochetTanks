CC = gcc
CFLAGS = -Wall -Wextra -Iinclude -I/usr/local/include/SDL2 -D_THREAD_SAFE
LDFLAGS = -L/usr/local/lib -lSDL2 -lSDL2_image
SRC = $(wildcard src/*.c)
OBJ = $(SRC:.c=.o)
TARGET = spel

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) -o $@ $^ $(LDFLAGS)

clean:
	rm -f $(OBJ) $(TARGET)