.PHONY: example

CC        = gcc
OBJS      = src/lfqueue.o
HEADERS   = include/lfqueue.h
EXAMPLE   = example
LIB_NAME  = lfqueue
LIB_DIR   = lib$(LIB_NAME)
TARGETS   = src/lib$(LIB_NAME).a
INCLUDE   = include
CFLAGS    = -Wall -Wextra -pedantic -std=c11
DIST      = dist

-include site.mk

ifeq ($(DEBUG), 0)
    CFLAGS += -O2 -g0
else
    CFLAGS += -O0 -ggdb
endif

ifeq ($(ENABLE_SA), 1)
    CFLAGS += -fanalyzer
    CFLAGS += --analyzer
endif

ifeq ($(ENABLE_ASAN), 1)
    CFLAGS += -fsanitize=address -fsanitize=leak
endif

ifeq ($(ENABLE_UBSAN), 1)
    CFLAGS += -fsanitize=undefined
endif

ifeq ($(ENABLE_TSAN), 1)
    CFLAGS += -fsanitize=thread
endif

INSTALL_PATH ?= /opt/share

export DIST
export CFLAGS
export LIB_NAME
export INCLUDE
export CC

all: dist example

install: dist
	mkdir -p $(INSTALL_PATH)/$(LIB_DIR)
	cp -rf $(DIST)/* $(INSTALL_PATH)/$(LIB_DIR)

dist: $(TARGETS)
	mkdir -p $(DIST)/include
	mkdir -p $(DIST)/lib
	cp -r $(HEADERS) $(DIST)/include
	cp $(TARGETS) $(DIST)/lib

$(TARGETS): $(OBJS)
	ar -cr $(TARGETS) $(OBJS) 

example:
	$(MAKE) -C $(EXAMPLE)

%.o: %.c
	$(CC) -c -o $@ -I$(INCLUDE) $(CFLAGS) $^

clean: 
	$(MAKE) -C $(EXAMPLE) clean
	rm -rf $(OBJS) $(TARGETS) $(DIST)

uninstall: 
	rm -rf $(INSTALL_PATH)/$(LIB_DIR)
