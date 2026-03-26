CC = clang
AS = as

ASM_SRCS = stack_ops.s frame.s bytecode.s tagged.s object.s lookup.s
ASM_OBJS = $(ASM_SRCS:.s=.o)

test: test.c $(ASM_OBJS)
	$(CC) -o test test.c $(ASM_OBJS)
	./test

%.o: %.s
	$(AS) -o $@ $<

clean:
	rm -f test $(ASM_OBJS)

.PHONY: clean

