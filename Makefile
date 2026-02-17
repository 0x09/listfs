-include config.mak

PREFIX ?= /usr/local
CONFIG_CFLAGS ?= -O3 -std=gnu11
CFLAGS := $(CONFIG_CFLAGS) $(CFLAGS)

export CC
export CFLAGS

FUSE_FLAGS = -DFUSE_USE_VERSION=35 -D_FILE_OFFSET_BITS=64
LDLIBS = -lfuse3
OS := $(shell uname)
ifeq ($(OS), Darwin)
	FUSE_FLAGS += -I/usr/local/include
	FUSE_LIB += -L/usr/local/lib
endif

CPPFLAGS := $(FUSE_FLAGS) $(CPPFLAGS)
LDFLAGS := $(FUSE_LIB) $(LDFLAGS)

SRCS = $(wildcard src/*.c)
OBJS = $(SRCS:%.c=%.o)
TARGET = listfs

vpath %.o src

.PHONY: all clean config install uninstall

all: $(TARGET)

listfs: src/listfs.o

clean:
	$(RM) $(OBJS) $(TARGET)

install: $(TARGET)
	install $< $(PREFIX)/bin/

uninstall:
	$(RM) $(PREFIX)/bin/$(TARGET)

config:
	echo CC=$(CC) > config.mak
	echo CONFIG_CFLAGS=$(CFLAGS) >> config.mak
