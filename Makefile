ARCH ?= arm64
CC ?= clang
AS ?= as
CFLAGS += -arch $(ARCH) -Itests -Isrc/c
ASFLAGS += -arch $(ARCH) -I src/asm

BIN_DIR = bin
TEST_BIN = $(BIN_DIR)/test

ASM_SRCS = $(wildcard src/asm/*.s)
ASM_OBJS = $(patsubst src/asm/%.s,$(BIN_DIR)/%.o,$(ASM_SRCS))

TEST_SRCS = $(wildcard tests/*.c) src/c/bootstrap_compiler.c src/c/primitives.c

test: $(TEST_BIN)
	./$(TEST_BIN)

$(TEST_BIN): $(TEST_SRCS) tests/test_defs.h $(ASM_OBJS) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $(TEST_SRCS) $(ASM_OBJS)


$(BIN_DIR)/%.o: src/asm/%.s | $(BIN_DIR)
	$(AS) $(ASFLAGS) -o $@ $<

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

clean:
	rm -rf $(BIN_DIR)
	rm -f *.o test test_new

.PHONY: clean
