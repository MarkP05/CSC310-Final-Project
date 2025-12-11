# Simple Makefile to build all C programs in the current directory
# Usage:
#  - To build all programs: make
#  - To build with debug info: make DEBUG=1
#  - To clean up binaries: make clean

CC      ?= gcc
CFLAGS  ?= -Wall

SRC := $(wildcard *.c)
EXE := $(SRC:.c=)

ifdef DEBUG
CFLAGS += -DDEBUG
endif

.PHONY: all debug clean

all: $(EXE)

%: %.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $< -o $@ $(LDFLAGS)

clean:
	rm -f $(EXE)