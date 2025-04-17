CC = gcc
CFLAGS = -O3 -march=native -Wall -Wextra -std=c11 `sdl2-config --cflags`
# CFLAGS = -O3 -march=native -flto -Wall -Wextra -std=c11 `sdl2-config --cflags`
LDFLAGS = -flto `sdl2-config --libs`
SRC = $(wildcard *.c)
OBJ = $(SRC:.c=.o)
TARGET = GBemu

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $@ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f $(OBJ) $(TARGET)
