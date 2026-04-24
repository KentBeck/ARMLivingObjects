# Debugging Handoff: `Compiler compileExpression: '1 + 2'`

Current state:

- The repo is green after removing the noisy failing runtime probes.
- `make test` passed at `1305 passed, 0 failed`.
- `make INTERPRETER=c test-smalltalk-expr` still only smokes:
  `nil`, `true`, `false`, `1`, `#foo`.
- `1 + 2` is still failing in the real Smalltalk compiler path.

Key conclusion:

The bug is **not** a general C interpreter bytecode execution bug.

What was proven:

1. Manual C bytecode repros for the `visitMessage:`-shaped send sequence pass.
2. Manual block activation / copied-temp / return / send variants pass.
3. A bootstrap-compiled outer probe method
   `probeIfFalseFullVisitMessageThenSelf:`
   also passes in a minimal fake world when the fake dispatch surface uses
   real interned symbol selectors.

So the remaining failure depends on the **real `CodeGenerator` helper methods**
and/or the real compiler object graph, not on the raw bytecode mechanics alone.

Files touched for this narrowing:

- `tests/test_blocks.c`
  - contains the useful binary-search repros
  - keep these; they rule out the low-level interpreter path
- `tests/test_smalltalk_runtime.c`
  - the broad failing probe block was removed to keep the tree green
- `plan.md`
  - contains a summary under `Current Debugging Boundary`

Important pitfalls already hit:

- The fake exact-source repro initially trapped for the wrong reason because
  the fake method dictionaries used tagged smallints as selectors.
  The bootstrap compiler emits real interned symbol selectors.
- The fake minimal world also needed a real `False` class entry in
  `CLASS_TABLE_FALSE` for the bootstrap-compiled `ifTrue:ifFalse:` probe.

Best next step:

- Add a focused test around a real `CodeGenerator` instance in the real
  Smalltalk world.
- After each helper send in the failing path, inspect:
  - receiver identity
  - `bytecodeCount`
  - `literalCount`
  - key literal contents
  - temp/literal arrays if they may have moved
- Assume helper sends like `addSelectorLiteral:` may allocate and invalidate
  stale raw object pointers.
- Start from this sequence:
  1. `visitNode: aNode receiver`
  2. `visitMessageArgs: aNode arguments from: 1`
  3. `selectorIndex := addSelectorLiteral: aNode selector`
  4. `emitSendMessage: selectorIndex argc: aNode arguments size`

Do not restart from "maybe bytecodes are wrong". That path has already been
narrowed out enough to stop spending time there.
