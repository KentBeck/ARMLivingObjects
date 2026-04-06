#include "test_defs.h"
#include "bootstrap_compiler.h"

static int read_file(const char *path, char *buf, size_t cap)
{
    FILE *f = fopen(path, "rb");
    if (!f)
    {
        return 0;
    }
    size_t n = fread(buf, 1, cap - 1, f);
    fclose(f);
    buf[n] = '\0';
    return 1;
}

void test_smalltalk_sources(TestContext *ctx)
{
    char class_src[4096];
    char string_src[8192];
    char symbol_src[2048];
    char array_src[4096];
    char association_src[4096];
    char dictionary_src[12288];
    char system_dictionary_src[4096];
    char read_stream_src[8192];
    char write_stream_src[8192];
    char expr_specs_src[4096];
    BCompiledMethodDef methods[64];
    int method_count = 0;

    ASSERT_EQ(ctx, read_file("src/smalltalk/Class.st", class_src, sizeof(class_src)), 1,
              "src/smalltalk/Class.st exists");
    ASSERT_EQ(ctx, strstr(class_src, "new\n    ^ self basicNew") != NULL, 1,
              "Class>>new delegates to basicNew");
    ASSERT_EQ(ctx, strstr(class_src, "new: size\n    ^ self basicNew: size") != NULL, 1,
              "Class>>new: delegates to basicNew:");
    ASSERT_EQ(ctx, bc_compile_source_methods(class_src, methods, 64, &method_count), 1,
              "Class.st compiles through chunk pipeline");
    ASSERT_EQ(ctx, method_count, 4, "Class.st method count");
    ASSERT_EQ(ctx, strcmp(methods[0].class_name, "Class"), 0, "Class.st compiled class name");

    ASSERT_EQ(ctx, read_file("src/smalltalk/String.st", string_src, sizeof(string_src)), 1,
              "src/smalltalk/String.st exists");
    ASSERT_EQ(ctx, strstr(string_src, ", aString") != NULL, 1,
              "String>>, method exists");
    ASSERT_EQ(ctx, strstr(string_src, "result := String new: self size + aString size.") != NULL, 1,
              "String>>, allocates result with String new:");
    ASSERT_EQ(ctx, strstr(string_src, "result at: i put: (self at: i).") != NULL, 1,
              "String>>, copies receiver bytes");
    ASSERT_EQ(ctx, strstr(string_src, "result at: (offset + i) put: (aString at: i).") != NULL, 1,
              "String>>, copies argument bytes");
    ASSERT_EQ(ctx, strstr(string_src, "printString") != NULL, 1,
              "String>>printString exists");
    ASSERT_EQ(ctx, strstr(string_src, "(self at: i) asCharacter printChar.") != NULL, 1,
              "String>>printString prints chars via asCharacter printChar");

    ASSERT_EQ(ctx, read_file("src/smalltalk/Symbol.st", symbol_src, sizeof(symbol_src)), 1,
              "src/smalltalk/Symbol.st exists");
    ASSERT_EQ(ctx, strstr(symbol_src, "= aSymbol") != NULL, 1,
              "Symbol>>= method exists");
    ASSERT_EQ(ctx, strstr(symbol_src, "^ self == aSymbol") != NULL, 1,
              "Symbol>>= uses identity equality");
    ASSERT_EQ(ctx, bc_compile_source_methods(symbol_src, methods, 64, &method_count), 1,
              "Symbol.st compiles through chunk pipeline");
    ASSERT_EQ(ctx, method_count, 1, "Symbol.st method count");

    ASSERT_EQ(ctx, read_file("src/smalltalk/Array.st", array_src, sizeof(array_src)), 1,
              "src/smalltalk/Array.st exists");
    ASSERT_EQ(ctx, strstr(array_src, "size\n    <primitive: 11>") != NULL, 1,
              "Array>>size uses primitive 11");
    ASSERT_EQ(ctx, strstr(array_src, "at: index\n    <primitive: 7>") != NULL, 1,
              "Array>>at: uses primitive 7");
    ASSERT_EQ(ctx, strstr(array_src, "at: index put: value\n    <primitive: 9>") != NULL, 1,
              "Array>>at:put: uses primitive 9");
    ASSERT_EQ(ctx, bc_compile_source_methods(array_src, methods, 64, &method_count), 1,
              "Array.st compiles through chunk pipeline");
    ASSERT_EQ(ctx, method_count, 3, "Array.st method count");

    ASSERT_EQ(ctx, read_file("src/smalltalk/Association.st", association_src, sizeof(association_src)), 1,
              "src/smalltalk/Association.st exists");
    ASSERT_EQ(ctx, strstr(association_src, "key: aKey value: aValue") != NULL, 1,
              "Association has key:value: initializer");
    ASSERT_EQ(ctx, strstr(association_src, "value: anObject") != NULL, 1,
              "Association has value: mutator");
    ASSERT_EQ(ctx, bc_compile_source_methods(association_src, methods, 64, &method_count), 1,
              "Association.st compiles through chunk pipeline");
    ASSERT_EQ(ctx, method_count, 5, "Association.st method count");

    ASSERT_EQ(ctx, read_file("src/smalltalk/Dictionary.st", dictionary_src, sizeof(dictionary_src)), 1,
              "src/smalltalk/Dictionary.st exists");
    ASSERT_EQ(ctx, strstr(dictionary_src, "indexOfKey: aKey startingAt: index") != NULL, 1,
              "Dictionary has linear-search helper");
    ASSERT_EQ(ctx, strstr(dictionary_src, "self indexOfKey: aKey startingAt: index + 1") != NULL, 1,
              "Dictionary search advances linearly");
    ASSERT_EQ(ctx, strstr(dictionary_src, "assoc := Association new.") != NULL, 1,
              "Dictionary stores Associations");
    ASSERT_EQ(ctx, strstr(dictionary_src, "associations at: tally put: assoc.") != NULL, 1,
              "Dictionary appends association in storage array");
    ASSERT_EQ(ctx, strstr(dictionary_src, "at: aKey ifAbsent: aBlock") != NULL, 1,
              "Dictionary supports at:ifAbsent:");

    ASSERT_EQ(ctx, read_file("src/smalltalk/SystemDictionary.st", system_dictionary_src, sizeof(system_dictionary_src)), 1,
              "src/smalltalk/SystemDictionary.st exists");
    ASSERT_EQ(ctx, strstr(system_dictionary_src, "initializeSmalltalkNamespace") != NULL, 1,
              "SystemDictionary has namespace initializer");
    ASSERT_EQ(ctx, strstr(system_dictionary_src, "globals := Dictionary new.") != NULL, 1,
              "Namespace initializer allocates Dictionary");
    ASSERT_EQ(ctx, strstr(system_dictionary_src, "globals at: #Smalltalk put: globals.") != NULL, 1,
              "Namespace initializer stores #Smalltalk self-binding");

    ASSERT_EQ(ctx, read_file("src/smalltalk/ReadStream.st", read_stream_src, sizeof(read_stream_src)), 1,
              "src/smalltalk/ReadStream.st exists");
    ASSERT_EQ(ctx, strstr(read_stream_src, "on: aCollection") != NULL, 1,
              "ReadStream has class-side constructor protocol");
    ASSERT_EQ(ctx, strstr(read_stream_src, "next") != NULL, 1,
              "ReadStream has next");
    ASSERT_EQ(ctx, strstr(read_stream_src, "peek") != NULL, 1,
              "ReadStream has peek");
    ASSERT_EQ(ctx, strstr(read_stream_src, "atEnd") != NULL, 1,
              "ReadStream has atEnd");
    ASSERT_EQ(ctx, strstr(read_stream_src, "upToEnd") != NULL, 1,
              "ReadStream has upToEnd");

    ASSERT_EQ(ctx, read_file("src/smalltalk/WriteStream.st", write_stream_src, sizeof(write_stream_src)), 1,
              "src/smalltalk/WriteStream.st exists");
    ASSERT_EQ(ctx, strstr(write_stream_src, "nextPut: aByte") != NULL, 1,
              "WriteStream has nextPut:");
    ASSERT_EQ(ctx, strstr(write_stream_src, "nextPutAll: aCollection") != NULL, 1,
              "WriteStream has nextPutAll:");
    ASSERT_EQ(ctx, strstr(write_stream_src, "contents") != NULL, 1,
              "WriteStream has contents");

    ASSERT_EQ(ctx, read_file("tests/ExpressionSpecs.txt", expr_specs_src, sizeof(expr_specs_src)), 1,
              "tests/ExpressionSpecs.txt exists");
    ASSERT_EQ(ctx, strstr(expr_specs_src, "simple add | 1 + 2 | 3") != NULL, 1,
              "Expression specs include arithmetic baseline");
    ASSERT_EQ(ctx, strstr(expr_specs_src, "nested send helper | self bar | 7") != NULL, 1,
              "Expression specs include nested send baseline");
}
