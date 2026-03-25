#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

// ARM64 assembly functions
extern void stack_push(uint64_t **sp_ptr, uint64_t *stack_base, uint64_t value);
extern uint64_t stack_top(uint64_t **sp_ptr);

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

    printf("\n%d passed, %d failed\n", passes, failures);
    return failures > 0 ? 1 : 0;
}
