CC = gcc

CFLAGS = -Wall -Wextra -std=c11 \
    -I../lib/include \
    -I/usr/local/include \
    -D_THREAD_SAFE

LDFLAGS = -L../lib/src \
    -L/usr/local/lib \
    -lSDL2 -lSDL2_image -lSDL2_net -lSDL2_ttf

SRC = src/main.c \
      ../lib/src/tank.c \
      ../lib/src/timer.c \
      ../lib/src/bullet.c \
      ../lib/src/collision.c \
      ../lib/src/text.c \
      ../lib/src/wall.c

OBJ = $(SRC:.c=.o)
EXEC = spel

all: $(EXEC)

$(EXEC): $(OBJ)
	$(CC) $(OBJ) -o $@ $(LDFLAGS)

clean:
	rm -f $(OBJ) $(EXEC)