#include "vm_defs.h"

#include <signal.h>
#include <stdint.h>

extern uint64_t oop_class(uint64_t oop, uint64_t *class_table);
extern uint64_t class_lookup(uint64_t *klass, uint64_t selector);
extern void activate_method(uint64_t **sp_ptr, uint64_t **fp_ptr, uint64_t saved_ip,
                            uint64_t method, uint64_t num_args, uint64_t num_temps);
extern uint64_t tag_smallint(int64_t value);
extern int64_t untag_smallint(uint64_t tagged);

static uint32_t read_u32(uint8_t **ip)
{
    uint8_t *p = *ip;
    uint32_t value = (uint32_t)p[0] |
                     ((uint32_t)p[1] << 8) |
                     ((uint32_t)p[2] << 16) |
                     ((uint32_t)p[3] << 24);
    *ip = p + 4;
    return value;
}

static void push(uint64_t **sp_ptr, uint64_t value)
{
    uint64_t *sp = *sp_ptr - 1;
    *sp = value;
    *sp_ptr = sp;
}

static uint64_t pop(uint64_t **sp_ptr)
{
    uint64_t *sp = *sp_ptr;
    uint64_t value = *sp;
    *sp_ptr = sp + 1;
    return value;
}

static uint64_t frame_num_args_local(uint64_t *fp)
{
    return (fp[FRAME_FLAGS] >> FRAME_FLAGS_NUM_ARGS_SHIFT) & 0xFF;
}

static uint64_t frame_arg_local(uint64_t *fp, uint64_t index)
{
    uint64_t num_args = frame_num_args_local(fp);
    return fp[FP_ARG_BASE_WORDS + (num_args - 1 - index)];
}

static void unsupported_bytecode(uint8_t opcode)
{
    (void)opcode;
    raise(SIGTRAP);
}

uint64_t interpret(uint64_t **sp_ptr, uint64_t **fp_ptr, uint8_t *ip,
                   uint64_t *class_table, uint64_t *om, uint64_t *txn_log)
{
    (void)om;
    (void)txn_log;

    uint8_t *bytecode_base = ip;
    uint64_t *entry_fp = *fp_ptr;

    for (;;)
    {
        uint8_t opcode = *ip++;

        switch (opcode)
        {
        case BC_PUSH_LITERAL:
        {
            uint32_t literal_index = read_u32(&ip);
            uint64_t *method = (uint64_t *)(*fp_ptr)[FRAME_METHOD];
            uint64_t *literals = (uint64_t *)OBJ_FIELD(method, CM_LITERALS);
            push(sp_ptr, OBJ_FIELD(literals, literal_index));
            break;
        }

        case BC_PUSH_INST_VAR:
        {
            uint32_t field_index = read_u32(&ip);
            uint64_t *receiver = (uint64_t *)(*fp_ptr)[FRAME_RECEIVER];
            push(sp_ptr, OBJ_FIELD(receiver, field_index));
            break;
        }

        case BC_PUSH_TEMP:
        {
            uint32_t temp_index = read_u32(&ip);
            push(sp_ptr, (*fp_ptr)[-(int64_t)(FP_TEMP_BASE_WORDS + temp_index)]);
            break;
        }

        case BC_PUSH_SELF:
            push(sp_ptr, (*fp_ptr)[FRAME_RECEIVER]);
            break;

        case BC_STORE_INST_VAR:
        {
            uint32_t field_index = read_u32(&ip);
            uint64_t value = pop(sp_ptr);
            uint64_t *receiver = (uint64_t *)(*fp_ptr)[FRAME_RECEIVER];
            OBJ_FIELD(receiver, field_index) = value;
            break;
        }

        case BC_STORE_TEMP:
        {
            uint32_t temp_index = read_u32(&ip);
            (*fp_ptr)[-(int64_t)(FP_TEMP_BASE_WORDS + temp_index)] = pop(sp_ptr);
            break;
        }

        case BC_SEND_MESSAGE:
        {
            uint32_t selector_index = read_u32(&ip);
            uint32_t arg_count = read_u32(&ip);
            uint64_t *current_method = (uint64_t *)(*fp_ptr)[FRAME_METHOD];
            uint64_t *literals = (uint64_t *)OBJ_FIELD(current_method, CM_LITERALS);
            uint64_t selector = OBJ_FIELD(literals, selector_index);
            uint64_t receiver = (*sp_ptr)[arg_count];
            uint64_t klass = oop_class(receiver, class_table);
            uint64_t method = class_lookup((uint64_t *)klass, selector);

            if (method == 0 || OBJ_FIELD((uint64_t *)method, CM_PRIMITIVE) != tag_smallint(PRIM_NONE))
            {
                unsupported_bytecode(opcode);
                break;
            }

            uint64_t num_temps = (uint64_t)untag_smallint(OBJ_FIELD((uint64_t *)method, CM_NUM_TEMPS));
            activate_method(sp_ptr, fp_ptr, (uint64_t)ip, method, arg_count, num_temps);

            uint64_t *new_method = (uint64_t *)(*fp_ptr)[FRAME_METHOD];
            uint64_t *bytecodes = (uint64_t *)OBJ_FIELD(new_method, CM_BYTECODES);
            bytecode_base = (uint8_t *)&OBJ_FIELD(bytecodes, 0);
            ip = bytecode_base;
            break;
        }

        case BC_RETURN_STACK_TOP:
        {
            uint64_t return_value = **sp_ptr;
            uint64_t *fp = *fp_ptr;
            uint64_t num_args = frame_num_args_local(fp);
            uint64_t *new_sp = fp + FP_ARG_BASE_WORDS + num_args;
            uint64_t *caller_fp = (uint64_t *)fp[FRAME_SAVED_FP];
            uint8_t *caller_ip = (uint8_t *)fp[FRAME_SAVED_IP];

            *new_sp = return_value;
            *sp_ptr = new_sp;
            *fp_ptr = caller_fp;

            if (fp == entry_fp)
            {
                return return_value;
            }

            ip = caller_ip;
            if (caller_fp != 0 && caller_fp != (uint64_t *)0xCAFE)
            {
                uint64_t *caller_method = (uint64_t *)caller_fp[FRAME_METHOD];
                uint64_t *bytecodes = (uint64_t *)OBJ_FIELD(caller_method, CM_BYTECODES);
                bytecode_base = (uint8_t *)&OBJ_FIELD(bytecodes, 0);
            }
            break;
        }

        case BC_JUMP:
        {
            uint32_t offset = read_u32(&ip);
            ip = bytecode_base + offset;
            break;
        }

        case BC_JUMP_IF_TRUE:
        {
            uint32_t offset = read_u32(&ip);
            uint64_t condition = pop(sp_ptr);
            if (condition == TAGGED_TRUE)
            {
                ip = bytecode_base + offset;
            }
            break;
        }

        case BC_JUMP_IF_FALSE:
        {
            uint32_t offset = read_u32(&ip);
            uint64_t condition = pop(sp_ptr);
            if (condition == TAGGED_FALSE)
            {
                ip = bytecode_base + offset;
            }
            break;
        }

        case BC_POP:
            (void)pop(sp_ptr);
            break;

        case BC_DUPLICATE:
            push(sp_ptr, **sp_ptr);
            break;

        case BC_HALT:
            return **sp_ptr;

        case BC_PUSH_CLOSURE:
            unsupported_bytecode(opcode);
            break;

        case BC_PUSH_ARG:
        {
            uint32_t arg_index = read_u32(&ip);
            push(sp_ptr, frame_arg_local(*fp_ptr, arg_index));
            break;
        }

        case BC_RETURN_NON_LOCAL:
        case BC_PUSH_THIS_CONTEXT:
        case BC_PUSH_GLOBAL:
            unsupported_bytecode(opcode);
            break;

        default:
            unsupported_bytecode(opcode);
            break;
        }
    }
}
