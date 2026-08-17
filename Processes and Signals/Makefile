all: program
program: main.c	
	gcc -fsanitize=address,undefined -static-libasan -Wall -Wextra -Wshadow -Werror -o program main.c
.PHONY: clean all
clean:
	rm program
	rm [0-9]* -f