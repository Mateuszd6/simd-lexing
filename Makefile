all:
	gcc -O0 -g3 -Wall -Wextra -Wshadow -Wno-unused-function                    \
		-march=native	                                                       \
		-fsanitize=address,undefined -fstrict-aliasing                         \
		main.c -o main
