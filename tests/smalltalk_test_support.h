#ifndef SMALLTALK_TEST_SUPPORT_H
#define SMALLTALK_TEST_SUPPORT_H

#include "test_defs.h"
#include "primitives.h"

Oop stt_selector_oop(Om om, const char *selector);
void stt_md_append(Om om, ObjPtr class_class, ObjPtr klass, const char *selector, Oop method);
ObjPtr stt_make_primitive_cm(Om om, ObjPtr class_class, int prim, int num_args);
ObjPtr stt_make_byte_string(Om om, ObjPtr string_class, const char *text);
ObjPtr stt_make_class_with_ivars(Om om, ObjPtr class_class, ObjPtr string_class,
                                 ObjPtr superclass, const char **ivars, uint64_t ivar_count);
void stt_smalltalk_at_put(Om om, ObjPtr array_class, ObjPtr association_class,
                          const char *name, Oop value);

#endif
