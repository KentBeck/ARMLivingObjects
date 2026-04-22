# ARM Living Objects — Road to Self-Hosting Smalltalk

See completed.md for all finished work (594 tests).

## Goal

A self-hosting Smalltalk: the Smalltalk compiler is written in Smalltalk,
running on our VM, compiling itself. Then LSP server in Smalltalk.

## Design Philosophy

- Prefer the fewest possible VM primitives for now.
- Implement behavior in Smalltalk first; do not add new primitives unless required for correctness.
- Revisit primitive additions only after profiling post-JIT to target proven performance bottlenecks.
- Ignore chunk method categories for bootstrap correctness; later we will explore deriving categories automatically via an LLM pass.

## Dependency Chain

Characters → Strings/Symbols → Collections → Streams →
Scanner → Parser → Code Generator → Compiler (self-hosting) →
LSP Server

## Plan

### Next (Critical Path)

- [x] Add `PUSH_CLOSURE` codegen for block literals
- [x] Add stack bounds checking for direct stack push and method activation
- [x] Replace ARM interpreter with C incrementally, keeping assembly tests green
- [x] Add focused C interpreter smoke tests and run them alongside full-suite checks
- [x] Introduce real heap context objects using the existing frame context slot
- [x] Materialize contexts lazily for block homes, `thisContext`, and debugger/exception paths
- [x] Change block closures to reference a home context, not only copied values
- [x] Implement non-local block return using home contexts
- [x] Implement `cannotReturn:` when a non-local return targets a dead home
- [ ] Preserve arguments but not volatile non-argument temps for widowed contexts
- [ ] Implement class builder (`Class` + method install from parsed source)
  - [x] Define classes from a restricted Smalltalk class declaration
  - [x] Load one class declaration plus method chunks from one source
- [ ] Implement `.st` file loader (compile/install methods into classes)
  - [x] Load one class declaration plus method chunks from a `.st` file
  - [x] Expose class-file loading through the canonical Smalltalk test world
  - [x] Load `Token.st` as a real class-definition file
- [ ] Bootstrap compile/install core classes (String/Array/Dictionary/Streams)
- [ ] Start minimal Smalltalk compiler in Smalltalk and compile with C bootstrap
- [ ] Run first self-hosting check (Smalltalk compiler recompiles itself)
- [ ] Keep `OrderedCollection` deferred unless compiler ergonomics demand it

### Expression-Driven Development

- [x] Add executable expression specs (`expression -> expected result`) in C test harness
- [x] Cover currently-applicable C runtime tests with expression specs where representable
- [ ] Keep adding expression specs as capabilities land (booleans, collections, blocks, streams)
- [x] Add loader-backed spec files so expressions can live outside C source

### ARM-to-C Interpreter Migration

Goal: replace the ARM assembly interpreter with vanilla C while keeping both
the default C-backed VM and explicit `INTERPRETER=asm` comparison path green.

Rules:

- Default `make test` must pass after every change.
- `make test-both-interpreters` must pass before interpreter migration commits.
- Add focused C-interpreter smoke tests for only the bytecodes/features the C
  interpreter is expected to support so far; run those alongside `make test`.
- Keep the external VM ABI stable: `interpret(sp_ptr, fp_ptr, ip, class_table, om, txn_log)`.
- Do not change bytecode format, object layout, frame layout, or Smalltalk semantics during the migration.
- Leave excluded assembly files in the repo until the C interpreter is default and stable.

Completed:

- [x] Create shared C VM definitions in `src/c_vm/vm_defs.h`.
- [x] Replace leaf helper assembly with C:
  `tagged`, `object`, `lookup`, `stack_ops`, `frame`, and standalone `bytecode` helpers.
- [x] Add opt-in C interpreter skeleton behind `INTERPRETER=c`.
- [x] Keep assembly interpreter as the default build during the parity push.
- [x] Switch the default build to the C interpreter once the full suite passed with C.

Next small steps:

- [x] Add `make test-c-interpreter-smoke` target.
- [x] Add smoke tests for current C interpreter support:
  `PUSH_LITERAL`, `PUSH_SELF`, `PUSH_TEMP`, `PUSH_ARG`,
  `STORE_TEMP`, `POP`, `DUPLICATE`, `RETURN_STACK_TOP`,
  `HALT`, `JUMP`, `JUMP_IF_TRUE`, and `JUMP_IF_FALSE`.
- [x] Make the smoke target build with `INTERPRETER=c` and run only supported cases.
- [x] Port `SEND_MESSAGE` without primitives; verify simple unary/argument sends.
- [x] Port non-allocating primitive families:
  SmallInteger arithmetic/comparison, identity/class/hash, character, String equality/hash, and Symbol equality.
- [x] Port remaining standalone primitives:
  `printChar` and String `asSymbol`.
- [x] Port indexed access primitives and transaction-aware `at:` / `at:put:`.
- [x] Port instance-variable transaction reads/writes and write barrier behavior.
- [x] Port allocation primitives:
  - [x] Add non-GC `basicNew` and `basicNew:` success/fallback paths.
  - [x] Add GC retry/root preservation for allocation primitives.
- [x] Port block activation and copied values.
- [x] Port context and non-local return support:
  - [x] Add `thisContext` and closure home-context materialization.
  - [x] Add non-local block return and `cannotReturn:`.
- [x] Switch default `INTERPRETER` to `c` only after full `make test` passes with C.
- [ ] Remove obsolete assembly helper/interpreter files after the C default has stayed green.

### 17. Primitive Infrastructure

New primitives needed by the class library. Each is a VM primitive
dispatched by the interpreter's primitive handler.

Block closure scenarios from "Under Cover Contexts and the Big Frame-Up":

- [x] Copied outer temp is captured at closure creation time
- [x] Escaped block retains copied outer state after its home method returns
- [x] Copied outer argument is visible inside the block body
- [ ] Non-local return from block to home activation
- [ ] `cannotReturn:` when a non-local return escapes a dead home activation
- [ ] Married/widowed context semantics for long-lived block homes

- [x] `basicNew` — primitive on Class: allocate fixed-size instance (reads instSize from receiver)
- [x] `basicNew:` — primitive on Class: allocate indexable/byte instance (reads instFormat from receiver)
- [x] `new` / `new:` — Smalltalk methods on Class: `^ self basicNew` / `^ self basicNew: size`
- [x] `size` — return object size (inst var count or indexable size)
- [x] `==` — identity comparison (same pointer), returns tagged true/false
- [x] `basicClass` — primitive: return class of receiver (handles tagged values)
- [x] `class` — Smalltalk method: `^ self basicClass`
- [x] fix `at:` / `at:put:` — dispatch on format:
  - FORMAT_FIELDS: error (use inst var access instead)
  - FORMAT_INDEXABLE: 1-based word access, bounds-checked
  - FORMAT_BYTES: 1-based byte access, returns/stores SmallInt byte value, bounds-checked
- [x] `basicAt:` / `basicAt:put:` — same primitives as at:/at:put: (not overridden by convention)
- [x] `hash` — identity hash (address-based, or SmallInt value)
- [x] `printChar` — write a single character (SmallInt) to stdout (for debugging/bootstrap)
- [x] `value:` — Block>>value: (1-arg block evaluation)
- [x] `perform:` — send a message (symbol) to receiver dynamically
- [x] `halt` — crash the VM (brk #9)

### 18. Characters

Character is its own class, represented as an immediate with tag `1111` (low 4 bits = `0x0F`).
This shares the `11` low-2-bit tag with nil/true/false, distinguished by bits 3:2 = `11`.
The Unicode code point is stored in bits [31:4], giving 28-bit range (covers all of Unicode).

Encoding: `(codePoint << 4) | 0x0F`. E.g. `$A` = `(65 << 4) | 0x0F` = `0x41F`.

The class table entry for Character is at index 4 (after SmallInteger, BlockClosure, True, False).

- [x] `tag_character(code_point)` / `untag_character(tagged)` in tagged.s
- [x] `is_character(tagged)` — check low 4 bits = `0x0F`
- [x] Character class in class table (index 4), `basicClass` returns it
- [x] `value` — return the code point as SmallInteger (PRIM_CHAR_VALUE 19)
- [x] `asCharacter` on SmallInteger — convert to Character immediate (PRIM_AS_CHARACTER 20)
- [x] `isLetter`, `isDigit` as Character primitives (21, 22)
- [x] `asUppercase`, `asLowercase` as Character primitives (23, 24)
- [x] `printChar` on Character (not SmallInteger) — write byte to stdout
- [x] character literal `$A` in the bootstrap compiler
- [x] `=` on Character — identity comparison via `==` (same encoding → same bits)

### 19. String (ByteArray subclass)

String is a FORMAT_BYTES object. Characters accessed via `byteAt:`.
Selectors (Symbols) are interned Strings — identity comparison suffices.

- [x] String class with `size`, `at:`, `at:put:` (wired to primitives)
- [x] `=` — byte-by-byte comparison
- [x] `,` (comma) — concatenation (allocate new string, copy bytes) — in Smalltalk
- [x] `hash` — string hash (FNV-1a or similar)
- [x] `asSymbol` — intern a string (look up in symbol table, add if absent)
- [x] Symbol table — a global Array of interned strings
- [x] `printString` — for debugging (calls printChar per byte)

### 20. Array

Array is a FORMAT_INDEXABLE object. Already have `at:` and `at:put:` prims.

- [x] Array class with `size`, `at:`, `at:put:` (wired to existing prims)
- [ ] `copyFrom:to:` — sub-array (in Smalltalk)
- [ ] `do:` — iterate with a block (in Smalltalk)
- [ ] `collect:` — map (in Smalltalk)
- [ ] `with:collect:` — zip (in Smalltalk)
- [ ] `indexOf:` — linear search (in Smalltalk)

### 21. OrderedCollection

Variable-size collection backed by an Array with first/last indices.

Deferred for now: not on the shortest self-hosting path. Re-enable when
the compiler/class library needs dynamic growth convenience.

- [ ] `add:` — append, grow if needed
- [ ] `at:`, `size`
- [ ] `do:`, `collect:`, `select:`, `reject:`
- [ ] `removeLast`, `removeFirst`

### 22. Dictionary (Association-based)

Array of Associations (key→value). Linear scan for small dicts,
hash for larger. Start with linear.

- [x] Association class (key, value)
- [x] `at:put:`, `at:`, `at:ifAbsent:`
- [x] `includesKey:`
- [ ] `do:` (iterate values), `keysDo:`, `associationsDo:`
- [ ] Hash-based lookup (when performance matters)

### 23. Stream

ReadStream and WriteStream over collections.

- [x] ReadStream: `next`, `peek`, `atEnd`, `position`, `upToEnd`
- [x] WriteStream: `nextPut:`, `nextPutAll:`, `contents`
- [x] ReadStream on String (for the scanner)
- [x] WriteStream on String (for code generation output)

### 23b. Global Namespace

A system dictionary, conventionally named `Smalltalk`, for storing global variables,
primarily classes. This dictionary acts as the central namespace for the compiler.

- [x] Create a global `Dictionary` instance named `Smalltalk`.
- [ ] The bootstrap process will populate this dictionary with newly created classes.
- [ ] Keys are `Symbols` (e.g., `#Array`), values are the `Class` objects.
- [ ] The compiler looks up class names and globals in this dictionary.
- [x] Store the dictionary itself under the key `#Smalltalk` for reflective access.

### 24. Bootstrap Compiler (in C)

Minimal Smalltalk parser and code generator written in C.
Reads `.st` source files, emits bytecoded CompiledMethod objects
into the heap. Just enough to compile the class library and the
Smalltalk compiler itself.

- Completed items moved to `completed.md`.

- [ ] Code gen: emit JUMP, JUMP_IF_TRUE, JUMP_IF_FALSE (defer: keep conditionals/loops as message sends for now)
- [x] Code gen: emit PUSH_CLOSURE (block literal placeholder + balanced parsing)
- [x] Code gen: compile block bodies into nested codegen block tables
- [x] Loader: compile chunked `.st` source text into method definitions (`bc_compile_source_methods`)
- [ ] Code gen/class builder: materialize block CompiledMethod objects and replace `__blockN` placeholders
- [ ] Code gen: emit DUP
- [ ] Code gen: literal frame — collect literals, intern symbols (interning pending)
- [ ] Class builder: parse class definition, create Class object with methods
- [x] Class builder (partial): install compiled methods into bound classes/metaclasses
- [ ] File loader: read .st file, compile all methods, install in classes
- [ ] Bootstrap: compile String, Array, Dictionary, Stream (OrderedCollection later)

### 25. Class Library (in Smalltalk, compiled by bootstrap compiler)

Source files compiled by the C bootstrap compiler into heap objects.

- [ ] Object (base class: `=`, `~=`, `hash`, `printString`, `class`, `yourself`)
- [ ] UndefinedObject (nil: `isNil`, `ifNil:ifNotNil:`)
- [ ] Boolean, True, False (`ifTrue:ifFalse:`, `and:`, `or:`, `not`)
- [ ] SmallInteger (`+`, `-`, `*`, `/`, `<`, `>`, `=`, `to:do:`, `to:by:do:`, `printString`)
- [ ] Character (`isLetter`, `isDigit`, `value`, `asString`)
- [ ] String (`size`, `at:`, `,`, `=`, `hash`, `asSymbol`, `printString`)
- [ ] Symbol (interned String, `=` is `==`)
- [ ] Array (`size`, `at:`, `at:put:`, `do:`, `collect:`, `copyFrom:to:`)
- [ ] OrderedCollection (`add:`, `size`, `do:`, `collect:`, `select:`)
- [ ] Association (`key`, `value`, `key:value:`)
- [ ] Dictionary (`at:put:`, `at:`, `at:ifAbsent:`, `do:`, `keysDo:`)
- [ ] ReadStream (`on:`, `next`, `peek`, `atEnd`, `upToEnd`)
- [ ] WriteStream (`on:`, `nextPut:`, `nextPutAll:`, `contents`)
- [ ] Compiler-related: see section 27

### 25b. SUnit (minimal, no exceptions)

Minimal xUnit framework. Tests that pass print `.`, tests that
fail crash the VM (no exception handling yet). Good enough to
bootstrap — you see how far you get.

- [ ] TestCase class with `assert:` and `assert:equals:` (crash on failure via primitive)
- [ ] TestCase>>runTest: send the test selector to self
- [ ] TestRunner: iterate test selectors, send each, print `.` on success
- [ ] First Smalltalk test: SmallIntegerTest>>testAddition
- [ ] Test String, Array, OrderedCollection, Dictionary using SUnit
- [ ] Later: add `on:do:` exceptions, TestResult, proper failure reporting

### 26. Smalltalk Compiler (in Smalltalk)

The compiler itself, written in Smalltalk, compiled initially by the
C bootstrap compiler, then able to compile itself.

- [ ] Scanner (on ReadStream): tokens — identifiers, keywords, numbers, strings, symbols, special
- [ ] Parser: method node, send node, block node, literal node, variable node, return node, assignment node
- [ ] Parser: message precedence — unary > binary > keyword
- [ ] Parser: cascades, parentheses
- [ ] Code generator: visit AST, emit bytecodes to WriteStream
- [ ] Code generator: scope analysis — locals, inst vars, literals, block temps
- [ ] Code generator: jump patching for conditionals and blocks
- [ ] Compiler: `compile: source in: class` → CompiledMethod
- [ ] Self-hosting test: compiler compiles itself, produces identical bytecodes

### 27. Self-Hosting Milestone

- [ ] C bootstrap compiles Smalltalk compiler from .st source
- [ ] Smalltalk compiler compiles itself from the same .st source
- [ ] Compare output: both produce identical CompiledMethod objects
- [ ] Remove C bootstrap compiler dependency (optional — keep for bootstrapping new images)
- [ ] Save self-hosting image to disk

### 28. LSP Server (in Smalltalk)

- [ ] I/O primitives: read/write file descriptors (stdin/stdout)
- [ ] JSON parser (in Smalltalk, using Scanner/Stream)
- [ ] JSON emitter (in Smalltalk, using WriteStream)
- [ ] LSP message framing: Content-Length header parsing
- [ ] LSP initialize / initialized / shutdown handshake
- [ ] textDocument/didOpen, didChange — maintain source buffers
- [ ] textDocument/completion — method names from class hierarchy
- [ ] textDocument/hover — method source and class info
- [ ] textDocument/definition — find method definition

### 29. MCP Server (in Smalltalk)

- [ ] JSON-RPC over stdin/stdout (reuse LSP transport)
- [ ] tool: evaluate Smalltalk expression, return result
- [ ] tool: browse class hierarchy
- [ ] tool: inspect object fields
- [ ] tool: modify object fields (within a transaction)
- [ ] resource: expose live object graph as browsable context

---

## Deferred

### D1. JIT Compiler

### D2. Stack Pages & Overflow

### D3. Context Marriage/Widowhood/Divorce

### D4. Object Header Compaction

### D5. Non-local Return from Blocks

### D6. Nested Transactions
