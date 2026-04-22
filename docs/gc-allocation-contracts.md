# GC and Allocation Contracts

The C VM distinguishes object-memory allocation from moving garbage collection.

- `LO_NO_ALLOC`: does not allocate in Living Objects object memory.
- `LO_ALLOCATES`: may call `om_alloc` directly or transitively, but does not
  trigger moving GC under the current contract.
- `LO_MAY_GC`: may trigger moving GC. Do not keep raw `uint64_t *` object
  pointers across these calls unless the referenced object is rooted and the
  pointer is reloaded afterward.

`om_alloc` only bumps object memory and returns `NULL` on exhaustion. It does
not collect. The current moving-GC boundary is the C interpreter retry path
used after allocation failure.

Treat these public calls as GC-capable boundaries:

- `interpret`
- `sw_send0`
- `sw_send1`
- `sw_send2`
- `gc_collect`
- `gc_update_stack`
- `gc_copy_object`

Treat these public calls as allocation-capable but not GC-capable:

- `om_alloc`
- `intern_cstring_symbol`
- `ensure_frame_context`
- `ensure_frame_context_global`
- `bc_install_compiled_methods`
- `bc_compile_and_install_source_methods`
- `bc_define_class`
- `bc_define_class_from_source`
- `bc_compile_and_install_class_source`
- `bc_compile_and_install_class_file`
- `bc_compile_and_install_existing_class_source`
- `bc_compile_and_install_existing_class_file`
- `bc_compile_and_install_classes_source`
- `bc_compile_and_install_classes_file`
- `smalltalk_world_init`
- `smalltalk_world_define_class`
- `smalltalk_world_install_st_file`
- `smalltalk_world_install_class_file`
- `smalltalk_world_install_existing_class_file`
- `sw_make_string`

The practical rule is simple: across `LO_MAY_GC`, store object references as
oops in VM roots, stack slots, object fields, or explicit root arrays. Reload
raw object pointers only after the call returns.
