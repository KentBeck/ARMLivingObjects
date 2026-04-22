ARCH ?= arm64
CC ?= clang
AS ?= as
CFLAGS += -arch $(ARCH) -Itests -Isrc/c -Isrc/c_vm
ASFLAGS += -arch $(ARCH) -I src/arm

BIN_DIR = bin
TEST_BIN = $(BIN_DIR)/test
GC_STRESS_BIN = $(BIN_DIR)/gc_stress

ASM_SRCS = $(wildcard src/arm/*.s)
ASM_OBJS = $(patsubst src/arm/%.s,$(BIN_DIR)/%.o,$(ASM_SRCS))
GC_STRESS_OBJS = $(BIN_DIR)/object.o $(BIN_DIR)/gc.o $(BIN_DIR)/tagged.o

TEST_SRCS = $(wildcard tests/*.c) src/c/bootstrap_compiler.c src/c/primitives.c
GC_STRESS_SRCS = tools/gc_stress.c

test: $(TEST_BIN)
	./$(TEST_BIN)

gc-stress: $(GC_STRESS_BIN)

$(TEST_BIN): $(TEST_SRCS) tests/test_defs.h $(ASM_OBJS) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $(TEST_SRCS) $(ASM_OBJS)

$(GC_STRESS_BIN): $(GC_STRESS_SRCS) tests/test_defs.h $(GC_STRESS_OBJS) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $(GC_STRESS_SRCS) $(GC_STRESS_OBJS)

$(BIN_DIR)/%.o: src/arm/%.s | $(BIN_DIR)
	$(AS) $(ASFLAGS) -o $@ $<

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

clean:
	rm -rf $(BIN_DIR)
	rm -f *.o test test_new

.PHONY: clean gc-stress
