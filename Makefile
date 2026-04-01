ARCH ?= arm64
CC ?= clang
AS ?= as
CFLAGS += -arch $(ARCH)
ASFLAGS += -arch $(ARCH)

ASM_SRCS = stack_ops.s frame.s bytecode.s tagged.s object.s lookup.s interpret.s txn.s gc.s persist.s
ASM_OBJS = $(ASM_SRCS:.s=.o)

TEST_SRCS = test_main.c test_stack.c test_tagged.c test_object.c test_dispatch.c test_blocks.c test_factorial.c test_transaction.c test_gc.c test_persist.c test_primitives.c primitives.c

test: $(TEST_SRCS) test_defs.h $(ASM_OBJS)
	$(CC) $(CFLAGS) -o test $(TEST_SRCS) $(ASM_OBJS)
	./test

%.o: %.s
	$(AS) $(ASFLAGS) -o $@ $<

clean:
	rm -f test $(ASM_OBJS)

.PHONY: clean
