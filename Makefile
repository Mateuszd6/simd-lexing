.PHONY: all validate snapshot clear

all:
	# DEBUG
	# gcc -O0 -g3 -DDEBUG=1 -Wall -Wextra -Wshadow -Wno-unused-function            \
	    # -march=native                                                            \
	    # -fsanitize=address,undefined -fstrict-aliasing                           \
	    # main.c -o main

	# COVERAGE
	gcc --std=c99 -pedantic -g3 -O0 -DRELEASE=1 -Wall -Wextra -Wshadow -Wno-unused-function          \
	    -march=native                                                            \
	    -fno-omit-frame-pointer -fstrict-aliasing                                \
	    main.c -o main

	# PROFILE
	# gcc -O3 -DRELEASE=1 -Wall -Wextra -Wshadow -Wno-unused-function               \
	    # -g3 -fno-omit-frame-pointer -march=native                                 \
	    # -fstrict-aliasing                                                         \
	    # main.c -o main

	# RELEASE
	# gcc -O3 -DRELEASE=1 -Wall -Wextra -Wshadow -Wno-unused-function              \
	    # -march=native                                                            \
	    # -fstrict-aliasing                                                        \
	    # main.c -o main

validate:
	@./validate.sh

snapshot:
	@./validate.sh --snapshot

clean:
	rm -rf main
