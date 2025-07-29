# Makefile for building test_all on Linux/macOS

CC = gcc
CFLAGS = -Wall -Wextra -O2 -Wno-format-truncation
INCLUDES = -Ilibpaf
SRC = libpaf/libpaf.c
TEST = test/test_all.c
OUT = test/test_all

all: $(OUT)

$(OUT): $(SRC) $(TEST)
	$(CC) $(CFLAGS) $(INCLUDES) -o $(OUT) $(SRC) $(TEST)

clean:
	rm -f $(OUT)
