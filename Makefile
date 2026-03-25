CC = clang
AS = as

test: test.c stack.o
	$(CC) -o test test.c stack.o
	./test

stack.o: stack.s
	$(AS) -o stack.o stack.s

clean:
	rm -f test stack.o

.PHONY: clean

