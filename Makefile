.PHONY: all example

CC=gcc
CFLAGS=--std=c99 -pedantic -Wall -Wextra -Wshadow -Wno-unused-function

all: example

example:
	${CC} ${CFLAGS} -g3 -O0 -DDEBUG=1 -DPRINT_TOKENS=1                           \
        -march=native                                                            \
        -fsanitize=address,undefined -fno-omit-frame-pointer -fstrict-aliasing   \
        example.c -o example

clean:
	rm -rf example
