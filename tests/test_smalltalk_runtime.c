// Self-hosting progression: run real Smalltalk source files end-to-end.
//
// Step 1 (this file): verify Token.st runs — specifically that Token class>>eof
// returns a Token instance with type ivar bound to #eof. This proves class-side
// instance creation plus instance-side initialization work.
//
// Future steps: ReadStream, Tokenizer, Parser, CodeGenerator.

#include "test_defs.h"
#include "smalltalk_world.h"
#include "primitives.h"
#include "bootstrap_compiler.h"

static int byte_object_equals_cstring(uint64_t value, const char *text)
{
    if (!is_object_ptr(value))
    {
        return 0;
    }
    uint64_t *object = (uint64_t *)value;
    size_t len = strlen(text);
    return OBJ_FORMAT(object) == FORMAT_BYTES &&
           OBJ_SIZE(object) == (uint64_t)len &&
           memcmp(&OBJ_FIELD(object, 0), text, len) == 0;
}

#ifdef ALO_INTERPRETER_C
static uint64_t *materialize_codegen_method(SmalltalkWorld *world, uint64_t *generator)
{
    int64_t bytecode_count = untag_smallint(OBJ_FIELD(generator, 1));
    int64_t literal_count = untag_smallint(OBJ_FIELD(generator, 3));
    int64_t temp_count = untag_smallint(OBJ_FIELD(generator, 5));
    int64_t arg_count = untag_smallint(OBJ_FIELD(generator, 7));
    if (bytecode_count < 0 || literal_count < 0 || temp_count < 0 || arg_count < 0)
    {
        return NULL;
    }

    uint64_t *source_bytecodes = (uint64_t *)OBJ_FIELD(generator, 0);
    uint64_t *source_literals = (uint64_t *)OBJ_FIELD(generator, 2);
    uint64_t *bytecodes = om_alloc(world->om, (uint64_t)world->class_class,
                                   FORMAT_BYTES, (uint64_t)bytecode_count);
    if (bytecodes == NULL)
    {
        return NULL;
    }
    memcpy(&OBJ_FIELD(bytecodes, 0), &OBJ_FIELD(source_bytecodes, 0), (size_t)bytecode_count);

    uint64_t *literals = NULL;
    if (literal_count > 0)
    {
        literals = om_alloc(world->om, (uint64_t)world->class_class,
                            FORMAT_INDEXABLE, (uint64_t)literal_count);
        if (literals == NULL)
        {
            return NULL;
        }
        for (int64_t index = 0; index < literal_count; index++)
        {
            OBJ_FIELD(literals, index) = OBJ_FIELD(source_literals, index);
        }
    }

    uint64_t *method = om_alloc(world->om, (uint64_t)world->class_class, FORMAT_FIELDS, 5);
    if (method == NULL)
    {
        return NULL;
    }
    OBJ_FIELD(method, CM_PRIMITIVE) = tag_smallint(PRIM_NONE);
    OBJ_FIELD(method, CM_NUM_ARGS) = tag_smallint(arg_count);
    OBJ_FIELD(method, CM_NUM_TEMPS) = tag_smallint(temp_count);
    OBJ_FIELD(method, CM_LITERALS) = literals != NULL ? (uint64_t)literals : tagged_nil();
    OBJ_FIELD(method, CM_BYTECODES) = (uint64_t)bytecodes;
    return method;
}

static uint64_t run_materialized_method(SmalltalkWorld *world, TestContext *ctx,
                                        uint64_t *method, uint64_t receiver)
{
    uint64_t *bytecodes = (uint64_t *)OBJ_FIELD(method, CM_BYTECODES);
    uint64_t num_args = (uint64_t)untag_smallint(OBJ_FIELD(method, CM_NUM_ARGS));
    uint64_t num_temps = (uint64_t)untag_smallint(OBJ_FIELD(method, CM_NUM_TEMPS));
    uint64_t *sp = (uint64_t *)((uint8_t *)ctx->stack + STACK_WORDS * sizeof(uint64_t));
    uint64_t *fp = (uint64_t *)0xCAFE;
    stack_push(&sp, ctx->stack, receiver);
    activate_method(&sp, &fp, 0, (uint64_t)method, num_args, num_temps);
    return interpret(&sp, &fp, (uint8_t *)&OBJ_FIELD(bytecodes, 0), world->class_table, world->om, NULL);
}
#endif

void test_smalltalk_runtime(TestContext *ctx)
{
    static uint8_t world_buf[16 * 1024 * 1024] __attribute__((aligned(8)));
    SmalltalkWorld world;
    smalltalk_world_init(&world, world_buf, sizeof(world_buf));

    uint64_t *point_file_class =
        smalltalk_world_install_class_file(&world, "tests/fixtures/PointFile.st");
    ASSERT_EQ(ctx, point_file_class != NULL, 1,
              "runtime: class file loader creates PointFile");
    ASSERT_EQ(ctx, (uint64_t)smalltalk_world_lookup_class(&world, "PointFile"),
              (uint64_t)point_file_class,
              "runtime: class file loader registers PointFile");
    ASSERT_EQ(ctx, class_lookup(point_file_class, intern_cstring_symbol(world.om, "y")) != 0,
              1, "runtime: class file loader installs PointFile methods");

    ASSERT_EQ(ctx, smalltalk_world_install_class_file(&world, "src/smalltalk/Token.st") != NULL,
              1, "runtime: Token.st defines class and installs methods");

    // --- `Token eof` should return a Token instance with type = #eof. ---
    uint64_t *token_class = smalltalk_world_lookup_class(&world, "Token");
    ASSERT_EQ(ctx, token_class != NULL, 1, "runtime: Token in Smalltalk dict");

    uint64_t eof_tok = sw_send0(&world, ctx, (uint64_t)token_class, world.class_class, "eof");
    ASSERT_EQ(ctx, is_object_ptr(eof_tok), 1, "runtime: Token eof returns an object");
    uint64_t *eof_ptr = (uint64_t *)eof_tok;
    ASSERT_EQ(ctx, OBJ_SIZE(eof_ptr), 3, "runtime: Token eof has 3 ivar slots");

    // type ivar (slot 0) should be the interned Symbol #eof.
    uint64_t eof_type = OBJ_FIELD(eof_ptr, 0);
    uint64_t expected_eof_sym = intern_cstring_symbol(world.om, "eof");
    ASSERT_EQ(ctx, eof_type, expected_eof_sym, "runtime: Token eof type ivar is #eof");

    // --- Step 2: ReadStream.st. Build stream on a String, call next/peek/atEnd. ---
    // ReadStream uses ifTrue:ifFalse: which is defined on True/False in True.st/False.st.
    uint64_t *true_class = smalltalk_world_install_existing_class_file(&world, "src/smalltalk/True.st");
    ASSERT_EQ(ctx, true_class != NULL, 1,
              "runtime: True.st declaration matches existing class and installs methods");
    ASSERT_EQ(ctx, (uint64_t)true_class, (uint64_t)world.true_class,
              "runtime: True.st attaches to the existing True class");
    uint64_t *false_class = smalltalk_world_install_existing_class_file(&world, "src/smalltalk/False.st");
    ASSERT_EQ(ctx, false_class != NULL, 1,
              "runtime: False.st declaration matches existing class and installs methods");
    ASSERT_EQ(ctx, (uint64_t)false_class, (uint64_t)world.false_class,
              "runtime: False.st attaches to the existing False class");

    ASSERT_EQ(ctx, smalltalk_world_install_class_file(&world, "src/smalltalk/ReadStream.st") != NULL,
              1, "runtime: ReadStream.st defines class and installs methods");

    uint64_t *rs_class = smalltalk_world_lookup_class(&world, "ReadStream");
    ASSERT_EQ(ctx, rs_class != NULL, 1, "runtime: ReadStream in Smalltalk dict");

    // `ReadStream on: 'abc'` → a ReadStream whose collection is that string and position is 1.
    uint64_t abc = (uint64_t)sw_make_string(&world, "abc");
    uint64_t rs = sw_send1(&world, ctx, (uint64_t)rs_class, world.class_class, "on:", abc);
    ASSERT_EQ(ctx, is_object_ptr(rs), 1, "runtime: ReadStream on: returns an object");
    uint64_t *rs_ptr = (uint64_t *)rs;
    ASSERT_EQ(ctx, OBJ_SIZE(rs_ptr), 3, "runtime: ReadStream has 3 ivar slots");
    ASSERT_EQ(ctx, OBJ_FIELD(rs_ptr, 1), tag_smallint(1), "runtime: ReadStream position starts at 1");

    // Re-fetch the class (GC may have moved it during installs above).
    uint64_t *rs_class_live = smalltalk_world_lookup_class(&world, "ReadStream");
    // `stream next` should return the first byte ('a' = 97 as a SmallInteger,
    // because collection is a byte String and at: returns byte values).
    uint64_t first = sw_send0(&world, ctx, rs, rs_class_live, "next");
    ASSERT_EQ(ctx, first, tag_smallint('a'), "runtime: ReadStream next returns first byte");
    ASSERT_EQ(ctx, OBJ_FIELD(rs_ptr, 1), tag_smallint(2), "runtime: ReadStream position advances to 2");

    // `stream peek` returns next byte without advancing.
    uint64_t peeked = sw_send0(&world, ctx, rs, rs_class_live, "peek");
    ASSERT_EQ(ctx, peeked, tag_smallint('b'), "runtime: ReadStream peek returns second byte");
    ASSERT_EQ(ctx, OBJ_FIELD(rs_ptr, 1), tag_smallint(2), "runtime: ReadStream peek does not advance");

    // --- Step 3: Tokenizer.st end-to-end. Tokenize "1" and inspect the result. ---
    // The Tokenizer uses isNil on Character results (Object.st/UndefinedObject.st)
    // and `>`/`<=` on integers (SmallInteger.st adds these helpers).
    uint64_t *object_class = smalltalk_world_install_existing_class_file(&world, "src/smalltalk/Object.st");
    ASSERT_EQ(ctx, object_class != NULL, 1,
              "runtime: Object.st declaration matches existing class and installs methods");
    ASSERT_EQ(ctx, (uint64_t)object_class, (uint64_t)world.object_class,
              "runtime: Object.st attaches to the existing Object class");
    uint64_t *undefined_class = smalltalk_world_install_existing_class_file(&world, "src/smalltalk/UndefinedObject.st");
    ASSERT_EQ(ctx, undefined_class != NULL, 1,
              "runtime: UndefinedObject.st declaration matches existing class and installs methods");
    ASSERT_EQ(ctx, (uint64_t)undefined_class, (uint64_t)world.undefined_class,
              "runtime: UndefinedObject.st attaches to the existing UndefinedObject class");
    uint64_t *smallint_class = smalltalk_world_install_existing_class_file(&world, "src/smalltalk/SmallInteger.st");
    ASSERT_EQ(ctx, smallint_class != NULL, 1,
              "runtime: SmallInteger.st declaration matches existing class and installs methods");
    ASSERT_EQ(ctx, (uint64_t)smallint_class, (uint64_t)world.smallint_class,
              "runtime: SmallInteger.st attaches to the existing SmallInteger class");
    uint64_t *array_class = smalltalk_world_install_existing_class_file(&world, "src/smalltalk/Array.st");
    ASSERT_EQ(ctx, array_class != NULL, 1,
              "runtime: Array.st declaration matches existing class and installs methods");
    ASSERT_EQ(ctx, (uint64_t)array_class, (uint64_t)world.array_class,
              "runtime: Array.st attaches to the existing Array class");
    uint64_t *string_class = smalltalk_world_install_existing_class_file(&world, "src/smalltalk/String.st");
    ASSERT_EQ(ctx, string_class != NULL, 1,
              "runtime: String.st declaration matches existing class and installs methods");
    ASSERT_EQ(ctx, (uint64_t)string_class, (uint64_t)world.string_class,
              "runtime: String.st attaches to the existing String class");
    uint64_t hello = (uint64_t)sw_make_string(&world, "ab");
    uint64_t suffix = (uint64_t)sw_make_string(&world, "cd");
    uint64_t combined = sw_send1(&world, ctx, hello, NULL, ",", suffix);
    ASSERT_EQ(ctx, byte_object_equals_cstring(combined, "abcd"), 1,
              "runtime: String.st concatenation executes with whileTrue:");

    ASSERT_EQ(ctx, smalltalk_world_install_class_file(&world, "src/smalltalk/Tokenizer.st") != NULL,
              1, "runtime: Tokenizer.st defines class and installs methods");

    uint64_t *tokenizer_class = smalltalk_world_lookup_class(&world, "Tokenizer");
    ASSERT_EQ(ctx, tokenizer_class != NULL, 1, "runtime: Tokenizer in Smalltalk dict");

    // `Tokenizer on: '1'` → fresh tokenizer, source ivar is '1', stream is a
    // ReadStream on that source, buffered is nil.
    uint64_t src = (uint64_t)sw_make_string(&world, "1");
    uint64_t tokenizer = sw_send1(&world, ctx, (uint64_t)tokenizer_class, world.class_class, "on:", src);
    ASSERT_EQ(ctx, is_object_ptr(tokenizer), 1, "runtime: Tokenizer on: returns an object");
    // Ivars should be populated: source = input string, stream = ReadStream, buffered = nil.
    uint64_t *tok_ptr = (uint64_t *)tokenizer;
    ASSERT_EQ(ctx, OBJ_SIZE(tok_ptr), 3, "runtime: Tokenizer has 3 ivars");
    ASSERT_EQ(ctx, OBJ_FIELD(tok_ptr, 0), src, "runtime: Tokenizer source ivar is input string");
    ASSERT_EQ(ctx, is_object_ptr(OBJ_FIELD(tok_ptr, 1)), 1, "runtime: Tokenizer stream ivar is a ReadStream");
    ASSERT_EQ(ctx, OBJ_FIELD(tok_ptr, 2), tagged_nil(), "runtime: Tokenizer buffered ivar is nil");

    uint64_t first_tok_oop = sw_send0(&world, ctx, tokenizer, NULL, "next");
    ASSERT_EQ(ctx, is_object_ptr(first_tok_oop), 1, "runtime: Tokenizer next returns a Token");
    uint64_t *first_tok = (uint64_t *)first_tok_oop;
    ASSERT_EQ(ctx, OBJ_SIZE(first_tok), 3, "runtime: Token has 3 ivars");
    // type ivar (slot 0) should be #integer; value ivar (slot 2) should be 1.
    uint64_t integer_sym = intern_cstring_symbol(world.om, "integer");
    ASSERT_EQ(ctx, OBJ_FIELD(first_tok, 0), integer_sym,
              "runtime: Tokenizer produced a Token with type #integer");
    ASSERT_EQ(ctx, OBJ_FIELD(first_tok, 2), tag_smallint(1),
              "runtime: Tokenizer produced a Token with value 1");

#ifdef ALO_INTERPRETER_C
    OopRootSet compiler_roots = {0};
    uint64_t method_gen_root = 0;
    int have_method_gen_root = 0;

    ASSERT_EQ(ctx,
              bc_compile_and_install_classes_file(world.om, world.class_class,
                                                  world.string_class, world.array_class,
                                                  world.association_class,
                                                  NULL, 0, "src/smalltalk/ASTNodes.st"),
              1, "runtime: ASTNodes.st defines classes and installs methods");
    ASSERT_EQ(ctx, smalltalk_world_install_class_file(&world, "src/smalltalk/Parser.st") != NULL,
              1, "runtime: Parser.st defines class and installs methods");
    ASSERT_EQ(ctx, smalltalk_world_install_class_file(&world, "src/smalltalk/CodeGenerator.st") != NULL,
              1, "runtime: CodeGenerator.st defines class and installs methods");
    ASSERT_EQ(ctx, smalltalk_world_install_class_file(&world, "src/smalltalk/Compiler.st") != NULL,
              1, "runtime: Compiler.st defines class and installs methods");

    uint64_t *compiler_class = smalltalk_world_lookup_class(&world, "Compiler");
    ASSERT_EQ(ctx, compiler_class != NULL, 1, "runtime: Compiler in Smalltalk dict");
    uint64_t method_source = (uint64_t)sw_make_string(&world, "answer ^ 1");
    uint64_t method_gen = sw_send1(&world, ctx, (uint64_t)compiler_class, world.class_class,
                                   "compileMethod:", method_source);
    ASSERT_EQ(ctx, is_object_ptr(method_gen), 1,
              "runtime: Compiler compileMethod: returns a CodeGenerator object");
    method_gen_root = oop_roots_add(&compiler_roots, method_gen);
    have_method_gen_root = 1;
    uint64_t *method_gen_ptr = oop_roots_ptr(&compiler_roots, method_gen_root);
    ASSERT_EQ(ctx, OBJ_SIZE(method_gen_ptr), 11,
              "runtime: method CodeGenerator has expected ivar slots");
    ASSERT_EQ(ctx, OBJ_FIELD(method_gen_ptr, 1), tag_smallint(7),
              "runtime: Smalltalk compiler emits seven bytes for answer ^ 1");
    ASSERT_EQ(ctx, OBJ_FIELD(method_gen_ptr, 3), tag_smallint(1),
              "runtime: Smalltalk compiler records one literal for answer ^ 1");
    uint64_t *generated_bytecodes = (uint64_t *)OBJ_FIELD(method_gen_ptr, 0);
    ASSERT_EQ(ctx, is_object_ptr((uint64_t)generated_bytecodes), 1,
              "runtime: generated method bytecodes are stored in a byte object");
    uint8_t *generated_bytes = (uint8_t *)&OBJ_FIELD(generated_bytecodes, 0);
    ASSERT_EQ(ctx, generated_bytes[0], BC_PUSH_LITERAL,
              "runtime: Smalltalk compiler emits push literal first");
    ASSERT_EQ(ctx, generated_bytes[5], BC_RETURN,
              "runtime: Smalltalk compiler emits return after literal");
    ASSERT_EQ(ctx, generated_bytes[6], BC_RETURN,
              "runtime: Smalltalk compiler currently emits trailing implicit return");
    uint64_t *generated_literals = (uint64_t *)OBJ_FIELD(method_gen_ptr, 2);
    ASSERT_EQ(ctx, is_object_ptr((uint64_t)generated_literals), 1,
              "runtime: generated method literals are stored in an array");
    ASSERT_EQ(ctx, OBJ_FIELD(generated_literals, 0), tag_smallint(1),
              "runtime: Smalltalk compiler records literal 1");

    uint64_t *materialized_method = materialize_codegen_method(&world, method_gen_ptr);
    ASSERT_EQ(ctx, materialized_method != NULL, 1,
              "runtime: Smalltalk compiler output materializes as a CompiledMethod");
    uint64_t materialized_result =
        run_materialized_method(&world, ctx, materialized_method, tagged_nil());
    ASSERT_EQ(ctx, materialized_result, tag_smallint(1),
              "runtime: materialized Smalltalk-compiled method executes");
#endif

    ASSERT_EQ(ctx, smalltalk_world_install_class_file(&world, "src/smalltalk/TestCase.st") != NULL,
              1, "runtime: TestCase.st defines class and installs methods");
    uint64_t *test_case_class = smalltalk_world_lookup_class(&world, "TestCase");
    ASSERT_EQ(ctx, test_case_class != NULL, 1, "runtime: TestCase in Smalltalk dict");
    ASSERT_EQ(ctx, untag_smallint(OBJ_FIELD(test_case_class, CLASS_INST_SIZE)), 4,
              "runtime: TestCase.st class declaration has four instance variables");
    ASSERT_EQ(ctx, class_lookup(test_case_class, intern_cstring_symbol(world.om, "runOn:")) != 0,
              1, "runtime: TestCase.st installs runOn:");

    ASSERT_EQ(ctx, smalltalk_world_install_class_file(&world, "src/smalltalk/TestSuite.st") != NULL,
              1, "runtime: TestSuite.st defines class and installs methods");
    uint64_t *test_suite_class = smalltalk_world_lookup_class(&world, "TestSuite");
    ASSERT_EQ(ctx, test_suite_class != NULL, 1, "runtime: TestSuite in Smalltalk dict");
    ASSERT_EQ(ctx, untag_smallint(OBJ_FIELD(test_suite_class, CLASS_INST_SIZE)), 2,
              "runtime: TestSuite.st class declaration has two instance variables");
    ASSERT_EQ(ctx, class_lookup(test_suite_class, intern_cstring_symbol(world.om, "runOn:")) != 0,
              1, "runtime: TestSuite.st installs runOn:");

#ifdef ALO_INTERPRETER_C
    if (have_method_gen_root)
    {
        static uint8_t compiler_gc_buf[32 * 1024 * 1024] __attribute__((aligned(8)));
        uint64_t to_space[2];
        om_init(compiler_gc_buf, sizeof(compiler_gc_buf), to_space);

        gc_collect(compiler_roots.roots, compiler_roots.count, world.om, to_space,
                   (uint64_t)world_buf, (uint64_t)(world_buf + sizeof(world_buf)));

        uint64_t moved_method_gen = oop_roots_get(&compiler_roots, method_gen_root);
        uint64_t *moved_method_gen_ptr = (uint64_t *)moved_method_gen;
        ASSERT_EQ(ctx, moved_method_gen >= (uint64_t)compiler_gc_buf, 1,
                  "runtime: rooted compiler result moves to GC to-space lower bound");
        ASSERT_EQ(ctx, moved_method_gen < (uint64_t)(compiler_gc_buf + sizeof(compiler_gc_buf)), 1,
                  "runtime: rooted compiler result moves to GC to-space upper bound");
        ASSERT_EQ(ctx, OBJ_SIZE(moved_method_gen_ptr), 11,
                  "runtime: rooted compiler result remains a CodeGenerator after GC");
        ASSERT_EQ(ctx, OBJ_FIELD(moved_method_gen_ptr, 1), tag_smallint(7),
                  "runtime: rooted compiler result preserves bytecode count after GC");
        ASSERT_EQ(ctx, OBJ_FIELD(moved_method_gen_ptr, 3), tag_smallint(1),
                  "runtime: rooted compiler result preserves literal count after GC");
        uint64_t *moved_bytecodes = (uint64_t *)OBJ_FIELD(moved_method_gen_ptr, 0);
        ASSERT_EQ(ctx, is_object_ptr((uint64_t)moved_bytecodes), 1,
                  "runtime: rooted compiler result preserves bytecodes object after GC");
        uint8_t *moved_bytes = (uint8_t *)&OBJ_FIELD(moved_bytecodes, 0);
        ASSERT_EQ(ctx, moved_bytes[0], BC_PUSH_LITERAL,
                  "runtime: rooted compiler result preserves bytecodes after GC");
    }
#endif

    smalltalk_world_teardown(&world);
}
