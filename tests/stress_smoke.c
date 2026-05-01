#include "test_defs.h"
#include "smalltalk_world.h"

#include <unistd.h>

static const char *STRESS_CHECKPOINT_PATH = "/tmp/arlo_stress_smoke.image";

static void print_smalltalk_suite_progress(const char *class_name, int pass_count, int failure_count)
{
    int index;
    printf("[stress] %s ", class_name);
    for (index = 0; index < pass_count; index++)
    {
        putchar('.');
    }
    for (index = 0; index < failure_count; index++)
    {
        putchar('F');
    }
    printf(" (%d passed, %d failed)\n", pass_count, failure_count);
    fflush(stdout);
}

static int copy_symbol_to_cstring(Oop symbol_oop, char *buffer, size_t capacity)
{
    ObjPtr symbol;
    uint64_t size;

    if (!is_object_ptr(symbol_oop) || capacity == 0)
    {
        return 0;
    }

    symbol = (ObjPtr)symbol_oop;
    size = OBJ_SIZE(symbol);
    if (size + 1 > capacity)
    {
        return 0;
    }

    memcpy(buffer, &OBJ_FIELD(symbol, 0), (size_t)size);
    buffer[size] = '\0';
    return 1;
}

static void run_smalltalk_selectors(TestContext *ctx, SmalltalkWorld *world,
                                    const char *class_name, int expected_tests)
{
    uint64_t *test_class = smalltalk_world_lookup_class(world, class_name);
    Oop selectors;
    Oop selector_count;
    int pass_count = 0;

    ASSERT_EQ(ctx, test_class != NULL, 1, "stress: test class available");
    selectors = sw_send0(world, ctx, (Oop)test_class, world->class_class, "testSelectors");
    ASSERT_EQ(ctx, is_object_ptr(selectors), 1, "stress: selectors array exists");
    selector_count = sw_send0(world, ctx, selectors, NULL, "size");
    ASSERT_EQ(ctx, selector_count, tag_smallint(expected_tests),
              "stress: selector count matches");

    for (int index = 1; index <= expected_tests; index++)
    {
        Oop selector = sw_send1(world, ctx, selectors, NULL, "at:", tag_smallint(index));
        Oop test_case = sw_send0(world, ctx, (Oop)test_class, world->class_class, "new");
        char selector_name[128];

        ASSERT_EQ(ctx, is_object_ptr(selector), 1, "stress: selector exists");
        ASSERT_EQ(ctx, is_object_ptr(test_case), 1, "stress: test instance exists");
        ASSERT_EQ(ctx, copy_symbol_to_cstring(selector, selector_name, sizeof(selector_name)), 1,
                  "stress: selector name copied");
        printf("[stress] %s>>%s\n", class_name, selector_name);
        (void)sw_send0(world, ctx, test_case, NULL, selector_name);
        pass_count++;
    }

    print_smalltalk_suite_progress(class_name, pass_count, 0);
}

static void install_stress_world(TestContext *ctx, SmalltalkWorld *world)
{
    ASSERT_EQ(ctx, smalltalk_world_install_existing_class_file(world, "src/smalltalk/Object.st") != NULL,
              1, "stress: Object.st installs");
    ASSERT_EQ(ctx, smalltalk_world_install_existing_class_file(world, "src/smalltalk/UndefinedObject.st") != NULL,
              1, "stress: UndefinedObject.st installs");
    ASSERT_EQ(ctx, smalltalk_world_install_existing_class_file(world, "src/smalltalk/SmallInteger.st") != NULL,
              1, "stress: SmallInteger.st installs");
    ASSERT_EQ(ctx, smalltalk_world_install_st_file(world, "src/smalltalk/Class.st"), 1,
              "stress: Class.st installs");
    ASSERT_EQ(ctx, smalltalk_world_install_existing_class_file(world, "src/smalltalk/Array.st") != NULL,
              1, "stress: Array.st installs");
    ASSERT_EQ(ctx, smalltalk_world_install_existing_class_file(world, "src/smalltalk/String.st") != NULL,
              1, "stress: String.st installs");
    ASSERT_EQ(ctx, smalltalk_world_install_st_file(world, "src/smalltalk/Association.st"), 1,
              "stress: Association.st installs");
    ASSERT_EQ(ctx, smalltalk_world_install_st_file(world, "src/smalltalk/Dictionary.st"), 1,
              "stress: Dictionary.st installs");
    ASSERT_EQ(ctx, smalltalk_world_install_st_file(world, "src/smalltalk/Context.st"), 1,
              "stress: Context.st installs");
    ASSERT_EQ(ctx, smalltalk_world_install_existing_class_file(world, "src/smalltalk/True.st") != NULL,
              1, "stress: True.st installs");
    ASSERT_EQ(ctx, smalltalk_world_install_existing_class_file(world, "src/smalltalk/False.st") != NULL,
              1, "stress: False.st installs");
    ASSERT_EQ(ctx, smalltalk_world_install_st_file(world, "src/smalltalk/BlockClosure.st"), 1,
              "stress: BlockClosure.st installs");

    ASSERT_EQ(ctx, smalltalk_world_install_class_file(world, "src/smalltalk/TestResult.st") != NULL,
              1, "stress: TestResult.st installs");
    ASSERT_EQ(ctx, smalltalk_world_install_class_file(world, "src/smalltalk/Exception.st") != NULL,
              1, "stress: Exception.st installs");
    ASSERT_EQ(ctx, smalltalk_world_install_class_file(world, "src/smalltalk/Error.st") != NULL,
              1, "stress: Error.st installs");
    ASSERT_EQ(ctx, smalltalk_world_install_class_file(world, "src/smalltalk/TestFailure.st") != NULL,
              1, "stress: TestFailure.st installs");
    ASSERT_EQ(ctx, smalltalk_world_install_class_file(world, "src/smalltalk/TestCase.st") != NULL,
              1, "stress: TestCase.st installs");
    ASSERT_EQ(ctx, smalltalk_world_install_class_file(world, "src/smalltalk/TestSuite.st") != NULL,
              1, "stress: TestSuite.st installs");
    ASSERT_EQ(ctx, smalltalk_world_install_class_file(world, "src/smalltalk/Transaction.st") != NULL,
              1, "stress: Transaction.st installs");
    ASSERT_EQ(ctx, smalltalk_world_install_class_file(world, "src/smalltalk/Image.st") != NULL,
              1, "stress: Image.st installs");

    ASSERT_EQ(ctx, smalltalk_world_install_class_file(world, "tests/fixtures/StressSmokeTest.st") != NULL,
              1, "stress: StressSmokeTest.st installs");
}

int main(void)
{
    static uint8_t world_buf[128 * 1024 * 1024] __attribute__((aligned(8)));
    TestContext ctx;
    SmalltalkWorld world;

    memset(&ctx, 0, sizeof(ctx));
    setbuf(stdout, NULL);

    txn_durable_log_clear();
    unlink(STRESS_CHECKPOINT_PATH);
    unlink("/tmp/arlo_stress_smoke.image.tmp");

    printf("[stress] init world\n");
    smalltalk_world_init(&world, world_buf, sizeof(world_buf));
    printf("[stress] install world\n");
    install_stress_world(&ctx, &world);

    printf("[stress] run suite\n");
    run_smalltalk_selectors(&ctx, &world, "StressSmokeTest", 7);

    printf("[stress] teardown\n");
    smalltalk_world_teardown(&world);
    txn_durable_log_clear();
    unlink(STRESS_CHECKPOINT_PATH);
    unlink("/tmp/arlo_stress_smoke.image.tmp");

    printf("\n%d passed, %d failed\n", ctx.passes, ctx.failures);
    return ctx.failures > 0 ? 1 : 0;
}
