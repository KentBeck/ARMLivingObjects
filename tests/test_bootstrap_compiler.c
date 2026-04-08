#include "test_defs.h"
#include "bootstrap_compiler.h"

static void assert_tok(TestContext *ctx, BTokenizer *tokenizer, BTokenType type, const char *text)
{
    BToken token = bt_next(tokenizer);
    ASSERT_EQ(ctx, token.type, type, "token type matches");
    ASSERT_EQ(ctx, strcmp(token.text, text), 0, "token text matches");
}

static uint32_t read_u32(const uint8_t *bytes, int index)
{
    return ((uint32_t)bytes[index]) |
           (((uint32_t)bytes[index + 1]) << 8) |
           (((uint32_t)bytes[index + 2]) << 16) |
           (((uint32_t)bytes[index + 3]) << 24);
}

void test_bootstrap_compiler(TestContext *ctx)
{
    {
        BTokenizer tokenizer;
        bt_init(&tokenizer, "at: index put: value ^ index + 1");

        assert_tok(ctx, &tokenizer, BTOK_KEYWORD, "at:");
        assert_tok(ctx, &tokenizer, BTOK_IDENTIFIER, "index");
        assert_tok(ctx, &tokenizer, BTOK_KEYWORD, "put:");
        assert_tok(ctx, &tokenizer, BTOK_IDENTIFIER, "value");
        assert_tok(ctx, &tokenizer, BTOK_SPECIAL, "^");
        assert_tok(ctx, &tokenizer, BTOK_IDENTIFIER, "index");
        assert_tok(ctx, &tokenizer, BTOK_SPECIAL, "+");

        BToken int_token = bt_next(&tokenizer);
        ASSERT_EQ(ctx, int_token.type, BTOK_INTEGER, "integer token type");
        ASSERT_EQ(ctx, int_token.int_value, 1, "integer token value");

        BToken eof = bt_next(&tokenizer);
        ASSERT_EQ(ctx, eof.type, BTOK_EOF, "tokenizer reaches EOF");
    }

    {
        BTokenizer tokenizer;
        bt_init(&tokenizer, "#Array 'hello''s' ()[]");

        assert_tok(ctx, &tokenizer, BTOK_SYMBOL, "Array");
        assert_tok(ctx, &tokenizer, BTOK_STRING, "hello's");
        assert_tok(ctx, &tokenizer, BTOK_SPECIAL, "(");
        assert_tok(ctx, &tokenizer, BTOK_SPECIAL, ")");
        assert_tok(ctx, &tokenizer, BTOK_SPECIAL, "[");
        assert_tok(ctx, &tokenizer, BTOK_SPECIAL, "]");

        BToken eof = bt_next(&tokenizer);
        ASSERT_EQ(ctx, eof.type, BTOK_EOF, "tokenizer EOF after specials");
    }

    {
        BTokenizer tokenizer;
        bt_init(&tokenizer, "$A");

        BToken token = bt_next(&tokenizer);
        ASSERT_EQ(ctx, token.type, BTOK_CHARACTER, "character token type");
        ASSERT_EQ(ctx, token.int_value, 65, "character token value");
        ASSERT_EQ(ctx, strcmp(token.text, "A"), 0, "character token text");

        BToken eof = bt_next(&tokenizer);
        ASSERT_EQ(ctx, eof.type, BTOK_EOF, "tokenizer EOF after character");
    }

    {
        BTokenizer tokenizer;
        bt_init(&tokenizer, "#(1 #foo)");

        assert_tok(ctx, &tokenizer, BTOK_SPECIAL, "#(");
        assert_tok(ctx, &tokenizer, BTOK_INTEGER, "1");
        assert_tok(ctx, &tokenizer, BTOK_SYMBOL, "foo");
        assert_tok(ctx, &tokenizer, BTOK_SPECIAL, ")");

        BToken eof = bt_next(&tokenizer);
        ASSERT_EQ(ctx, eof.type, BTOK_EOF, "tokenizer EOF after literal array");
    }

    {
        BTokenizer tokenizer;
        bt_init(&tokenizer, "x := 1");

        assert_tok(ctx, &tokenizer, BTOK_IDENTIFIER, "x");
        assert_tok(ctx, &tokenizer, BTOK_SPECIAL, ":=");
        assert_tok(ctx, &tokenizer, BTOK_INTEGER, "1");

        BToken eof = bt_next(&tokenizer);
        ASSERT_EQ(ctx, eof.type, BTOK_EOF, "tokenizer EOF after assignment");
    }

    {
        BMethodHeader header;
        ASSERT_EQ(ctx, bc_parse_method_header("size", &header), 1,
                  "parse unary method header");
        ASSERT_EQ(ctx, header.kind, BMETHOD_UNARY,
                  "unary header kind");
        ASSERT_EQ(ctx, strcmp(header.selector, "size"), 0,
                  "unary selector");
        ASSERT_EQ(ctx, header.arg_count, 0,
                  "unary arg count");
    }

    {
        BMethodHeader header;
        ASSERT_EQ(ctx, bc_parse_method_header("+ other", &header), 1,
                  "parse binary method header");
        ASSERT_EQ(ctx, header.kind, BMETHOD_BINARY,
                  "binary header kind");
        ASSERT_EQ(ctx, strcmp(header.selector, "+"), 0,
                  "binary selector");
        ASSERT_EQ(ctx, header.arg_count, 1,
                  "binary arg count");
        ASSERT_EQ(ctx, strcmp(header.arg_names[0], "other"), 0,
                  "binary arg name");
    }

    {
        BMethodHeader header;
        ASSERT_EQ(ctx, bc_parse_method_header("at: index put: value", &header), 1,
                  "parse keyword method header");
        ASSERT_EQ(ctx, header.kind, BMETHOD_KEYWORD,
                  "keyword header kind");
        ASSERT_EQ(ctx, strcmp(header.selector, "at:put:"), 0,
                  "keyword selector");
        ASSERT_EQ(ctx, header.arg_count, 2,
                  "keyword arg count");
        ASSERT_EQ(ctx, strcmp(header.arg_names[0], "index"), 0,
                  "keyword first arg");
        ASSERT_EQ(ctx, strcmp(header.arg_names[1], "value"), 0,
                  "keyword second arg");
    }

    {
        BMethodHeader header;
        ASSERT_EQ(ctx, bc_parse_method_header("at: put:", &header), 0,
                  "reject invalid keyword method header");
    }

    {
        BMethodBody body;
        ASSERT_EQ(ctx, bc_parse_method_body("| x y | x := 1. y := x + 2. ^ y", &body), 1,
                  "parse method body with temps and assignments");
        ASSERT_EQ(ctx, body.temp_count, 2, "temp count");
        ASSERT_EQ(ctx, strcmp(body.temp_names[0], "x"), 0, "first temp");
        ASSERT_EQ(ctx, strcmp(body.temp_names[1], "y"), 0, "second temp");
        ASSERT_EQ(ctx, body.assignment_count, 2, "assignment count");
        ASSERT_EQ(ctx, body.return_count, 1, "return count");
        ASSERT_EQ(ctx, body.literal_integer_count, 2, "integer literal count");
        ASSERT_EQ(ctx, body.literal_character_count, 0, "character literal count");
        ASSERT_EQ(ctx, body.literal_string_count, 0, "string literal count");
        ASSERT_EQ(ctx, body.literal_symbol_count, 0, "symbol literal count");
        ASSERT_EQ(ctx, body.message_send_count, 1, "message send count");
    }

    {
        BMethodBody body;
        ASSERT_EQ(ctx, bc_parse_method_body("^ self at: 1 put: 'a'", &body), 1,
                  "parse method body with keyword message");
        ASSERT_EQ(ctx, body.temp_count, 0, "no temps");
        ASSERT_EQ(ctx, body.assignment_count, 0, "no assignments");
        ASSERT_EQ(ctx, body.return_count, 1, "single return");
        ASSERT_EQ(ctx, body.literal_integer_count, 1, "integer literal in keyword arg");
        ASSERT_EQ(ctx, body.literal_character_count, 0, "no character literals");
        ASSERT_EQ(ctx, body.literal_string_count, 1, "string literal in keyword arg");
        ASSERT_EQ(ctx, body.literal_symbol_count, 0, "no symbol literals");
        ASSERT_EQ(ctx, body.message_send_count, 1, "single keyword send");
    }

    {
        BMethodBody body;
        ASSERT_EQ(ctx, bc_parse_method_body("^ #foo", &body), 1,
                  "parse method body with symbol return");
        ASSERT_EQ(ctx, body.return_count, 1, "single return for symbol");
        ASSERT_EQ(ctx, body.literal_character_count, 0, "no character literals");
        ASSERT_EQ(ctx, body.literal_symbol_count, 1, "symbol literal count");
        ASSERT_EQ(ctx, body.message_send_count, 0, "no message sends");
    }

    {
        BMethodBody body;
        ASSERT_EQ(ctx, bc_parse_method_body("^ $A", &body), 1,
                  "parse method body with character return");
        ASSERT_EQ(ctx, body.return_count, 1, "single return for character");
        ASSERT_EQ(ctx, body.literal_character_count, 1, "character literal count");
    }

    {
        BMethodBody body;
        ASSERT_EQ(ctx, bc_parse_method_body("^ [ :x | x + 1 ] value: 2", &body), 1,
                  "parse method body with block and keyword send");
        ASSERT_EQ(ctx, body.return_count, 1, "block body return count");
        ASSERT_EQ(ctx, body.assignment_count, 0, "block body assignment count");
        ASSERT_EQ(ctx, body.literal_integer_count, 2, "block literal integers");
        ASSERT_EQ(ctx, body.message_send_count, 2, "block and outer message sends");
    }

    {
        BMethodBody body;
        ASSERT_EQ(ctx, bc_parse_method_body("^ [ :x | x + 1", &body), 0,
                  "reject unterminated block body");
    }

    {
        BMethodBody body;
        ASSERT_EQ(ctx, bc_parse_method_body("^ (1 + 2) = 3", &body), 1,
                  "parse parenthesized expression");
        ASSERT_EQ(ctx, body.return_count, 1, "parenthesized return count");
        ASSERT_EQ(ctx, body.literal_integer_count, 3, "parenthesized integer literals");
        ASSERT_EQ(ctx, body.message_send_count, 2, "parenthesized message sends");
    }

    {
        BMethodBody body;
        ASSERT_EQ(ctx, bc_parse_method_body("^ stream nextPut: $A; nextPut: $B; contents", &body), 1,
                  "parse cascade expression");
        ASSERT_EQ(ctx, body.return_count, 1, "cascade return count");
        ASSERT_EQ(ctx, body.literal_character_count, 2, "cascade character literals");
        ASSERT_EQ(ctx, body.message_send_count, 3, "cascade message sends");
    }

    {
        BMethodBody body;
        ASSERT_EQ(ctx, bc_parse_method_body("^ #(1 'a' #foo $B)", &body), 1,
                  "parse literal array expression");
        ASSERT_EQ(ctx, body.return_count, 1, "literal array return count");
        ASSERT_EQ(ctx, body.literal_integer_count, 1, "literal array integer count");
        ASSERT_EQ(ctx, body.literal_string_count, 1, "literal array string count");
        ASSERT_EQ(ctx, body.literal_symbol_count, 1, "literal array symbol count");
        ASSERT_EQ(ctx, body.literal_character_count, 1, "literal array character count");
    }

    {
        BMethodBody body;
        ASSERT_EQ(ctx, bc_parse_method_body("^ #(1 #foo", &body), 0,
                  "reject unterminated literal array");
    }

    {
        BCompiledBody compiled;
        ASSERT_EQ(ctx, bc_codegen_method_body("^ self", &compiled), 1,
                  "codegen return self");
        ASSERT_EQ(ctx, compiled.literal_count, 0, "return self has no literals");
        ASSERT_EQ(ctx, compiled.bytecode_count, 2, "return self bytecode count");
        ASSERT_EQ(ctx, compiled.bytecodes[0], 3, "return self push opcode");
        ASSERT_EQ(ctx, compiled.bytecodes[1], 7, "return self return opcode");
    }

    {
        BCompiledBody compiled;
        ASSERT_EQ(ctx, bc_codegen_method_body("^ #foo", &compiled), 1,
                  "codegen return literal");
        ASSERT_EQ(ctx, compiled.literal_count, 1, "return literal count");
        ASSERT_EQ(ctx, compiled.literals[0].type, BTOK_SYMBOL, "return literal token type");
        ASSERT_EQ(ctx, strcmp(compiled.literals[0].text, "foo"), 0, "return literal token value");
        ASSERT_EQ(ctx, compiled.bytecode_count, 6, "return literal bytecode count");
        ASSERT_EQ(ctx, compiled.bytecodes[0], 0, "return literal push opcode");
        ASSERT_EQ(ctx, read_u32(compiled.bytecodes, 1), 0, "return literal index");
        ASSERT_EQ(ctx, compiled.bytecodes[5], 7, "return literal return opcode");
    }

    {
        BCompiledBody compiled;
        ASSERT_EQ(ctx, bc_codegen_method_body("| x | x := 1. ^ x", &compiled), 1,
                  "codegen temp assign and return");
        ASSERT_EQ(ctx, compiled.literal_count, 1, "assignment literal count");
        ASSERT_EQ(ctx, compiled.literals[0].type, BTOK_INTEGER, "assignment literal type");
        ASSERT_EQ(ctx, compiled.literals[0].int_value, 1, "assignment literal value");
        ASSERT_EQ(ctx, compiled.bytecode_count, 16, "assignment bytecode count");
        ASSERT_EQ(ctx, compiled.bytecodes[0], 0, "assignment push literal opcode");
        ASSERT_EQ(ctx, read_u32(compiled.bytecodes, 1), 0, "assignment push literal index");
        ASSERT_EQ(ctx, compiled.bytecodes[5], 5, "assignment store temp opcode");
        ASSERT_EQ(ctx, read_u32(compiled.bytecodes, 6), 0, "assignment store temp index");
        ASSERT_EQ(ctx, compiled.bytecodes[10], 2, "assignment push temp opcode");
        ASSERT_EQ(ctx, read_u32(compiled.bytecodes, 11), 0, "assignment push temp index");
        ASSERT_EQ(ctx, compiled.bytecodes[15], 7, "assignment return opcode");
    }

    {
        BCompiledBody compiled;
        ASSERT_EQ(ctx, bc_codegen_method_body("^ 1 + 2", &compiled), 1,
                  "codegen binary send return");
        ASSERT_EQ(ctx, compiled.literal_count, 3, "binary send literal count");
        ASSERT_EQ(ctx, compiled.literals[2].type, BTOK_SYMBOL, "binary selector literal type");
        ASSERT_EQ(ctx, strcmp(compiled.literals[2].text, "+"), 0, "binary selector literal value");
        ASSERT_EQ(ctx, compiled.bytecodes[0], 0, "binary push receiver literal");
        ASSERT_EQ(ctx, compiled.bytecodes[5], 0, "binary push arg literal");
        ASSERT_EQ(ctx, compiled.bytecodes[10], 6, "binary send opcode");
        ASSERT_EQ(ctx, read_u32(compiled.bytecodes, 11), 2, "binary selector literal index");
        ASSERT_EQ(ctx, read_u32(compiled.bytecodes, 15), 1, "binary arg count");
        ASSERT_EQ(ctx, compiled.bytecodes[19], 7, "binary return opcode");
    }

    {
        BCompiledBody compiled;
        ASSERT_EQ(ctx, bc_codegen_method_body("^ self at: 1 put: 2", &compiled), 1,
                  "codegen keyword send return");
        ASSERT_EQ(ctx, compiled.literal_count, 3, "keyword send literal count");
        ASSERT_EQ(ctx, compiled.literals[2].type, BTOK_SYMBOL, "keyword selector literal type");
        ASSERT_EQ(ctx, strcmp(compiled.literals[2].text, "at:put:"), 0, "keyword selector literal value");
        ASSERT_EQ(ctx, compiled.bytecodes[0], 3, "keyword push self opcode");
        ASSERT_EQ(ctx, compiled.bytecodes[1], 0, "keyword push first arg literal opcode");
        ASSERT_EQ(ctx, compiled.bytecodes[6], 0, "keyword push second arg literal opcode");
        ASSERT_EQ(ctx, compiled.bytecodes[11], 6, "keyword send opcode");
        ASSERT_EQ(ctx, read_u32(compiled.bytecodes, 12), 2, "keyword selector index");
        ASSERT_EQ(ctx, read_u32(compiled.bytecodes, 16), 2, "keyword arg count");
        ASSERT_EQ(ctx, compiled.bytecodes[20], 7, "keyword return opcode");
    }

    {
        BCompiledBody compiled;
        ASSERT_EQ(ctx, bc_codegen_method_body("^ foo", &compiled), 1,
                  "codegen ivar read return");
        ASSERT_EQ(ctx, compiled.inst_var_count, 1, "ivar count after read");
        ASSERT_EQ(ctx, strcmp(compiled.inst_var_names[0], "foo"), 0, "ivar name after read");
        ASSERT_EQ(ctx, compiled.bytecodes[0], 1, "ivar read opcode");
        ASSERT_EQ(ctx, read_u32(compiled.bytecodes, 1), 0, "ivar read index");
        ASSERT_EQ(ctx, compiled.bytecodes[5], 7, "ivar read return opcode");
    }

    {
        BCompiledBody compiled;
        ASSERT_EQ(ctx, bc_codegen_method_body("foo := 1. ^ foo", &compiled), 1,
                  "codegen ivar write then read");
        ASSERT_EQ(ctx, compiled.inst_var_count, 1, "ivar count after write/read");
        ASSERT_EQ(ctx, strcmp(compiled.inst_var_names[0], "foo"), 0, "ivar name after write/read");
        ASSERT_EQ(ctx, compiled.bytecodes[0], 0, "ivar write push literal opcode");
        ASSERT_EQ(ctx, compiled.bytecodes[5], 4, "ivar write store opcode");
        ASSERT_EQ(ctx, read_u32(compiled.bytecodes, 6), 0, "ivar write index");
        ASSERT_EQ(ctx, compiled.bytecodes[10], 1, "ivar read opcode");
        ASSERT_EQ(ctx, read_u32(compiled.bytecodes, 11), 0, "ivar read index reused");
        ASSERT_EQ(ctx, compiled.bytecodes[15], 7, "ivar write/read return opcode");
    }

    {
        BCompiledBody compiled;
        ASSERT_EQ(ctx, bc_codegen_method_body("foo := 1. bar := 2. ^ bar", &compiled), 1,
                  "codegen multiple ivars deterministic slots");
        ASSERT_EQ(ctx, compiled.inst_var_count, 2, "multiple ivar count");
        ASSERT_EQ(ctx, strcmp(compiled.inst_var_names[0], "foo"), 0, "first ivar name");
        ASSERT_EQ(ctx, strcmp(compiled.inst_var_names[1], "bar"), 0, "second ivar name");
    }

    {
        BCompiledBody compiled;
        ASSERT_EQ(ctx, bc_codegen_method_body("^ [ 1 ] value", &compiled), 1,
                  "codegen block literal with unary send");
        ASSERT_EQ(ctx, compiled.block_count, 1, "block literal compiles one block body");
        ASSERT_EQ(ctx, compiled.bytecodes[0], BC_PUSH_CLOSURE, "block push closure opcode");
        ASSERT_EQ(ctx, read_u32(compiled.bytecodes, 1), 0, "block literal index");
        ASSERT_EQ(ctx, compiled.bytecodes[5], BC_SEND_MESSAGE, "block send value opcode");
        ASSERT_EQ(ctx, read_u32(compiled.bytecodes, 6), 1, "block selector index");
        ASSERT_EQ(ctx, read_u32(compiled.bytecodes, 10), 0, "block value arg count");
        ASSERT_EQ(ctx, compiled.bytecodes[14], BC_RETURN, "block expression return opcode");
        ASSERT_EQ(ctx, compiled.literal_count, 2, "block expression literal count");
        ASSERT_EQ(ctx, compiled.literals[0].type, BTOK_BLOCK_LITERAL, "block literal token type");
        ASSERT_EQ(ctx, compiled.literals[0].int_value, 0, "block literal block index");
        ASSERT_EQ(ctx, strcmp(compiled.literals[1].text, "value"), 0, "block value selector literal");
        ASSERT_EQ(ctx, compiled.blocks[0].bytecodes[0], BC_PUSH_LITERAL, "compiled block pushes literal");
        ASSERT_EQ(ctx, read_u32(compiled.blocks[0].bytecodes, 1), 0, "compiled block literal index");
        ASSERT_EQ(ctx, compiled.blocks[0].bytecodes[5], BC_RETURN, "compiled block returns top");
        ASSERT_EQ(ctx, compiled.blocks[0].literal_count, 1, "compiled block literal count");
        ASSERT_EQ(ctx, compiled.blocks[0].literals[0].type, BTOK_INTEGER, "compiled block literal type");
        ASSERT_EQ(ctx, compiled.blocks[0].literals[0].int_value, 1, "compiled block literal value");
    }

    {
        BCompiledBody compiled;
        ASSERT_EQ(ctx, bc_codegen_method_body("^ [ [ 1 ] value ] value", &compiled), 1,
                  "codegen nested block literal balancing");
        ASSERT_EQ(ctx, compiled.block_count >= 1, 1, "nested block creates at least outer block body");
    }

    {
        BCompiledBody compiled;
        ASSERT_EQ(ctx, bc_codegen_method_body("^ [ ^ 1 ] value", &compiled), 1,
                  "codegen explicit return inside block");
        ASSERT_EQ(ctx, compiled.block_count, 1, "non-local-return source compiles one block");
        ASSERT_EQ(ctx, compiled.blocks[0].bytecodes[5], BC_RETURN_NON_LOCAL,
                  "block ^ emits non-local return opcode");
    }

    {
        BCompiledBody compiled;
        ASSERT_EQ(ctx, bc_codegen_method_body("self", &compiled), 1,
                  "implicit return for final expression");
        ASSERT_EQ(ctx, compiled.bytecode_count, 2, "implicit return bytecode count");
        ASSERT_EQ(ctx, compiled.bytecodes[0], BC_PUSH_SELF, "implicit return push self");
        ASSERT_EQ(ctx, compiled.bytecodes[1], BC_RETURN, "implicit return return opcode");
    }

    {
        const char *source =
            "!Object methodsFor: 'comparing'!\n"
            "== anObject\n"
            "    <primitive: 12>\n"
            "!\n"
            "\n"
            "!ReadStream class methodsFor: 'instance creation'!\n"
            "on: aCollection\n"
            "    ^ self new initializeOn: aCollection\n"
            "!\n";
        BMethodChunk chunks[8];
        int count = 0;
        ASSERT_EQ(ctx, bc_parse_method_chunks(source, chunks, 8, &count), 1,
                  "parse chunk methods source");
        ASSERT_EQ(ctx, count, 2, "chunk method count");
        ASSERT_EQ(ctx, strcmp(chunks[0].class_name, "Object"), 0, "chunk class name");
        ASSERT_EQ(ctx, chunks[0].class_side, 0, "chunk class-side false");
        ASSERT_EQ(ctx, strcmp(chunks[0].category, "comparing"), 0, "chunk category");
        ASSERT_EQ(ctx, strstr(chunks[0].method_source, "== anObject") != NULL, 1,
                  "chunk method source content");
        ASSERT_EQ(ctx, strcmp(chunks[1].class_name, "ReadStream"), 0, "chunk class-side class name");
        ASSERT_EQ(ctx, chunks[1].class_side, 1, "chunk class-side true");
        ASSERT_EQ(ctx, strcmp(chunks[1].category, "instance creation"), 0,
                  "chunk class-side category");
        ASSERT_EQ(ctx, strstr(chunks[1].method_source, "on: aCollection") != NULL, 1,
                  "chunk class-side method content");
    }

    {
        const char *source =
            "!Object methodsFor: 'comparing'!\n"
            "== anObject\n"
            "    <primitive: 12>\n"
            "!\n"
            "\n"
            "!Class methodsFor: 'instance creation'!\n"
            "new\n"
            "    ^ self basicNew\n"
            "!\n";
        BMethodChunk chunks[8];
        int chunk_count = 0;
        BCompiledMethodDef methods[8];
        int method_count = 0;

        ASSERT_EQ(ctx, bc_parse_method_chunks(source, chunks, 8, &chunk_count), 1,
                  "parse chunks before compile");
        ASSERT_EQ(ctx, bc_compile_method_chunks(chunks, chunk_count, methods, 8, &method_count), 1,
                  "compile chunk methods");
        ASSERT_EQ(ctx, method_count, 2, "compiled method count");

        ASSERT_EQ(ctx, strcmp(methods[0].class_name, "Object"), 0, "compiled method class name");
        ASSERT_EQ(ctx, methods[0].class_side, 0, "compiled method class_side");
        ASSERT_EQ(ctx, strcmp(methods[0].header.selector, "=="), 0, "compiled binary selector");
        ASSERT_EQ(ctx, methods[0].header.arg_count, 1, "compiled binary arg count");
        ASSERT_EQ(ctx, methods[0].primitive_index, 12, "compiled primitive index");

        ASSERT_EQ(ctx, strcmp(methods[1].class_name, "Class"), 0, "compiled unary class name");
        ASSERT_EQ(ctx, strcmp(methods[1].header.selector, "new"), 0, "compiled unary selector");
        ASSERT_EQ(ctx, methods[1].primitive_index, -1, "compiled unary non-primitive");
        ASSERT_EQ(ctx, methods[1].body.bytecode_count, 11, "compiled unary body bytecode count");
        ASSERT_EQ(ctx, methods[1].body.bytecodes[0], BC_PUSH_SELF, "compiled unary body push self");
        ASSERT_EQ(ctx, methods[1].body.bytecodes[1], BC_SEND_MESSAGE, "compiled unary body send");
        ASSERT_EQ(ctx, methods[1].body.bytecodes[10], BC_RETURN, "compiled unary body return");
    }

    {
        const char *source =
            "!Character methodsFor: 'accessing'!\n"
            "value\n"
            "    <primitive: 19>\n"
            "    ^ 0\n"
            "!\n";
        BMethodChunk chunks[4];
        int chunk_count = 0;
        BCompiledMethodDef methods[4];
        int method_count = 0;

        ASSERT_EQ(ctx, bc_parse_method_chunks(source, chunks, 4, &chunk_count), 1,
                  "parse primitive+body chunks");
        ASSERT_EQ(ctx, bc_compile_method_chunks(chunks, chunk_count, methods, 4, &method_count), 1,
                  "compile primitive+body chunks");
        ASSERT_EQ(ctx, method_count, 1, "primitive+body method count");
        ASSERT_EQ(ctx, methods[0].primitive_index, 19, "primitive+body primitive index");
        ASSERT_EQ(ctx, methods[0].body.bytecode_count, 6, "primitive+body bytecode count");
        ASSERT_EQ(ctx, methods[0].body.bytecodes[0], BC_PUSH_LITERAL, "primitive+body push literal");
        ASSERT_EQ(ctx, read_u32(methods[0].body.bytecodes, 1), 0, "primitive+body literal index");
        ASSERT_EQ(ctx, methods[0].body.bytecodes[5], BC_RETURN, "primitive+body return");
        ASSERT_EQ(ctx, methods[0].body.literal_count, 1, "primitive+body literal count");
        ASSERT_EQ(ctx, methods[0].body.literals[0].type, BTOK_INTEGER, "primitive+body literal type");
        ASSERT_EQ(ctx, methods[0].body.literals[0].int_value, 0, "primitive+body literal value");
    }

    {
        uint64_t *sample_meta = om_alloc(ctx->om, (uint64_t)ctx->class_class, FORMAT_FIELDS, 4);
        OBJ_FIELD(sample_meta, CLASS_SUPERCLASS) = tagged_nil();
        OBJ_FIELD(sample_meta, CLASS_METHOD_DICT) = tagged_nil();
        OBJ_FIELD(sample_meta, CLASS_INST_SIZE) = tag_smallint(0);
        OBJ_FIELD(sample_meta, CLASS_INST_FORMAT) = tag_smallint(FORMAT_FIELDS);

        uint64_t *sample_class = om_alloc(ctx->om, (uint64_t)sample_meta, FORMAT_FIELDS, 4);
        OBJ_FIELD(sample_class, CLASS_SUPERCLASS) = tagged_nil();
        OBJ_FIELD(sample_class, CLASS_METHOD_DICT) = tagged_nil();
        OBJ_FIELD(sample_class, CLASS_INST_SIZE) = tag_smallint(0);
        OBJ_FIELD(sample_class, CLASS_INST_FORMAT) = tag_smallint(FORMAT_FIELDS);

        BClassBinding bindings[1] = {
            {"Sample", sample_class},
        };

        const char *source =
            "!Sample methodsFor: 'testing'!\n"
            "foo\n"
            "    ^ 7\n"
            "!\n"
            "bar\n"
            "    ^ self foo\n"
            "!\n"
            "!Sample class methodsFor: 'instance creation'!\n"
            "answer\n"
            "    ^ 42\n"
            "!\n";

        ASSERT_EQ(ctx,
                  bc_compile_and_install_source_methods(ctx->om, ctx->class_class, bindings, 1, source),
                  1,
                  "compile and install source methods");

        uint64_t *instance_md = (uint64_t *)OBJ_FIELD(sample_class, CLASS_METHOD_DICT);
        uint64_t *class_md = (uint64_t *)OBJ_FIELD(sample_meta, CLASS_METHOD_DICT);
        ASSERT_EQ(ctx, instance_md != NULL, 1, "instance-side method dictionary installed");
        ASSERT_EQ(ctx, class_md != NULL, 1, "class-side method dictionary installed");
        ASSERT_EQ(ctx, OBJ_SIZE(instance_md), 4, "instance-side has two methods");
        ASSERT_EQ(ctx, OBJ_SIZE(class_md), 2, "class-side has one method");

        uint64_t *bar_cm = NULL;
        for (uint64_t index = 1; index < OBJ_SIZE(instance_md); index += 2)
        {
            uint64_t *cm = (uint64_t *)OBJ_FIELD(instance_md, index);
            uint64_t *bc = (uint64_t *)OBJ_FIELD(cm, CM_BYTECODES);
            uint8_t *bytes = (uint8_t *)&OBJ_FIELD(bc, 0);
            if (bytes[0] == BC_PUSH_SELF && bytes[1] == BC_SEND_MESSAGE)
            {
                bar_cm = cm;
                break;
            }
        }
        ASSERT_EQ(ctx, bar_cm != NULL, 1, "bar compiled method found");

        uint64_t *sample_instance = om_alloc(ctx->om, (uint64_t)sample_class, FORMAT_FIELDS, 0);
        uint64_t *bar_bc = (uint64_t *)OBJ_FIELD(bar_cm, CM_BYTECODES);

        uint64_t *sp = (uint64_t *)((uint8_t *)ctx->stack + STACK_WORDS * sizeof(uint64_t));
        uint64_t *fp = (uint64_t *)0xCAFE;
        stack_push(&sp, ctx->stack, (uint64_t)sample_instance);
        activate_method(&sp, &fp, 0, (uint64_t)bar_cm, 0, 0);

        uint64_t result = interpret(&sp, &fp, (uint8_t *)&OBJ_FIELD(bar_bc, 0), ctx->class_table, ctx->om, NULL);
        ASSERT_EQ(ctx, result, tag_smallint(7), "installed methods execute via send lookup");
    }
}
