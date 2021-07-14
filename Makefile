all:
	gcc -O0 -g3 -DDEBUG=1 -Wall -Wextra -Wshadow -Wno-unused-function          \
	    -march=native                                                          \
	    -fsanitize=address,undefined -fstrict-aliasing                         \
	    main.c -o main

	# gcc -O3 -DRELEASE=1 -Wall -Wextra -Wshadow -Wno-unused-function              \
	    # -march=native                                                            \
	    # -fstrict-aliasing                                                        \
	    # main.c -o main

clean:
	rm -rf main
