
CC = gcc
CFLAGS = -Wall -std=c99 $(shell pkg-config --cflags sdl2)
LIBS = $(shell pkg-config --libs sdl2) -lm

SRC = main.c gen.c
OBJ = $(SRC:.c=.o)
TARGET = galaxy

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

clean:
	rm -f $(OBJ) $(TARGET)
