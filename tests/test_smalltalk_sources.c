#include "test_defs.h"
#include "bootstrap_compiler.h"

typedef struct
{
    const char *path;
    const char *label;
    int should_compile;
} SmalltalkSourceFile;

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
    static const SmalltalkSourceFile corpus_files[] = {
        {"src/smalltalk/Array.st", "Array.st corpus compile", 1},
        {"src/smalltalk/Association.st", "Association.st corpus compile", 1},
        {"src/smalltalk/BlockClosure.st", "BlockClosure.st corpus compile", 1},
        {"src/smalltalk/Character.st", "Character.st corpus compile", 1},
        {"src/smalltalk/Class.st", "Class.st corpus compile", 1},
        {"src/smalltalk/Collection.st", "Collection.st corpus compile", 0},
        {"src/smalltalk/Context.st", "Context.st corpus compile", 1},
        {"src/smalltalk/Dictionary.st", "Dictionary.st corpus compile", 1},
        {"src/smalltalk/ExpressionSpecTest.st", "ExpressionSpecTest.st corpus compile", 1},
        {"src/smalltalk/False.st", "False.st corpus compile", 1},
        {"src/smalltalk/LSPDocument.st", "LSPDocument.st corpus compile", 1},
        {"src/smalltalk/LSPMethodSpan.st", "LSPMethodSpan.st corpus compile", 1},
        {"src/smalltalk/LSPSourceIndex.st", "LSPSourceIndex.st corpus compile", 1},
        {"src/smalltalk/Object.st", "Object.st corpus compile", 1},
        {"src/smalltalk/ReadStream.st", "ReadStream.st corpus compile", 1},
        {"src/smalltalk/SmallInteger.st", "SmallInteger.st corpus compile", 1},
        {"src/smalltalk/Stdio.st", "Stdio.st corpus compile", 1},
        {"src/smalltalk/String.st", "String.st corpus compile", 1},
        {"src/smalltalk/Symbol.st", "Symbol.st corpus compile", 1},
        {"src/smalltalk/SystemDictionary.st", "SystemDictionary.st corpus compile", 1},
        {"src/smalltalk/TestCase.st", "TestCase.st corpus compile", 1},
        {"src/smalltalk/TestResult.st", "TestResult.st corpus compile", 1},
        {"src/smalltalk/TestSuite.st", "TestSuite.st corpus compile", 1},
        {"src/smalltalk/Tokenizer.st", "Tokenizer.st corpus compile", 1},
        {"src/smalltalk/Token.st", "Token.st corpus compile", 1},
        {"src/smalltalk/ASTNodes.st", "ASTNodes.st corpus compile", 0},
        {"src/smalltalk/Parser.st", "Parser.st corpus compile", 1},
        {"src/smalltalk/CodeGenerator.st", "CodeGenerator.st corpus compile", 1},
        {"src/smalltalk/Compiler.st", "Compiler.st corpus compile", 1},
        {"src/smalltalk/True.st", "True.st corpus compile", 1},
        {"src/smalltalk/UndefinedObject.st", "UndefinedObject.st corpus compile", 1},
        {"src/smalltalk/WriteStream.st", "WriteStream.st corpus compile", 1},
    };
    char corpus_src[32768];
    char class_src[4096];
    char object_src[4096];
    char string_src[8192];
    char symbol_src[2048];
    char array_src[4096];
    char association_src[4096];
    char collection_src[4096];
    char dictionary_src[12288];
    char system_dictionary_src[4096];
    char read_stream_src[8192];
    char stdio_src[4096];
    char write_stream_src[8192];
    char undefined_object_src[2048];
    char context_src[2048];
    char test_result_src[4096];
    char test_case_src[4096];
    char test_suite_src[4096];
    char tokenizer_src[32768];
    char expression_spec_test_src[2048];
    char lsp_document_src[4096];
    char lsp_method_span_src[4096];
    char lsp_source_index_src[4096];
    char expr_specs_src[4096];
    char expr_fixture_specs_src[8192];
    BCompiledMethodDef methods[64];
    int method_count = 0;

    for (uint64_t index = 0; index < sizeof(corpus_files) / sizeof(corpus_files[0]); index++)
    {
        ASSERT_EQ(ctx, read_file(corpus_files[index].path, corpus_src, sizeof(corpus_src)), 1,
                  corpus_files[index].path);
        ASSERT_EQ(ctx, bc_compile_source_methods(corpus_src, methods, 64, &method_count),
                  corpus_files[index].should_compile,
                  corpus_files[index].label);
    }

    ASSERT_EQ(ctx, read_file("src/smalltalk/Object.st", object_src, sizeof(object_src)), 1,
              "src/smalltalk/Object.st exists");
    ASSERT_EQ(ctx, strstr(object_src, "error: aString") != NULL, 1,
              "Object>>error: exists");
    ASSERT_EQ(ctx, strstr(object_src, "<primitive: 29>") != NULL, 1,
              "Object>>error: uses primitive 29");
    ASSERT_EQ(ctx, strstr(object_src, "== anObject\n    <primitive: 12>\n    ^ false") != NULL, 1,
              "Object>>== has explicit fallback body");
    ASSERT_EQ(ctx, strstr(object_src, "isNil\n    ^ false") != NULL, 1,
              "Object>>isNil exists");
    ASSERT_EQ(ctx, bc_compile_source_methods(object_src, methods, 64, &method_count), 1,
              "Object.st compiles through chunk pipeline");
    ASSERT_EQ(ctx, method_count, 9, "Object.st method count");

    ASSERT_EQ(ctx, read_file("src/smalltalk/Class.st", class_src, sizeof(class_src)), 1,
              "src/smalltalk/Class.st exists");
    ASSERT_EQ(ctx, strstr(class_src, "new\n    ^ self basicNew") != NULL, 1,
              "Class>>new delegates to basicNew");
    ASSERT_EQ(ctx, strstr(class_src, "new: size\n    ^ self basicNew: size") != NULL, 1,
              "Class>>new: delegates to basicNew:");
    ASSERT_EQ(ctx, strstr(class_src, "superclass\n    <primitive: 30>") != NULL, 1,
              "Class>>superclass uses live-image primitive");
    ASSERT_EQ(ctx, strstr(class_src, "name\n    <primitive: 31>") != NULL, 1,
              "Class>>name uses live-image primitive");
    ASSERT_EQ(ctx, strstr(class_src, "includesSelector: aSelector\n    <primitive: 32>") != NULL, 1,
              "Class>>includesSelector: uses live-image primitive");

    ASSERT_EQ(ctx, read_file("src/smalltalk/Stdio.st", stdio_src, sizeof(stdio_src)), 1,
              "src/smalltalk/Stdio.st exists");
    ASSERT_EQ(ctx, strstr(stdio_src, "readFd: anFd count: aCount\n    <primitive: 35>") != NULL, 1,
              "Stdio class>>readFd:count: uses fd-read primitive");
    ASSERT_EQ(ctx, strstr(stdio_src, "writeFd: anFd string: aString\n    <primitive: 36>") != NULL, 1,
              "Stdio class>>writeFd:string: uses fd-write primitive");
    ASSERT_EQ(ctx, strstr(class_src, "sourceAtSelector: aSelector") != NULL, 1,
              "Class>>sourceAtSelector: exists");
    ASSERT_EQ(ctx, bc_compile_source_methods(class_src, methods, 64, &method_count), 1,
              "Class.st compiles through chunk pipeline");
    ASSERT_EQ(ctx, method_count, 8, "Class.st method count");
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
    ASSERT_EQ(ctx, strstr(string_src, "= aString\n    <primitive: 25>\n    false") != NULL, 1,
              "String>>= has explicit false fallback");
    ASSERT_EQ(ctx, bc_compile_source_methods(string_src, methods, 64, &method_count), 1,
              "String.st compiles through chunk pipeline");
    ASSERT_EQ(ctx, method_count, 8, "String.st method count");
    ASSERT_EQ(ctx, read_file("src/smalltalk/Symbol.st", symbol_src, sizeof(symbol_src)), 1,
              "src/smalltalk/Symbol.st exists");
    ASSERT_EQ(ctx, strstr(symbol_src, "= aSymbol") != NULL, 1,
              "Symbol>>= method exists");
    ASSERT_EQ(ctx, strstr(symbol_src, "<primitive: 28>") != NULL, 1,
              "Symbol>>= uses primitive 28");
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
    ASSERT_EQ(ctx, strstr(array_src, "= anArray") != NULL, 1,
              "Array>>= method exists");
    ASSERT_EQ(ctx, strstr(array_src, "elementsEqualTo: anArray startingAt: index") != NULL, 1,
              "Array recursive element equality helper exists");
    ASSERT_EQ(ctx, bc_compile_source_methods(array_src, methods, 64, &method_count), 1,
              "Array.st compiles through chunk pipeline");
    ASSERT_EQ(ctx, method_count, 18, "Array.st method count");

    ASSERT_EQ(ctx, read_file("src/smalltalk/Collection.st", collection_src, sizeof(collection_src)), 1,
              "src/smalltalk/Collection.st exists");
    ASSERT_EQ(ctx, strstr(collection_src, "basicAt: index") != NULL, 1,
              "Collection has basicAt:");
    ASSERT_EQ(ctx, strstr(collection_src, "basicAt: index put: value") != NULL, 1,
              "Collection has basicAt:put:");
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
    ASSERT_EQ(ctx, strstr(dictionary_src, "associationAt: aKey") != NULL, 1,
              "Dictionary exposes associationAt:");
    ASSERT_EQ(ctx, strstr(dictionary_src, "associations at: tally put: assoc.") != NULL, 1,
              "Dictionary appends association in storage array");
    ASSERT_EQ(ctx, strstr(dictionary_src, "at: aKey ifAbsent: aBlock") != NULL, 1,
              "Dictionary supports at:ifAbsent:");
    ASSERT_EQ(ctx, strstr(dictionary_src, "smalltalk\n    <primitive: 33>") != NULL, 1,
              "Dictionary class>>smalltalk uses live-image primitive");
    ASSERT_EQ(ctx, strstr(dictionary_src, "classNamed: aClassName") != NULL, 1,
              "Dictionary>>classNamed: exists");
    ASSERT_EQ(ctx, strstr(dictionary_src, "methodSourceForClass: aClassName selector: aSelector\n    <primitive: 34>") != NULL, 1,
              "Dictionary>>methodSourceForClass:selector: uses live-image primitive");
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

    ASSERT_EQ(ctx, read_file("src/smalltalk/UndefinedObject.st", undefined_object_src, sizeof(undefined_object_src)), 1,
              "src/smalltalk/UndefinedObject.st exists");
    ASSERT_EQ(ctx, strstr(undefined_object_src, "printString") != NULL, 1,
              "UndefinedObject has printString");
    ASSERT_EQ(ctx, strstr(undefined_object_src, "^ 'nil'") != NULL, 1,
              "UndefinedObject>>printString returns 'nil'");
    ASSERT_EQ(ctx, strstr(undefined_object_src, "isNil\n    ^ true") != NULL, 1,
              "UndefinedObject>>isNil returns true");
    ASSERT_EQ(ctx, bc_compile_source_methods(undefined_object_src, methods, 64, &method_count), 1,
              "UndefinedObject.st compiles through chunk pipeline");
    ASSERT_EQ(ctx, method_count, 2, "UndefinedObject.st method count");

    ASSERT_EQ(ctx, read_file("src/smalltalk/Context.st", context_src, sizeof(context_src)), 1,
              "src/smalltalk/Context.st exists");
    ASSERT_EQ(ctx, strstr(context_src, "receiver\n    ^ receiver") != NULL, 1,
              "Context>>receiver is a normal ivar accessor");
    ASSERT_EQ(ctx, bc_compile_source_methods(context_src, methods, 64, &method_count), 1,
              "Context.st compiles through chunk pipeline");
    ASSERT_EQ(ctx, method_count, 2, "Context.st method count");

    ASSERT_EQ(ctx, read_file("tests/ExpressionSpecs.txt", expr_specs_src, sizeof(expr_specs_src)), 1,
              "tests/ExpressionSpecs.txt exists");
    ASSERT_EQ(ctx, strstr(expr_specs_src, "simple add | 1 + 2 | 3") != NULL, 1,
              "Expression specs include arithmetic baseline");
    ASSERT_EQ(ctx, strstr(expr_specs_src, "nested send helper | self bar | 7") != NULL, 1,
              "Expression specs include nested send baseline");
    ASSERT_EQ(ctx, strstr(expr_specs_src, "thisContext receiver isNil | thisContext receiver isNil | false") != NULL, 1,
              "Expression specs include thisContext receiver isNil");

    ASSERT_EQ(ctx, read_file("tests/ExpressionFixtureSpecs.txt", expr_fixture_specs_src, sizeof(expr_fixture_specs_src)), 1,
              "tests/ExpressionFixtureSpecs.txt exists");
    ASSERT_EQ(ctx, strstr(expr_fixture_specs_src, "class: FactorialHelper < Object") != NULL, 1,
              "Expression fixture specs declare factorial helper class");
    ASSERT_EQ(ctx, strstr(expr_fixture_specs_src, "ifTrue: [1]") != NULL, 1,
              "Expression fixture specs include factorial setup");
    ASSERT_EQ(ctx, strstr(expr_fixture_specs_src, "expression: self factorial: 10") != NULL, 1,
              "Expression fixture specs include higher-level factorial expression");

    ASSERT_EQ(ctx, read_file("src/smalltalk/TestResult.st", test_result_src, sizeof(test_result_src)), 1,
              "src/smalltalk/TestResult.st exists");
    ASSERT_EQ(ctx, strstr(test_result_src, "recordFailure: aCase selector: aSelector reason: aSymbol backtrace: aBacktrace") != NULL, 1,
              "TestResult records failure backtraces");
    ASSERT_EQ(ctx, bc_compile_source_methods(test_result_src, methods, 64, &method_count), 1,
              "TestResult.st compiles through chunk pipeline");
    ASSERT_EQ(ctx, method_count, 16, "TestResult.st method count");

    ASSERT_EQ(ctx, read_file("src/smalltalk/TestCase.st", test_case_src, sizeof(test_case_src)), 1,
              "src/smalltalk/TestCase.st exists");
    ASSERT_EQ(ctx, strstr(test_case_src, "backtraceFrom: aContext") != NULL, 1,
              "TestCase captures failure backtraces");
    ASSERT_EQ(ctx, strstr(test_case_src, "selfTest") != NULL, 1,
              "TestCase has class-side selfTest entrypoint");
    ASSERT_EQ(ctx, bc_compile_source_methods(test_case_src, methods, 64, &method_count), 1,
              "TestCase.st compiles through chunk pipeline");
    ASSERT_EQ(ctx, method_count, 23, "TestCase.st method count");

    ASSERT_EQ(ctx, read_file("src/smalltalk/TestSuite.st", test_suite_src, sizeof(test_suite_src)), 1,
              "src/smalltalk/TestSuite.st exists");
    ASSERT_EQ(ctx, strstr(test_suite_src, "add: aTest") != NULL, 1,
              "TestSuite grows incrementally");
    ASSERT_EQ(ctx, bc_compile_source_methods(test_suite_src, methods, 64, &method_count), 1,
              "TestSuite.st compiles through chunk pipeline");
    ASSERT_EQ(ctx, method_count, 11, "TestSuite.st method count");

    ASSERT_EQ(ctx, read_file("src/smalltalk/Tokenizer.st", tokenizer_src, sizeof(tokenizer_src)), 1,
              "src/smalltalk/Tokenizer.st exists");
    ASSERT_EQ(ctx, strstr(tokenizer_src, "tokens") != NULL, 1,
              "Tokenizer has tokens entrypoint");
    ASSERT_EQ(ctx, strstr(tokenizer_src, "nextTokenFromStream") != NULL, 1,
              "Tokenizer has nextTokenFromStream method");
    ASSERT_EQ(ctx, bc_compile_source_methods(tokenizer_src, methods, 64, &method_count), 1,
              "Tokenizer.st compiles through chunk pipeline");
    ASSERT_EQ(ctx, method_count > 20, 1, "Tokenizer.st has many methods");

    {
        char codegen_src[32768];
        int found_visit_message = 0;
        ASSERT_EQ(ctx, read_file("src/smalltalk/CodeGenerator.st", codegen_src, sizeof(codegen_src)), 1,
                  "src/smalltalk/CodeGenerator.st exists");
        ASSERT_EQ(ctx, bc_compile_source_methods(codegen_src, methods, 64, &method_count), 1,
                  "CodeGenerator.st compiles through chunk pipeline");
        for (int index = 0; index < method_count; index++)
        {
            if (strcmp(methods[index].header.selector, "visitMessage:") == 0)
            {
                found_visit_message = 1;
                ASSERT_EQ(ctx, methods[index].header.arg_count, 1,
                          "CodeGenerator visitMessage: has one arg");
                ASSERT_EQ(ctx, methods[index].body.temp_count, 1,
                          "CodeGenerator visitMessage: has one temp");
            }
        }
        ASSERT_EQ(ctx, found_visit_message, 1,
                  "CodeGenerator visitMessage: compiled method found");
    }

    ASSERT_EQ(ctx, read_file("src/smalltalk/ExpressionSpecTest.st", expression_spec_test_src, sizeof(expression_spec_test_src)), 1,
              "src/smalltalk/ExpressionSpecTest.st exists");
    ASSERT_EQ(ctx, strstr(expression_spec_test_src, "testThisContextReceiverIsNil") != NULL, 1,
              "ExpressionSpecTest has migrated xUnit test");
    ASSERT_EQ(ctx, strstr(expression_spec_test_src, "runOn: aResult") != NULL, 1,
              "ExpressionSpecTest has minimal in-image runner");
    ASSERT_EQ(ctx, bc_compile_source_methods(expression_spec_test_src, methods, 64, &method_count), 1,
              "ExpressionSpecTest.st compiles through chunk pipeline");
    ASSERT_EQ(ctx, method_count, 2, "ExpressionSpecTest.st method count");

    ASSERT_EQ(ctx, read_file("src/smalltalk/LSPMethodSpan.st", lsp_method_span_src, sizeof(lsp_method_span_src)), 1,
              "src/smalltalk/LSPMethodSpan.st exists");
    ASSERT_EQ(ctx, strstr(lsp_method_span_src, "className: aClassName selector: aSelector") != NULL, 1,
              "LSPMethodSpan has class-side constructor");
    ASSERT_EQ(ctx, strstr(lsp_method_span_src, "start: aStart stop: aStop") != NULL, 1,
              "LSPMethodSpan has start:stop: updater");
    ASSERT_EQ(ctx, strstr(lsp_method_span_src, "initializeClassName: aClassName selector: aSelector") != NULL, 1,
              "LSPMethodSpan has initializer");
    ASSERT_EQ(ctx, bc_compile_source_methods(lsp_method_span_src, methods, 64, &method_count), 1,
              "LSPMethodSpan.st compiles through chunk pipeline");
    ASSERT_EQ(ctx, method_count, 13, "LSPMethodSpan.st method count");

    ASSERT_EQ(ctx, read_file("src/smalltalk/LSPSourceIndex.st", lsp_source_index_src, sizeof(lsp_source_index_src)), 1,
              "src/smalltalk/LSPSourceIndex.st exists");
    ASSERT_EQ(ctx, strstr(lsp_source_index_src, "addMethodSpan: aMethodSpan") != NULL, 1,
              "LSPSourceIndex has addMethodSpan:");
    ASSERT_EQ(ctx, strstr(lsp_source_index_src, "methodSpanAtClass: aClassName selector: aSelector") != NULL, 1,
              "LSPSourceIndex has lookup protocol");
    ASSERT_EQ(ctx, strstr(lsp_source_index_src, "keyForClass: aClassName selector: aSelector") != NULL, 1,
              "LSPSourceIndex has key builder");
    ASSERT_EQ(ctx, bc_compile_source_methods(lsp_source_index_src, methods, 64, &method_count), 1,
              "LSPSourceIndex.st compiles through chunk pipeline");
    ASSERT_EQ(ctx, method_count, 7, "LSPSourceIndex.st method count");

    ASSERT_EQ(ctx, read_file("src/smalltalk/LSPDocument.st", lsp_document_src, sizeof(lsp_document_src)), 1,
              "src/smalltalk/LSPDocument.st exists");
    ASSERT_EQ(ctx, strstr(lsp_document_src, "parseMethodAst") != NULL, 1,
              "LSPDocument has cached parse entrypoint");
    ASSERT_EQ(ctx, strstr(lsp_document_src, "updateText: aText version: aVersion") != NULL, 1,
              "LSPDocument can update text/version");
    ASSERT_EQ(ctx, strstr(lsp_document_src, "recordMethodSpan: aMethodSpan") != NULL, 1,
              "LSPDocument can record method spans");
    ASSERT_EQ(ctx, bc_compile_source_methods(lsp_document_src, methods, 64, &method_count), 1,
              "LSPDocument.st compiles through chunk pipeline");
    ASSERT_EQ(ctx, method_count, 11, "LSPDocument.st method count");
}
