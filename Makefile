.PHONY: all build clean test

CC=gcc
CFLAGS=-std=c11 -Wall -Wextra -Iinclude
SRC=src/main.c src/sensor_sht35.c src/logger.c src/alert.c
OBJ=$(SRC:.c=.o)
TARGET=pi_lab

all: build

build: $(OBJ)
	$(CC) $(OBJ) -o $(TARGET)

test: test/test_sht35.c src/sensor_sht35.o
	$(CC) $(CFLAGS) $^ -o test/test_sht35

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(TARGET) test/test_sht35
