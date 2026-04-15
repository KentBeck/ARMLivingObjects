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

void test_smalltalk_runtime(TestContext *ctx)
{
    static uint8_t world_buf[16 * 1024 * 1024] __attribute__((aligned(8)));
    SmalltalkWorld world;
    smalltalk_world_init(&world, world_buf, sizeof(world_buf));

    // Define Token class (ivars: type, text, value) and install Token.st methods.
    const char *token_ivars[] = {"type", "text", "value"};
    smalltalk_world_define_class(&world, "Token", NULL, token_ivars, 3, FORMAT_FIELDS);

    ASSERT_EQ(ctx, smalltalk_world_install_st_file(&world, "src/smalltalk/Token.st"), 1,
              "runtime: Token.st installs");

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

    smalltalk_world_teardown(&world);
}
