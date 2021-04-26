#include <unity.h>

#include "../oha_utils.h"

/* Is run before every test, put unit init calls here. */
void
setUp(void)
{
}
/* Is run after every test, put unit clean-up calls here. */
void
tearDown(void)
{
}

void
test_log2(void)
{
    // power of 2 input
    TEST_ASSERT_EQUAL(0, oha_log2_32bit(1));
    TEST_ASSERT_EQUAL(1, oha_log2_32bit(2));
    TEST_ASSERT_EQUAL(2, oha_log2_32bit(4));
    TEST_ASSERT_EQUAL(3, oha_log2_32bit(8));
    TEST_ASSERT_EQUAL(4, oha_log2_32bit(16));
    TEST_ASSERT_EQUAL(5, oha_log2_32bit(32));
    TEST_ASSERT_EQUAL(6, oha_log2_32bit(64));
    TEST_ASSERT_EQUAL(7, oha_log2_32bit(128));
    TEST_ASSERT_EQUAL(8, oha_log2_32bit(256));
    TEST_ASSERT_EQUAL(9, oha_log2_32bit(512));
    TEST_ASSERT_EQUAL(10, oha_log2_32bit(1024));
    TEST_ASSERT_EQUAL(11, oha_log2_32bit(2048));
    TEST_ASSERT_EQUAL(12, oha_log2_32bit(4096));
    TEST_ASSERT_EQUAL(13, oha_log2_32bit(8192));
    TEST_ASSERT_EQUAL(14, oha_log2_32bit(16384));
    TEST_ASSERT_EQUAL(15, oha_log2_32bit(32768));
    TEST_ASSERT_EQUAL(16, oha_log2_32bit(65536));
    TEST_ASSERT_EQUAL(17, oha_log2_32bit(131072));
    TEST_ASSERT_EQUAL(18, oha_log2_32bit(262144));
    TEST_ASSERT_EQUAL(19, oha_log2_32bit(524288));
    TEST_ASSERT_EQUAL(20, oha_log2_32bit(1048576));
    TEST_ASSERT_EQUAL(21, oha_log2_32bit(2097152));
    TEST_ASSERT_EQUAL(22, oha_log2_32bit(4194304));
    TEST_ASSERT_EQUAL(23, oha_log2_32bit(8388608));
    TEST_ASSERT_EQUAL(24, oha_log2_32bit(16777216));

    // power of 2 input + 1
    TEST_ASSERT_EQUAL(1, oha_log2_32bit(2 + 1));
    TEST_ASSERT_EQUAL(2, oha_log2_32bit(4 + 1));
    TEST_ASSERT_EQUAL(3, oha_log2_32bit(8 + 1));
    TEST_ASSERT_EQUAL(4, oha_log2_32bit(16 + 1));
    TEST_ASSERT_EQUAL(5, oha_log2_32bit(32 + 1));
    TEST_ASSERT_EQUAL(6, oha_log2_32bit(64 + 1));
    TEST_ASSERT_EQUAL(7, oha_log2_32bit(128 + 1));
    TEST_ASSERT_EQUAL(8, oha_log2_32bit(256 + 1));
    TEST_ASSERT_EQUAL(9, oha_log2_32bit(512 + 1));
    TEST_ASSERT_EQUAL(10, oha_log2_32bit(1024 + 1));
    TEST_ASSERT_EQUAL(11, oha_log2_32bit(2048 + 1));
    TEST_ASSERT_EQUAL(12, oha_log2_32bit(4096 + 1));
    TEST_ASSERT_EQUAL(13, oha_log2_32bit(8192 + 1));
    TEST_ASSERT_EQUAL(14, oha_log2_32bit(16384 + 1));
    TEST_ASSERT_EQUAL(15, oha_log2_32bit(32768 + 1));
    TEST_ASSERT_EQUAL(16, oha_log2_32bit(65536 + 1));
    TEST_ASSERT_EQUAL(17, oha_log2_32bit(131072 + 1));
    TEST_ASSERT_EQUAL(18, oha_log2_32bit(262144 + 1));
    TEST_ASSERT_EQUAL(19, oha_log2_32bit(524288 + 1));
    TEST_ASSERT_EQUAL(20, oha_log2_32bit(1048576 + 1));
    TEST_ASSERT_EQUAL(21, oha_log2_32bit(2097152 + 1));
    TEST_ASSERT_EQUAL(22, oha_log2_32bit(4194304 + 1));
    TEST_ASSERT_EQUAL(23, oha_log2_32bit(8388608 + 1));
    TEST_ASSERT_EQUAL(24, oha_log2_32bit(16777216 + 1));

    // power of 2 input - 1
    TEST_ASSERT_EQUAL(0, oha_log2_32bit(2 - 1));
    TEST_ASSERT_EQUAL(1, oha_log2_32bit(4 - 1));
    TEST_ASSERT_EQUAL(2, oha_log2_32bit(8 - 1));
    TEST_ASSERT_EQUAL(3, oha_log2_32bit(16 - 1));
    TEST_ASSERT_EQUAL(4, oha_log2_32bit(32 - 1));
    TEST_ASSERT_EQUAL(5, oha_log2_32bit(64 - 1));
    TEST_ASSERT_EQUAL(6, oha_log2_32bit(128 - 1));
    TEST_ASSERT_EQUAL(7, oha_log2_32bit(256 - 1));
    TEST_ASSERT_EQUAL(8, oha_log2_32bit(512 - 1));
    TEST_ASSERT_EQUAL(9, oha_log2_32bit(1024 - 1));
    TEST_ASSERT_EQUAL(10, oha_log2_32bit(2048 - 1));
    TEST_ASSERT_EQUAL(11, oha_log2_32bit(4096 - 1));
    TEST_ASSERT_EQUAL(12, oha_log2_32bit(8192 - 1));
    TEST_ASSERT_EQUAL(13, oha_log2_32bit(16384 - 1));
    TEST_ASSERT_EQUAL(14, oha_log2_32bit(32768 - 1));
    TEST_ASSERT_EQUAL(15, oha_log2_32bit(65536 - 1));
    TEST_ASSERT_EQUAL(16, oha_log2_32bit(131072 - 1));
    TEST_ASSERT_EQUAL(17, oha_log2_32bit(262144 - 1));
    TEST_ASSERT_EQUAL(18, oha_log2_32bit(524288 - 1));
    TEST_ASSERT_EQUAL(19, oha_log2_32bit(1048576 - 1));
    TEST_ASSERT_EQUAL(20, oha_log2_32bit(2097152 - 1));
    TEST_ASSERT_EQUAL(21, oha_log2_32bit(4194304 - 1));
    TEST_ASSERT_EQUAL(22, oha_log2_32bit(8388608 - 1));
    TEST_ASSERT_EQUAL(23, oha_log2_32bit(16777216 - 1));
}

void
test_next_pow_of_2_32bit(void)
{
    // power of 2 input
    TEST_ASSERT_EQUAL(oha_next_power_of_two_32bit(0), 1);
    TEST_ASSERT_EQUAL(oha_next_power_of_two_32bit(1), 2);
    TEST_ASSERT_EQUAL(oha_next_power_of_two_32bit(2), 4);
    TEST_ASSERT_EQUAL(oha_next_power_of_two_32bit(3), 4);
    TEST_ASSERT_EQUAL(oha_next_power_of_two_32bit(4), 8);
    TEST_ASSERT_EQUAL(oha_next_power_of_two_32bit(5), 8);
    TEST_ASSERT_EQUAL(oha_next_power_of_two_32bit(6), 8);
    TEST_ASSERT_EQUAL(oha_next_power_of_two_32bit(7), 8);
    TEST_ASSERT_EQUAL(oha_next_power_of_two_32bit(8), 16);
    TEST_ASSERT_EQUAL(oha_next_power_of_two_32bit(9), 16);
    TEST_ASSERT_EQUAL(oha_next_power_of_two_32bit(15), 16);
    TEST_ASSERT_EQUAL(oha_next_power_of_two_32bit(16), 32);
    TEST_ASSERT_EQUAL(oha_next_power_of_two_32bit(17), 32);
    TEST_ASSERT_EQUAL(oha_next_power_of_two_32bit(31), 32);
    TEST_ASSERT_EQUAL(oha_next_power_of_two_32bit(32), 64);
    TEST_ASSERT_EQUAL(oha_next_power_of_two_32bit(33), 64);
    TEST_ASSERT_EQUAL(oha_next_power_of_two_32bit(63), 64);
    TEST_ASSERT_EQUAL(oha_next_power_of_two_32bit(64), 128);
    TEST_ASSERT_EQUAL(oha_next_power_of_two_32bit(65), 128);
    TEST_ASSERT_EQUAL(oha_next_power_of_two_32bit(127), 128);
    TEST_ASSERT_EQUAL(oha_next_power_of_two_32bit(128), 256);
    TEST_ASSERT_EQUAL(oha_next_power_of_two_32bit(255), 256);
    TEST_ASSERT_EQUAL(oha_next_power_of_two_32bit(256), 512);
    TEST_ASSERT_EQUAL(oha_next_power_of_two_32bit(257), 512);
    TEST_ASSERT_EQUAL(oha_next_power_of_two_32bit(512), 1024);
}

int
main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_log2);

    return UNITY_END();
}
