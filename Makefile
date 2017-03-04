-include config.mak

CC ?= cc
PREFIX ?= /usr/local
CONFIG_CFLAGS ?= -O3 -std=gnu11
CFLAGS := $(CONFIG_CFLAGS) $(CFLAGS)

export CC
export CFLAGS

FUSE_FLAGS = -DFUSE_USE_VERSION=28 -D_FILE_OFFSET_BITS=64
FUSE_LIB = -lfuse
OS := $(shell uname)
ifeq ($(OS), Darwin)
	FUSE_FLAGS += -I/usr/local/include/osxfuse
	FUSE_LIB = -losxfuse
else ifeq ($(OS), FreeBSD)
	FUSE_FLAGS += -I/usr/local/include
	FUSE_LIB += -L/usr/local/lib
endif

SRCS = $(wildcard src/*.c)
OBJS = $(SRCS:%.c=%.o)
TARGET = listfs

.PHONY: all clean config install uninstall

all: $(TARGET)

%.o: %.c
	$(CC) $(CFLAGS) $(FUSE_FLAGS) $(INCLUDE) -c -o $*.o $^

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(FUSE_LIB) -o $@ $^ -lpthread

clean:
	rm -f $(OBJS) $(TARGET)

install: $(TARGET)
	install $< $(PREFIX)/bin/

uninstall:
	rm -f $(PREFIX)/bin/$(TARGET)

config:
	echo CC=$(CC) > config.mak
	echo CONFIG_CFLAGS=$(CFLAGS) >> config.mak
