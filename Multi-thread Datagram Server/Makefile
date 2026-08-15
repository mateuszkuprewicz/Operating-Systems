all: server
server: server.c 
	gcc -fsanitize=address,undefined -static-libasan -Wall -Wextra -Wshadow -Werror -o server server.c
.PHONY: clean all
clean:
	rm server 