.PHONY: all validate snapshot clear

all:
	# DEBUG
	# gcc -O0 -g3 -Wall -Wextra -Wshadow -Wno-unused-function                      \
        # -DDEBUG=1 -DPRINT_LINES=1 -DPRINT_TOKENS=1                               \
	    # -march=native                                                            \
	    # -fsanitize=address,undefined -fstrict-aliasing                           \
	    # main.c -o main

	# COVERAGE
	gcc --std=c99 -pedantic -g3 -O0 -Wall -Wextra -Wshadow -Wno-unused-function  \
		-DRELEASE=1 -DPRINT_TOKENS=1                                             \
	    -march=native                                                            \
	    -fno-omit-frame-pointer -fstrict-aliasing                                \
	    main.c -o main

	# PROFILE
	# gcc -O3 -Wall -Wextra -Wshadow -Wno-unused-function                          \
        # -DRELEASE=1                                                              \
	    # -march=native                                                            \
	    # -fstrict-aliasing                                                        \
	    # main.c -o main

	# RELEASE
	# gcc -O3 -Wall -Wextra -Wshadow -Wno-unused-function                          \
        # -DRELEASE=1                                                              \
	    # -march=native                                                            \
	    # -fstrict-aliasing                                                        \
	    # main.c -o main

validate:
	@./validate.sh

snapshot:
	@./validate.sh --snapshot

clean:
	rm -rf main
