# Makefile for The Forest (Integrated Map System)

CC = gcc
CFLAGS = -Wall -Iraylib/include -std=c99 -Wno-missing-braces
LDFLAGS = -Lraylib/lib -lraylib -lopengl32 -lgdi32 -lwinmm

# Tệp nguồn chính
SRC = src/main.c src/game.c src/camera.c src/map.c
OBJ = $(SRC:.c=.o)
EXE = theforest.exe

all: $(EXE)

$(EXE): $(OBJ)
	$(CC) $(OBJ) -o $(EXE) $(LDFLAGS)

run: $(EXE)
	.\$(EXE)

%.o: %.c
	$(CC) -c $< -o $@ $(CFLAGS)

clean:
	del /f src\*.o $(EXE)
