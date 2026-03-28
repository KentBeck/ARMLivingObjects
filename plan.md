# ARM Living Objects — Road to Self-Hosting Smalltalk

See completed.md for all finished work (282 tests).

## Goal

A self-hosting Smalltalk: the Smalltalk compiler is written in Smalltalk,
running on our VM, compiling itself. Then LSP server in Smalltalk.

## Dependency Chain

Characters → Strings/Symbols → Collections → Streams →
Scanner → Parser → Code Generator → Compiler (self-hosting) →
LSP Server

## Plan

### 17. Primitive Infrastructure

New primitives needed by the class library. Each is a VM primitive
dispatched by the interpreter's primitive handler.

- [x] `basicNew` — primitive on Class: allocate fixed-size instance (reads instSize from receiver)
- [x] `basicNew:` — primitive on Class: allocate indexable/byte instance (reads instFormat from receiver)
- [ ] `new` / `new:` — Smalltalk methods on Class: `^ self basicNew initialize`
- [ ] `size` — return object size (inst var count or indexable size)
- [ ] `==` — identity comparison (same pointer), returns tagged true/false
- [ ] `class` — return the class of the receiver
- [ ] fix `at:` / `at:put:` — dispatch on format:
  - FORMAT_FIELDS: error (use inst var access instead)
  - FORMAT_INDEXABLE: 1-based word access, bounds-checked
  - FORMAT_BYTES: 1-based byte access, returns/stores SmallInt byte value, bounds-checked
- [ ] `basicAt:` / `basicAt:put:` — same as at:/at:put: (not overridden by convention)
- [ ] `hash` — identity hash (address-based, or SmallInt value)
- [ ] `printChar` — write a single character (SmallInt) to stdout (for debugging/bootstrap)
- [ ] `value:` — Block>>value: (1-arg block evaluation)
- [ ] `perform:` — send a message (symbol) to receiver dynamically
- [ ] `halt` — crash the VM (for assert: failure)

### 18. Characters

Characters are SmallIntegers — `$A` is just `65` tagged. No separate
Character class needed initially. String methods work with integer code points.

- [ ] character literal `$A` = tag_smallint(65) in the bootstrap compiler
- [ ] `isLetter`, `isDigit`, `isAlphanumeric` as SmallInteger methods
- [ ] `asUppercase`, `asLowercase` as SmallInteger methods

### 19. String (ByteArray subclass)

String is a FORMAT_BYTES object. Characters accessed via `byteAt:`.
Selectors (Symbols) are interned Strings — identity comparison suffices.

- [ ] String class with `size`, `at:`, `at:put:` (bytecodes, using byteAt: prim)
- [ ] `=` — byte-by-byte comparison
- [ ] `,` (comma) — concatenation (allocate new string, copy bytes) — in Smalltalk
- [ ] `hash` — string hash (FNV-1a or similar)
- [ ] `asSymbol` — intern a string (look up in symbol table, add if absent)
- [ ] Symbol table — a global Array of interned strings
- [ ] `printString` — for debugging (calls printChar per byte)

### 20. Array

Array is a FORMAT_INDEXABLE object. Already have `at:` and `at:put:` prims.

- [ ] Array class with `size`, `at:`, `at:put:` (wired to existing prims)
- [ ] `copyFrom:to:` — sub-array (in Smalltalk)
- [ ] `do:` — iterate with a block (in Smalltalk)
- [ ] `collect:` — map (in Smalltalk)
- [ ] `with:collect:` — zip (in Smalltalk)
- [ ] `indexOf:` — linear search (in Smalltalk)

### 21. OrderedCollection

Variable-size collection backed by an Array with first/last indices.

- [ ] `add:` — append, grow if needed
- [ ] `at:`, `size`
- [ ] `do:`, `collect:`, `select:`, `reject:`
- [ ] `removeLast`, `removeFirst`

### 22. Dictionary (Association-based)

Array of Associations (key→value). Linear scan for small dicts,
hash for larger. Start with linear.

- [ ] Association class (key, value)
- [ ] `at:put:`, `at:`, `at:ifAbsent:`
- [ ] `includesKey:`
- [ ] `do:` (iterate values), `keysDo:`, `associationsDo:`
- [ ] Hash-based lookup (when performance matters)

### 23. Stream

ReadStream and WriteStream over collections.

- [ ] ReadStream: `next`, `peek`, `atEnd`, `position`, `upToEnd`
- [ ] WriteStream: `nextPut:`, `nextPutAll:`, `contents`
- [ ] ReadStream on String (for the scanner)
- [ ] WriteStream on String (for code generation output)

### 24. Bootstrap Compiler (in C)

Minimal Smalltalk parser and code generator written in C.
Reads `.st` source files, emits bytecoded CompiledMethod objects
into the heap. Just enough to compile the class library and the
Smalltalk compiler itself.

- [ ] Tokenizer: identifiers, keywords, integers, strings, symbols, special chars
- [ ] Parser: method syntax — unary, binary, keyword messages
- [ ] Parser: temporaries `| x y |`, assignments `:=`
- [ ] Parser: blocks `[ :arg | body ]`
- [ ] Parser: cascades `;`, parentheses
- [ ] Parser: literals (integers, strings, symbols, arrays)
- [ ] Parser: return `^`
- [ ] Code gen: emit PUSH_LITERAL, PUSH_INST_VAR, PUSH_TEMP, PUSH_SELF
- [ ] Code gen: emit STORE_INST_VAR, STORE_TEMP
- [ ] Code gen: emit SEND_MESSAGE (unary, binary, keyword)
- [ ] Code gen: emit JUMP, JUMP_IF_TRUE, JUMP_IF_FALSE
- [ ] Code gen: emit PUSH_CLOSURE, RETURN, POP, DUP
- [ ] Code gen: literal frame — collect literals, intern symbols
- [ ] Class builder: parse class definition, create Class object with methods
- [ ] File loader: read .st file, compile all methods, install in classes
- [ ] Bootstrap: compile String, Array, OrderedCollection, Dictionary, Stream

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
