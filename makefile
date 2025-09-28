CC = gcc
CFLAGS = -O2 -Wall -Wextra
INCLUDES = -Ilibpaf

LIBSRC := $(wildcard libpaf/*.c)

all: libpaf.so libpaf.dylib libpaf.dll

libpaf.so: $(LIBSRC)
	$(CC) -shared -fPIC $(CFLAGS) $(INCLUDES) -o libpaf/libpaf.so $^

libpaf.dylib: $(LIBSRC)
	$(CC) -dynamiclib $(CFLAGS) $(INCLUDES) -o libpaf/libpaf.dylib $^

libpaf.dll: $(LIBSRC)
	$(CC) -shared $(CFLAGS) $(INCLUDES) -o libpaf/libpaf.dll $^

clean:
	rm -f libpaf/*.so libpaf/*.dylib libpaf/*.dll
