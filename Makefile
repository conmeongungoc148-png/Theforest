CC = gcc
CFLAGS = -Wall -Iraylib/include -std=c99 -Wno-missing-braces
LDFLAGS = -Lraylib/lib -lraylib -lopengl32 -lgdi32 -lwinmm

SRC = src/main.c src/game.c src/camera.c src/cJSON.c
OBJ = $(SRC:.c=.o)
TARGET = theforest.exe

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $(TARGET) $(LDFLAGS)

%.o: %.c
	$(CC) -c $< -o $@ $(CFLAGS)

clean:
	del /f src\*.o $(TARGET)

run: all
	.\$(TARGET)
