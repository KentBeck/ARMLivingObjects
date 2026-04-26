ARCH ?= arm64
CC ?= clang
AS ?= as
CFLAGS += -arch $(ARCH) -Itests -Isrc/c -Isrc/c_vm
ASFLAGS += -arch $(ARCH) -I src/arm

BIN_DIR = bin
TEST_BIN = $(BIN_DIR)/test
GC_STRESS_BIN = $(BIN_DIR)/gc_stress
C_INTERPRETER_SMOKE_BIN = $(BIN_DIR)/c_interpreter_smoke
SMALLTALK_EXPR_BIN = $(BIN_DIR)/smalltalk_expr
SMALLTALK_LSP_BIN = $(BIN_DIR)/smalltalk_lsp
INTERPRETER ?= c
ifeq ($(INTERPRETER),c)
CFLAGS += -DALO_INTERPRETER_C
else ifeq ($(INTERPRETER),asm)
CFLAGS += -DALO_INTERPRETER_ASM
endif

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
C_INTERPRETER_SMOKE_SRCS = tests/test_c_interpreter_smoke.c src/c/primitives.c
GC_STRESS_SRCS = tools/gc_stress.c
SMALLTALK_EXPR_SRCS = tools/smalltalk_expr.c tests/smalltalk_world.c src/c/bootstrap_compiler.c src/c/primitives.c src/c/smalltalk_tool_support.c
SMALLTALK_LSP_SRCS = tools/smalltalk_lsp.c tests/smalltalk_world.c src/c/bootstrap_compiler.c src/c/primitives.c src/c/smalltalk_tool_support.c

test: $(TEST_BIN)
	./$(TEST_BIN)

test-c:
	$(MAKE) clean
	$(MAKE) INTERPRETER=c test

test-asm:
	$(MAKE) clean
	$(MAKE) INTERPRETER=asm test

test-both-interpreters: test-c test-asm test-c-interpreter-smoke

test-c-interpreter-smoke:
	$(MAKE) INTERPRETER=c $(C_INTERPRETER_SMOKE_BIN)
	./$(C_INTERPRETER_SMOKE_BIN)

gc-stress: $(GC_STRESS_BIN)

smalltalk-expr: $(SMALLTALK_EXPR_BIN)

smalltalk-lsp: $(SMALLTALK_LSP_BIN)

test-smalltalk-expr: $(SMALLTALK_EXPR_BIN)
	@output="$$(./$(SMALLTALK_EXPR_BIN) "nil" "true" "false" "1" "#foo")"; \
	expected="$$(printf 'nil\ntrue\nfalse\n1\n#foo')"; \
	if [ "$$output" != "$$expected" ]; then \
		printf 'unexpected smalltalk_expr output:\n%s\n' "$$output"; \
		exit 1; \
	fi; \
	printf '%s\n' "$$output"

test-smalltalk-lsp: $(SMALLTALK_LSP_BIN)
	@init='{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"capabilities":{}}}'; \
	sym='{"jsonrpc":"2.0","id":2,"method":"workspace/symbol","params":{"query":"Object>>yourself"}}'; \
	hover='{"jsonrpc":"2.0","id":3,"method":"textDocument/hover","params":{"textDocument":{"uri":"file:///tmp/armlivingobjects-live-image/Object/yourself.st"},"position":{"line":0,"character":2}}}'; \
	defn='{"jsonrpc":"2.0","id":4,"method":"textDocument/definition","params":{"textDocument":{"uri":"file:///tmp/armlivingobjects-live-image/Object/yourself.st"},"position":{"line":0,"character":2}}}'; \
	shutdown='{"jsonrpc":"2.0","id":5,"method":"shutdown","params":null}'; \
	exitmsg='{"jsonrpc":"2.0","method":"exit","params":null}'; \
	output="$$( \
		{ printf 'Content-Length: %d\r\n\r\n%s' "$${#init}" "$$init"; \
		  printf 'Content-Length: %d\r\n\r\n%s' "$${#sym}" "$$sym"; \
		  printf 'Content-Length: %d\r\n\r\n%s' "$${#hover}" "$$hover"; \
		  printf 'Content-Length: %d\r\n\r\n%s' "$${#defn}" "$$defn"; \
		  printf 'Content-Length: %d\r\n\r\n%s' "$${#shutdown}" "$$shutdown"; \
		  printf 'Content-Length: %d\r\n\r\n%s' "$${#exitmsg}" "$$exitmsg"; } | ./$(SMALLTALK_LSP_BIN) \
	)"; \
	printf '%s\n' "$$output" | grep -q '"hoverProvider":true' || { printf 'initialize response missing hoverProvider\n'; exit 1; }; \
	printf '%s\n' "$$output" | grep -q '"Object>>yourself"' || { printf 'workspace/symbol missing Object>>yourself\n'; exit 1; }; \
	printf '%s\n' "$$output" | grep -q '"Object>>yourself\\n\\nyourself\\n    \^ self"' || { printf 'hover response missing live method source\n'; exit 1; }; \
	printf '%s\n' "$$output" | grep -q 'file:///tmp/armlivingobjects-live-image/Object/yourself.st' || { printf 'definition response missing live-image file uri\n'; exit 1; }

$(TEST_BIN): $(TEST_SRCS) tests/test_defs.h $(ASM_OBJS) $(C_VM_OBJS) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $(TEST_SRCS) $(ASM_OBJS) $(C_VM_OBJS)

$(GC_STRESS_BIN): $(GC_STRESS_SRCS) tests/test_defs.h $(GC_STRESS_OBJS) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $(GC_STRESS_SRCS) $(GC_STRESS_OBJS)

$(SMALLTALK_EXPR_BIN): $(SMALLTALK_EXPR_SRCS) tests/test_defs.h tests/smalltalk_world.h $(ASM_OBJS) $(C_VM_OBJS) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $(SMALLTALK_EXPR_SRCS) $(ASM_OBJS) $(C_VM_OBJS)

$(SMALLTALK_LSP_BIN): $(SMALLTALK_LSP_SRCS) tests/test_defs.h tests/smalltalk_world.h $(ASM_OBJS) $(C_VM_OBJS) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $(SMALLTALK_LSP_SRCS) $(ASM_OBJS) $(C_VM_OBJS)

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

.PHONY: clean gc-stress smalltalk-expr smalltalk-lsp test-c test-asm test-both-interpreters test-c-interpreter-smoke test-smalltalk-expr test-smalltalk-lsp
