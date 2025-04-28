UNAME := $(shell uname 2>/dev/null || echo Windows)

SRCDIR = ./src
COMMONSRCDIR = ../lib/src
INCDIR = ../lib/include

CC = gcc

ifeq ($(UNAME), Darwin)  # macOS
    INCLUDE = -I/opt/homebrew/include/SDL2 -I$(INCDIR)
    CFLAGS = -g $(INCLUDE) -c
    LDFLAGS = -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_net -lSDL2_ttf
    RM = rm -f
else  # Windows
    INCLUDE = -I"C:/msys64/mingw64/include/SDL2" -I"$(INCDIR)"
    CFLAGS = -g $(INCLUDE) -c
    LDFLAGS = -L"C:/msys64/mingw64/lib" -lmingw32 -lSDL2main -lSDL2 -lSDL2_image -lSDL2_net -lSDL2_ttf -mwindows
    RM = cmd.exe /C del /Q
endif

# Target executable
TARGET = spel

# Object files
OBJECTS = main.o tank.o bullet.o collision.o timer.o

# Default target
all: $(TARGET)

# Link object files to create the executable
$(TARGET): $(OBJECTS)
	$(CC) -o "$(TARGET)" $(OBJECTS) $(LDFLAGS)

# Compile main.c (client-specific)
main.o: $(SRCDIR)/main.c $(INCDIR)/tank.h $(INCDIR)/bullet.h $(INCDIR)/collision.h $(INCDIR)/timer.h
	$(CC) $(CFLAGS) $(SRCDIR)/main.c

# Compile tank.c (shared code)
tank.o: $(COMMONSRCDIR)/tank.c $(INCDIR)/tank.h
	$(CC) $(CFLAGS) $(COMMONSRCDIR)/tank.c

# Compile bullet.c (shared code)
bullet.o: $(COMMONSRCDIR)/bullet.c $(INCDIR)/bullet.h $(INCDIR)/tank.h
	$(CC) $(CFLAGS) $(COMMONSRCDIR)/bullet.c

# Compile collision.c (shared code)
collision.o: $(COMMONSRCDIR)/collision.c $(INCDIR)/collision.h
	$(CC) $(CFLAGS) $(COMMONSRCDIR)/collision.c

# Compile timer.c (shared code)
timer.o: $(COMMONSRCDIR)/timer.c $(INCDIR)/timer.h
	$(CC) $(CFLAGS) $(COMMONSRCDIR)/timer.c

# Clean up object files and executable
clean:
	-$(RM) $(OBJECTS) $(TARGET).exe