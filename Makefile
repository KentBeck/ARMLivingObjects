ARCH ?= arm64
CC ?= clang
AS ?= as
CFLAGS += -arch $(ARCH) -Itests -Isrc/c -Isrc/c_vm
ASFLAGS += -arch $(ARCH) -I src/arm

BIN_DIR = bin
TEST_BIN = $(BIN_DIR)/test
GC_STRESS_BIN = $(BIN_DIR)/gc_stress
C_INTERPRETER_SMOKE_BIN = $(BIN_DIR)/c_interpreter_smoke
INTERPRETER ?= asm

ASM_SRCS = $(wildcard src/arm/*.s)
C_VM_SRCS = src/c_vm/tagged.c src/c_vm/object.c src/c_vm/lookup.c src/c_vm/stack_ops.c src/c_vm/frame.c src/c_vm/bytecode.c
ASM_FILTER_OUT = src/arm/tagged.s src/arm/object.s src/arm/lookup.s src/arm/stack_ops.s src/arm/frame.s src/arm/bytecode.s
ifeq ($(INTERPRETER),c)
ASM_FILTER_OUT += src/arm/interpret.s
C_VM_SRCS += src/c_vm/interpret.c
endif
ASM_SRCS := $(filter-out $(ASM_FILTER_OUT),$(ASM_SRCS))
ASM_OBJS = $(patsubst src/arm/%.s,$(BIN_DIR)/%.o,$(ASM_SRCS))
C_VM_OBJS = $(patsubst src/c_vm/%.c,$(BIN_DIR)/c_vm_%.o,$(C_VM_SRCS))
GC_STRESS_OBJS = $(BIN_DIR)/c_vm_object.o $(BIN_DIR)/gc.o $(BIN_DIR)/c_vm_tagged.o

TEST_SRCS = $(filter-out tests/test_c_interpreter_smoke.c,$(wildcard tests/*.c)) src/c/bootstrap_compiler.c src/c/primitives.c
C_INTERPRETER_SMOKE_SRCS = tests/test_c_interpreter_smoke.c
GC_STRESS_SRCS = tools/gc_stress.c

test: $(TEST_BIN)
	./$(TEST_BIN)

test-c-interpreter-smoke:
	$(MAKE) INTERPRETER=c $(C_INTERPRETER_SMOKE_BIN)
	./$(C_INTERPRETER_SMOKE_BIN)

gc-stress: $(GC_STRESS_BIN)

$(TEST_BIN): $(TEST_SRCS) tests/test_defs.h $(ASM_OBJS) $(C_VM_OBJS) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $(TEST_SRCS) $(ASM_OBJS) $(C_VM_OBJS)

$(GC_STRESS_BIN): $(GC_STRESS_SRCS) tests/test_defs.h $(GC_STRESS_OBJS) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $(GC_STRESS_SRCS) $(GC_STRESS_OBJS)

$(C_INTERPRETER_SMOKE_BIN): $(C_INTERPRETER_SMOKE_SRCS) tests/test_defs.h $(ASM_OBJS) $(C_VM_OBJS) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $(C_INTERPRETER_SMOKE_SRCS) $(ASM_OBJS) $(C_VM_OBJS)

$(BIN_DIR)/%.o: src/arm/%.s | $(BIN_DIR)
	$(AS) $(ASFLAGS) -o $@ $<

$(BIN_DIR)/c_vm_%.o: src/c_vm/%.c src/c_vm/vm_defs.h | $(BIN_DIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

clean:
	rm -rf $(BIN_DIR)
	rm -f *.o test test_new

.PHONY: clean gc-stress test-c-interpreter-smoke
