#include "unity.h"

void setUp(void)
{
    // set stuff up here
}

void tearDown(void)
{
    // clean stuff up here
}

void test_first()
{
    TEST_ASSERT_TRUE(false);
}

int main()
{
    RUN_TEST(test_first);
    return UNITY_END();
}
