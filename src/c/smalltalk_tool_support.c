#include "smalltalk_tool_support.h"

#include "bootstrap_compiler.h"
#include "test_defs.h"

#include <stdio.h>
#include <string.h>

void debug_mnu(uint64_t selector)
{
    fprintf(stderr, "Message not understood: 0x%llx\n", (unsigned long long)selector);
}

void debug_mnu_context(uint64_t selector, uint64_t *current_cm, uint64_t selector_index)
{
    (void)current_cm;
    fprintf(stderr, "Message not understood: selector=0x%llx literal=%llu\n",
            (unsigned long long)selector, (unsigned long long)selector_index);
}

void debug_oom(void)
{
    fprintf(stderr, "Out of object memory\n");
}

void debug_unknown_prim(uint64_t prim_index)
{
    fprintf(stderr, "Unknown primitive: %llu\n", (unsigned long long)prim_index);
}

void debug_error(uint64_t message, uint64_t *fp, uint64_t *class_table)
{
    (void)fp;
    (void)class_table;
    fprintf(stderr, "VM error: 0x%llx\n", (unsigned long long)message);
}

void lo_init_context(TestContext *ctx, SmalltalkWorld *world)
{
    memset(ctx, 0, sizeof(*ctx));
    ctx->om = world->om;
    ctx->class_class = world->class_class;
    ctx->smallint_class = world->smallint_class;
    ctx->block_class = world->block_class;
    ctx->undefined_object_class = world->undefined_class;
    ctx->character_class = world->character_class;
    ctx->string_class = world->string_class;
    ctx->symbol_class = world->symbol_class;
    ctx->context_class = world->context_class;
    ctx->symbol_table = world->symbol_table;
    ctx->class_table = world->class_table;
}

int lo_install_runtime_sources(SmalltalkWorld *world,
                               const LoRuntimeSource *files,
                               size_t count)
{
    size_t index;

    for (index = 0; index < count; index++)
    {
        int installed;

        if (files[index].install_mode == LO_RUNTIME_SOURCE_NEW_CLASS)
        {
            installed = smalltalk_world_install_class_file(world, files[index].path) != NULL;
        }
        else if (files[index].install_mode == LO_RUNTIME_SOURCE_EXISTING_CLASS)
        {
            installed = smalltalk_world_install_existing_class_file(world, files[index].path) != NULL;
        }
        else
        {
            installed = smalltalk_world_install_st_file(world, files[index].path);
        }
        if (!installed)
        {
            fprintf(stderr, "failed to install %s\n", files[index].path);
            return 0;
        }
    }

    return 1;
}

int lo_install_smalltalk_compiler_sources(SmalltalkWorld *world)
{
    const char *class_files[] = {
        "src/smalltalk/Token.st",
        "src/smalltalk/ReadStream.st",
        "src/smalltalk/Tokenizer.st",
    };
    size_t class_count = sizeof(class_files) / sizeof(class_files[0]);
    size_t index;

    for (index = 0; index < class_count; index++)
    {
        if (smalltalk_world_install_class_file(world, class_files[index]) == NULL)
        {
            fprintf(stderr, "failed to install %s\n", class_files[index]);
            return 0;
        }
    }
    if (!bc_compile_and_install_classes_file(world->om, world->class_class,
                                             world->string_class, world->array_class,
                                             world->association_class, NULL, 0,
                                             "src/smalltalk/ASTNodes.st"))
    {
        fprintf(stderr, "failed to install src/smalltalk/ASTNodes.st\n");
        return 0;
    }
    if (smalltalk_world_install_class_file(world, "src/smalltalk/Parser.st") == NULL)
    {
        fprintf(stderr, "failed to install src/smalltalk/Parser.st\n");
        return 0;
    }
    if (smalltalk_world_install_class_file(world, "src/smalltalk/CodeGenerator.st") == NULL)
    {
        fprintf(stderr, "failed to install src/smalltalk/CodeGenerator.st\n");
        return 0;
    }
    if (smalltalk_world_install_class_file(world, "src/smalltalk/Compiler.st") == NULL)
    {
        fprintf(stderr, "failed to install src/smalltalk/Compiler.st\n");
        return 0;
    }
    return 1;
}
