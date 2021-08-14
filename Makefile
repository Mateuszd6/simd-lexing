.PHONY: all debug coverage profile release validate snapshot clear

CC=gcc
CFLAGS=--std=c99 -pedantic -Wextra -Wshadow -Wno-unused-function

all: debug

debug:
	${CC} ${CFLAGS} -g3 -O0                                                      \
        -DDEBUG=1 -DPRINT_TOKENS=1 -DPRINT_LINES=1                               \
        -march=native                                                            \
        -fsanitize=address,undefined -fno-omit-frame-pointer -fstrict-aliasing   \
        main.c -o main

coverage:
	${CC} ${CFLAGS} -g3 -O0                                                      \
        -DRELEASE=1 -DPRINT_TOKENS=1                                             \
        -march=native                                                            \
        -fno-omit-frame-pointer -fstrict-aliasing                                \
        main.c -o main

profile:
	${CC} ${CFLAGS} -g -O3                                                          \
        -DRELEASE=1                                                              \
        -march=native                                                            \
        -fstrict-aliasing                                                        \
        main.c -o main
	perf record -g ./main ./test2.c

release:
	${CC} ${CFLAGS} -O3                                                          \
        -DRELEASE=1                                                              \
        -march=native                                                            \
        -fstrict-aliasing                                                        \
        main.c -o main

validate: coverage
	@./validate.sh

snapshot: coverage
	@./validate.sh --snapshot

bench: release
	@./run.sh

clean:
	rm -rf main
