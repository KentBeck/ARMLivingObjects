#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

// ARM64 assembly functions
extern void stack_push(uint64_t **sp_ptr, uint64_t *stack_base, uint64_t value);
extern uint64_t stack_pop(uint64_t **sp_ptr);
extern uint64_t stack_top(uint64_t **sp_ptr);

// activate_method(sp_ptr, fp_ptr, saved_ip, method, num_args, num_temps)
// Builds a frame on the stack. Caller must have already pushed receiver (and args).
extern void activate_method(uint64_t **sp_ptr, uint64_t **fp_ptr,
                            uint64_t saved_ip, uint64_t method,
                            uint64_t num_args, uint64_t num_temps);

// frame_receiver(fp) -> uint64_t
// Read the receiver from a frame at FP - 4*W.
extern uint64_t frame_receiver(uint64_t *fp);

// Frame layout offsets from FP (in words, multiply by 8 for bytes)
#define FRAME_SAVED_IP 1  // FP + 1*W
#define FRAME_SAVED_FP 0  // FP + 0
#define FRAME_METHOD -1   // FP - 1*W
#define FRAME_FLAGS -2    // FP - 2*W
#define FRAME_CONTEXT -3  // FP - 3*W
#define FRAME_RECEIVER -4 // FP - 4*W
#define FRAME_TEMP0 -5    // FP - 5*W

#define STACK_WORDS 64
#define ASSERT_EQ(a, b, msg)                                             \
    do                                                                   \
    {                                                                    \
        uint64_t _a = (a), _b = (b);                                     \
        if (_a != _b)                                                    \
        {                                                                \
            printf("FAIL: %s (expected %llu, got %llu)\n", msg, _b, _a); \
            failures++;                                                  \
        }                                                                \
        else                                                             \
        {                                                                \
            printf("PASS: %s\n", msg);                                   \
            passes++;                                                    \
        }                                                                \
    } while (0)

int main()
{
    int passes = 0, failures = 0;

    // Allocate stack memory
    uint64_t stack[STACK_WORDS];
    // SP starts at top (one past end), grows down
    uint64_t *sp = (uint64_t *)((uint8_t *)stack + sizeof(stack));

    // Test: push a value onto a stack and read it back
    stack_push(&sp, stack, 42);
    ASSERT_EQ(stack_top(&sp), 42, "push a value and read it back");

    // Test: push two values and pop one, reading the remaining top
    stack_push(&sp, stack, 100);
    stack_push(&sp, stack, 200);
    stack_pop(&sp);
    ASSERT_EQ(stack_top(&sp), 100, "push two values and pop one");

    // --- Method Activation Tests ---
    // Reset stack for activation tests
    sp = (uint64_t *)((uint8_t *)stack + sizeof(stack));
    uint64_t *fp = 0;

    // Test: activate a method with 0 args, 0 temps
    uint64_t receiver = 0xBEEF;
    uint64_t fake_ip = 0x1000;
    uint64_t fake_method = 0x2000;

    stack_push(&sp, stack, receiver); // caller pushes receiver
    activate_method(&sp, &fp, fake_ip, fake_method, 0, 0);

    ASSERT_EQ(fp[FRAME_SAVED_IP], fake_ip, "activate 0/0: saved IP");
    ASSERT_EQ(fp[FRAME_SAVED_FP], 0, "activate 0/0: saved caller FP (was null)");
    ASSERT_EQ(fp[FRAME_METHOD], fake_method, "activate 0/0: method");
    ASSERT_EQ(fp[FRAME_FLAGS], 0, "activate 0/0: flags (all zero)");
    ASSERT_EQ(fp[FRAME_CONTEXT], 0, "activate 0/0: context slot (nil)");
    ASSERT_EQ(fp[FRAME_RECEIVER], receiver, "activate 0/0: receiver");
    ASSERT_EQ((uint64_t)sp, (uint64_t)&fp[FRAME_RECEIVER],
              "activate 0/0: SP points at receiver (last pushed)");

    // Test: read receiver from frame via ARM64 function
    ASSERT_EQ(frame_receiver(fp), receiver, "frame_receiver reads receiver at FP-4*W");

    // Test: activate with 0 temps (exercises cbz early-out)
    // Already tested above as activate 0/0

    // Test: activate with 1 temp (exercises odd path)
    sp = (uint64_t *)((uint8_t *)stack + sizeof(stack));
    fp = 0;
    stack_push(&sp, stack, receiver);
    activate_method(&sp, &fp, fake_ip, fake_method, 0, 1);
    ASSERT_EQ(fp[FRAME_RECEIVER], receiver, "activate 0/1: receiver");
    ASSERT_EQ(fp[FRAME_TEMP0], 0, "activate 0/1: temp 0 initialized to 0");
    ASSERT_EQ((uint64_t)sp, (uint64_t)&fp[FRAME_TEMP0],
              "activate 0/1: SP points at temp 0");

    // Test: activate with 2 temps (exercises pairs path, even count)
    sp = (uint64_t *)((uint8_t *)stack + sizeof(stack));
    fp = 0;
    stack_push(&sp, stack, receiver);
    activate_method(&sp, &fp, fake_ip, fake_method, 0, 2);
    ASSERT_EQ(fp[FRAME_TEMP0], 0, "activate 0/2: temp 0 initialized to 0");
    ASSERT_EQ(fp[FRAME_TEMP0 - 1], 0, "activate 0/2: temp 1 initialized to 0");
    ASSERT_EQ((uint64_t)sp, (uint64_t)&fp[FRAME_TEMP0 - 1],
              "activate 0/2: SP points at temp 1");

    // Test: activate with 3 temps (exercises odd + pairs path)
    sp = (uint64_t *)((uint8_t *)stack + sizeof(stack));
    fp = 0;
    stack_push(&sp, stack, receiver);
    activate_method(&sp, &fp, fake_ip, fake_method, 0, 3);
    ASSERT_EQ(fp[FRAME_TEMP0], 0, "activate 0/3: temp 0 initialized to 0");
    ASSERT_EQ(fp[FRAME_TEMP0 - 1], 0, "activate 0/3: temp 1 initialized to 0");
    ASSERT_EQ(fp[FRAME_TEMP0 - 2], 0, "activate 0/3: temp 2 initialized to 0");
    ASSERT_EQ((uint64_t)sp, (uint64_t)&fp[FRAME_TEMP0 - 2],
              "activate 0/3: SP points at temp 2");

    // Test: activate with 1 arg, 0 temps: arg accessible above frame
    sp = (uint64_t *)((uint8_t *)stack + sizeof(stack));
    fp = 0;
    uint64_t arg0 = 0xAAAA;
    stack_push(&sp, stack, receiver); // caller pushes receiver
    stack_push(&sp, stack, arg0);     // caller pushes arg 0
    activate_method(&sp, &fp, fake_ip, fake_method, 1, 0);
    ASSERT_EQ(fp[FRAME_RECEIVER], receiver, "activate 1/0: receiver");
    // arg 0 is at FP + 2*W (above saved IP)
    ASSERT_EQ(fp[2], arg0, "activate 1/0: arg 0 at FP+2*W");
    // flags should encode num_args=1 in byte 1
    ASSERT_EQ(fp[FRAME_FLAGS] & 0xFF00, 1 << 8, "activate 1/0: flags encode num_args=1");

    printf("\n%d passed, %d failed\n", passes, failures);
    return failures > 0 ? 1 : 0;
}
