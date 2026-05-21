.PHONY: all build clean test test_st7735 test_pico2

CC=gcc
CFLAGS=-std=c11 -Wall -Wextra -Iinclude
LDLIBS=-lgpiod
SRC=src/main.c src/sensor_sht35.c src/sensor_pico.c src/logger.c src/alert.c src/st7735.c
OBJ=$(SRC:.c=.o)
TARGET=pi_lab

all: build

build: $(OBJ)
	$(CC) $(OBJ) -o $(TARGET) $(LDLIBS)

test: test/test_sht35.c src/sensor_sht35.o
	$(CC) $(CFLAGS) $^ -o test/test_sht35

test_st7735: test/test_st7735.c src/st7735.o
	$(CC) $(CFLAGS) $^ -o test/test_st7735 $(LDLIBS)

test_pico2: test/test_pico2.c src/sensor_pico.o
	$(CC) $(CFLAGS) $^ -o test/test_pico2

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(TARGET) test/test_sht35 test/test_st7735 test/test_pico2
