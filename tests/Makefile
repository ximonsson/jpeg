
CC = gcc
NASM = nasm -f elf64

all: threads

threads: threads.o threads.c
	$(CC) -o threads threads.c threads.o -lpthread

%.o: %.asm
	$(NASM) $^

%.o: %.c
	$(CC) -o $@ $^

clean:
	rm *.o threads
