CC = gcc
CFLAGS = -O -Wall -Wextra -Werror

shuffle: shuffle.c
	$(CC) $(CFLAGS) -o $@ $@.c
