# Expression Test Coverage Matrix

This tracks which existing C test suites are currently covered by
`tests/ExpressionSpecs.txt`-style expression tests.

Legend:
- **Covered**: already represented by expression specs.
- **Partial**: some behaviors represented; some still C-only.
- **Blocked**: not realistically expressible yet with current expression pipeline.

## Current Matrix

| C Test Suite | Status | Notes |
|---|---|---|
| `test_stack.c` | Blocked | Low-level stack/frame helpers (`stack_push`, `frame_*`) are VM internals, not Smalltalk expressions. |
| `test_tagged.c` | Partial | Expression specs cover user-visible numeric/character behavior; raw tag-bit layout checks remain C-only. |
| `test_object.c` | Blocked | Object header layout, raw field/object memory invariants are implementation-level. |
| `test_dispatch.c` | Partial | Expression specs cover normal send/eval behavior; many dispatch edge cases and synthetic bytecode scenarios remain C-only. |
| `test_blocks.c` | Blocked | Expression installer path does not yet materialize block literals into runtime block objects for spec execution. |
| `test_factorial.c` | Partial | Recursive arithmetic style behavior is representable; current expression spec file has not yet mirrored full factorial scenarios. |
| `test_transaction.c` | Blocked | Transaction machinery is VM-level and not yet exposed as Smalltalk expression protocol. |
| `test_gc.c` | Blocked | GC forwarding/copying/root scanning checks are memory-management internals. |
| `test_persist.c` | Blocked | Image pointer-offset conversion and relocation checks are persistence internals. |
| `test_primitives.c` | Partial | Arithmetic/comparison/character primitive behavior is expression-covered; many primitive-specific edge/error paths remain C-only. |
| `test_smalltalk_sources.c` | Partial | Source presence/invariants are file-level checks; expression specs cover runtime evaluation only. |
| `test_string_dispatch.c` | Blocked | Expression installer currently lacks string-literal object materialization for general expression execution. |
| `test_array_dispatch.c` | Blocked | No array-literal/bootstrap install path in expression runner yet for broad array behavior specs. |
| `test_symbol_dispatch.c` | Blocked | Runtime symbol object scenarios are not fully wired through current expression literal materialization path. |
| `test_bootstrap_compiler.c` | Partial | Compiler parse/codegen/installer behavior partly reflected by expression execution; compiler-internal assertions remain C-only. |
| `test_smalltalk_expressions.c` | Covered | This is the expression-driven suite itself. |

## Why Some Suites Are Blocked

The current expression runner compiles methods from expression specs and installs them, but has important limits:

1. Literal materialization is intentionally minimal (safe bootstrap subset).
2. Block literals are compiled but not fully materialized for expression-suite runtime assertions.
3. VM-internal invariants (stack frames, GC, persistence, transaction internals) are intentionally tested at C level.

## Next Unlocks (to convert more C tests)

1. Add string-literal runtime materialization in the installer path.
2. Add block-literal materialization for expression-runner installed methods.
3. Add lightweight expression-fixture prelude support (define helper classes/methods once per file).
4. Add optional expected forms beyond scalar (`smallint/true/false`) as needed (`nil`, string, symbol identity).
