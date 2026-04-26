#ifndef SMALLTALK_TOOL_SUPPORT_H
#define SMALLTALK_TOOL_SUPPORT_H

#include "smalltalk_world.h"

typedef enum
{
    LO_RUNTIME_SOURCE_EXISTING_CLASS = 1,
    LO_RUNTIME_SOURCE_METHODS_ONLY = 0,
    LO_RUNTIME_SOURCE_NEW_CLASS = 2
} LoRuntimeSourceInstallMode;

typedef struct
{
    const char *path;
    LoRuntimeSourceInstallMode install_mode;
} LoRuntimeSource;

void lo_init_context(TestContext *ctx, SmalltalkWorld *world) LO_NO_ALLOC;
int lo_install_runtime_sources(SmalltalkWorld *world,
                               const LoRuntimeSource *files,
                               size_t count) LO_ALLOCATES;
int lo_install_smalltalk_compiler_sources(SmalltalkWorld *world) LO_ALLOCATES;

#endif
