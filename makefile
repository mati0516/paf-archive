CC = gcc
CFLAGS = -Wall -Wextra -O2 -Wno-format-truncation -I.\libpaf

SRC = \
    libpaf/libpaf_core.c \
    libpaf/libpaf_list.c \
    libpaf/libpaf_extract.c \
    libpaf/libpaf_exists.c \
    libpaf/fnmatch.c

TARGET = test/test_all

$(TARGET): $(SRC) test/test_all.c
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC) test/test_all.c

.PHONY: clean
clean:
	rm -f $(TARGET)
