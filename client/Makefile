CC = gcc
CFLAGS = -Wall -Wextra \
         -I../lib/include \
         -I/usr/local/include/SDL2 \
         -D_THREAD_SAFE

LDFLAGS = -L/usr/local/lib -lSDL2 -lSDL2_image -lSDL2_net

LIB_SRC = $(wildcard ../lib/src/*.c)
CLIENT_SRC = $(wildcard src/*.c)
OBJ = $(CLIENT_SRC:.c=.o) $(LIB_SRC:.c=.o)

TARGET = spel

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	rm -f $(OBJ) $(TARGET)