## GC / OOP Safety

- Never keep a raw `ObjPtr`, `uint64_t *`, or field/literal/method/source buffer pointer across any operation that may allocate or trigger GC.
- Treat all sends, compiler/materialization helpers, object allocation, string/symbol creation, array growth, checkpoint/restart helpers, and anything marked `LO_MAY_GC` as GC boundaries.
- Carry rooted oops across those boundaries, not raw pointers.
- After a GC-capable boundary, reacquire raw pointers from the rooted oop.
- If a helper needs both a raw pointer and an oop, derive the raw pointer late and keep its live range short.

## Review Checklist

- Look for cached raw pointers that survive across `interpret`, `sw_send*`, `gc_*`, `om_*alloc*`, compiler helpers, or primitive calls.
- Look for class/method/literal/source pointers derived before a send and used after it.
- Prefer one-shot timed test runs with logs over long-lived debug processes.
