#!/usr/bin/env python3
import re

with open("test.c") as f:
    lines = f.readlines()
m = {}
push_start = push_end = None
for i, line in enumerate(lines):
    if "// Allocate stack memory" in line:
        push_start = i
    if "// --- Initialize Object Memory" in line:
        push_end = i
    if "// --- Method Activation" in line:
        m["stack"] = i
    if "// --- Section 7:" in line:
        m["tagged"] = i
    if "// --- Section 8:" in line:
        m["object"] = i
    if "Section 10" in line and "---" in line:
        m["dispatch"] = i
    if "// --- Section 12b: Blocks" in line:
        m["blocks"] = i
    if "// --- Section 13: Factorial" in line:
        m["factorial"] = i
    if "printf" in line and "passed" in line and "failed" in line:
        m["end"] = i
for k in sorted(m, key=lambda x: m[x]):
    print("  %-12s line %d" % (k, m[k] + 1))
splits = [
    ("test_stack.c", "test_stack", m["stack"], m["tagged"]),
    ("test_tagged.c", "test_tagged", m["tagged"], m["object"]),
    ("test_object.c", "test_object", m["object"], m["dispatch"]),
    ("test_dispatch.c", "test_dispatch", m["dispatch"], m["blocks"]),
    ("test_blocks.c", "test_blocks", m["blocks"], m["factorial"]),
    ("test_factorial.c", "test_factorial", m["factorial"], m["end"]),
]
func_names = [s[1] for s in splits]
hdr = ["#ifndef TEST_DEFS_H", "#define TEST_DEFS_H"]
hdr += [
    "#include <stdio.h>",
    "#include <stdint.h>",
    "#include <stdlib.h>",
    "#include <string.h>",
    "",
]
src = "".join(lines)
for em in re.finditer(r"(extern [^;]+;)", src):
    hdr.append(" ".join(em.group(1).split()))
hdr.append("")
for line in lines:
    s = line.strip()
    if (
        s.startswith("#define ")
        and "OM_SIZE" not in s
        and "ASSERT_EQ" not in s
        and "WRITE_U32" not in s
    ):
        hdr.append(s)
hdr += ["", "#define OM_SIZE (1024 * 1024)", ""]
hdr += [
    "static inline void WRITE_U32(uint8_t *p, uint32_t v) {",
    "    p[0]=v&0xFF; p[1]=(v>>8)&0xFF; p[2]=(v>>16)&0xFF; p[3]=(v>>24)&0xFF;",
    "}",
    "",
]
hdr.append("typedef struct {")
for v in [
    "uint64_t *om;",
    "uint64_t *class_class;",
    "uint64_t *smallint_class;",
    "uint64_t *block_class;",
    "uint64_t *test_class;",
    "uint64_t receiver;",
    "uint64_t method;",
    "uint64_t class_table[4];",
    "uint64_t stack[STACK_WORDS];",
    "int passes;",
    "int failures;",
]:
    hdr.append("    " + v)
hdr += ["} TestContext;", ""]
hdr.append("#define ASSERT_EQ(ctx, a, b, msg) do { \\")
hdr.append("    uint64_t _a = (a), _b = (b); \\")
hdr.append(
    '    if (_a!=_b) { printf("FAIL: %s (expected %llu, got %llu)\\n",msg,_b,_a); (ctx)->failures++; } \\'
)
hdr.append('    else { printf("PASS: %s\\n",msg); (ctx)->passes++; } \\')
hdr += ["} while (0)", ""]
hdr += [
    "void debug_mnu(uint64_t selector);",
    "void debug_oom(void);",
    "void debug_unknown_prim(uint64_t prim_index);",
    "",
]
for fn in func_names:
    hdr.append("void %s(TestContext *ctx);" % fn)
hdr += ["", "#endif"]
with open("test_defs.h", "w") as f:
    f.write("\n".join(hdr) + "\n")
print("wrote test_defs.h (%d lines)" % len(hdr))
CTX = [
    "uint64_t *om=ctx->om;",
    "uint64_t *class_class=ctx->class_class;",
    "uint64_t *smallint_class=ctx->smallint_class;",
    "uint64_t *block_class=ctx->block_class;",
    "uint64_t *test_class=ctx->test_class;",
    "uint64_t receiver=ctx->receiver;",
    "uint64_t method=ctx->method;",
    "uint64_t *class_table=ctx->class_table;",
    "uint64_t *stack=ctx->stack;",
    "(void)om;(void)class_class;(void)smallint_class;",
    "(void)block_class;(void)test_class;(void)receiver;",
    "(void)method;(void)class_table;(void)stack;",
]
VARS = [
    ("uint64_t *sp;", "sp", 1),
    ("uint64_t *fp;", "fp", 1),
    ("uint64_t ip;", "ip", 0),
    ("uint64_t fake_ip=0x1000;", "fake_ip", 0),
    ("uint64_t caller_fp_val=0xBEEF;", "caller_fp_val", 0),
    ("uint64_t caller_ip_val=0xDEAD;", "caller_ip_val", 0),
    ("uint64_t result;", "result", 0),
]


def fix_redecl(line):
    for _, name, isptr in VARS:
        pfx = r"^(\s+)uint64_t\s+"
        if isptr:
            pfx += r"\*"
        pfx += name + r"\s*="
        if re.match(pfx, line):
            rep = r"^(\s+)uint64_t\s+"
            if isptr:
                rep += r"\*"
            rep += name
            return re.sub(rep, r"\g<1>" + name, line)
    return line


for filename, funcname, start, end in splits:
    body = lines[start:end]
    bt = "".join(body)
    with open(filename, "w") as f:
        f.write('#include "test_defs.h"\n\nvoid %s(TestContext *ctx)\n{\n' % funcname)
        for c in CTX:
            f.write("    %s\n" % c)
        if funcname == "test_object":
            f.write(
                "    uint64_t *iv_class = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 3);\n"
            )
            f.write("    OBJ_FIELD(iv_class, CLASS_SUPERCLASS) = tagged_nil();\n")
            f.write("    OBJ_FIELD(iv_class, CLASS_METHOD_DICT) = tagged_nil();\n")
            f.write("    OBJ_FIELD(iv_class, CLASS_INST_SIZE) = tag_smallint(4);\n")
        for decl, name, _ in VARS:
            if name in bt:
                f.write("    %s\n" % decl)
        f.write("\n")
        if funcname == "test_stack" and push_start is not None:
            f.write(
                "    sp = (uint64_t *)((uint8_t *)stack + STACK_WORDS * sizeof(uint64_t));\n\n"
            )
            for line in lines[push_start:push_end]:
                s = line.strip()
                if (
                    s.startswith("uint64_t stack[")
                    or s.startswith("uint64_t *sp")
                    or s.startswith("// Allocate")
                    or s.startswith("// SP starts")
                ):
                    continue
                line = re.sub(r"\bASSERT_EQ\(", "ASSERT_EQ(ctx, ", line)
                line = line.replace("sizeof(stack)", "STACK_WORDS * sizeof(uint64_t)")
                f.write(line)
            f.write("\n")
        for line in body:
            line = re.sub(r"\bASSERT_EQ\(", "ASSERT_EQ(ctx, ", line)
            line = fix_redecl(line)
            line = line.replace("sizeof(stack)", "STACK_WORDS * sizeof(uint64_t)")
            f.write(line)
        f.write("\n    ctx->smallint_class=smallint_class;\n")
        f.write(
            "    memcpy(ctx->class_table,class_table,sizeof(ctx->class_table));\n}\n"
        )
    print("wrote %s" % filename)
print("Done.")
