test:
	gcc -std=c99 -pedantic -Wall -Wextra -ggdb -o tests/test.out rebound.c $(wildcard tests/*.c) -I./ -lm
