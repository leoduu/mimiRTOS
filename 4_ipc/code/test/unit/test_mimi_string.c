#include <limits.h>
#include <string.h>
#include <unity.h>

#include "mimi_string.h"

static char buf[64];

static void test_string_literal_and_percent(void)
{
    TEST_ASSERT_EQUAL_INT(7, mimi_snprintf(buf, sizeof(buf), "hello %%"));
    TEST_ASSERT_EQUAL_STRING("hello %", buf);
}

static void test_string_signed(void)
{
    mimi_snprintf(buf, sizeof(buf), "%d|%d|%d", 0, 42, -42);
    TEST_ASSERT_EQUAL_STRING("0|42|-42", buf);

    mimi_snprintf(buf, sizeof(buf), "%d", INT_MIN);
    TEST_ASSERT_EQUAL_STRING("-2147483648", buf);
}

static void test_string_unsigned_hex(void)
{
    mimi_snprintf(buf, sizeof(buf), "%u %x %X", 42u, 0xabcdefu, 0xabcdefu);
    TEST_ASSERT_EQUAL_STRING("42 abcdef ABCDEF", buf);

    mimi_snprintf(buf, sizeof(buf), "%X", 0x1234u);
    TEST_ASSERT_EQUAL_STRING("1234", buf);
}

/* Width / flags / precision are not implemented: the whole spec is copied
 * through literally instead of silently consuming the argument. */
static void test_string_width_unsupported(void)
{
    mimi_snprintf(buf, sizeof(buf), "%5d|%-5s|%08X|%+d",
                  42, "ab", 0x1234u, 42);
    TEST_ASSERT_EQUAL_STRING("%5d|%-5s|%08X|%+d", buf);
}

static void test_string_char_and_null(void)
{
    mimi_snprintf(buf, sizeof(buf), "%c %s", 'Z', (const char *)NULL);
    TEST_ASSERT_EQUAL_STRING("Z (null)", buf);
}

static void test_string_long(void)
{
    mimi_snprintf(buf, sizeof(buf), "%ld %lu %lx", -7L, 7UL, 0xabul);
    TEST_ASSERT_EQUAL_STRING("-7 7 ab", buf);
}

static void test_string_pointer(void)
{
    mimi_snprintf(buf, sizeof(buf), "%p", (void *)0x1234);
    TEST_ASSERT_EQUAL_STRING("0x1234", buf);
}

static void test_string_truncation(void)
{
    char small[5];
    int n = mimi_snprintf(small, sizeof(small), "abcdefgh");

    TEST_ASSERT_EQUAL_INT(8, n);
    TEST_ASSERT_EQUAL_STRING("abcd", small);

    /* size 0 must not write and still reports the length */
    TEST_ASSERT_EQUAL_INT(3, mimi_snprintf(NULL, 0, "abc"));
}

static void test_string_edge_cases(void)
{
    TEST_ASSERT_EQUAL_INT(0, mimi_snprintf(buf, sizeof(buf), ""));
    TEST_ASSERT_EQUAL_STRING("", buf);

    mimi_snprintf(buf, sizeof(buf), "abc%");
    TEST_ASSERT_EQUAL_STRING("abc%", buf);

    mimi_snprintf(buf, sizeof(buf), "%q");
    TEST_ASSERT_EQUAL_STRING("%q", buf);
}

int run_string_tests(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_string_literal_and_percent);
    RUN_TEST(test_string_signed);
    RUN_TEST(test_string_unsigned_hex);
    RUN_TEST(test_string_width_unsupported);
    RUN_TEST(test_string_char_and_null);
    RUN_TEST(test_string_long);
    RUN_TEST(test_string_pointer);
    RUN_TEST(test_string_truncation);
    RUN_TEST(test_string_edge_cases);

    return UNITY_END();
}
