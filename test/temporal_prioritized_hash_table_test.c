#include <unity.h>

#include <string.h>

#include "oha.h"

// good for testing to create collisions
#define LOAF_FACTOR 0.7

/* Is run before every test, put unit init calls here. */
void setUp(void)
{
}
/* Is run after every test, put unit clean-up calls here. */
void tearDown(void)
{
}

void test_create_destroy()
{
    struct oha_lpht_config config_lpht;
    memset(&config_lpht, 0, sizeof(config_lpht));
    config_lpht.load_factor = LOAF_FACTOR;
    config_lpht.key_size = sizeof(uint64_t);
    config_lpht.value_size = sizeof(uint64_t);
    config_lpht.max_elems = 100;
    struct oha_tpht_config config;
    config.lpht_config = config_lpht;

    struct oha_tpht * table = oha_tpht_create(&config);
    TEST_ASSERT_NOT_NULL(table);
    oha_tpht_destroy(table);
}

void test_create_destroy_add_slot()
{
    struct oha_lpht_config config_lpht;
    memset(&config_lpht, 0, sizeof(config_lpht));
    config_lpht.load_factor = LOAF_FACTOR;
    config_lpht.key_size = sizeof(uint64_t);
    config_lpht.value_size = sizeof(uint64_t);
    config_lpht.max_elems = 100;
    struct oha_tpht_config config;
    config.lpht_config = config_lpht;

    struct oha_tpht * table = oha_tpht_create(&config);
    TEST_ASSERT_NOT_NULL(table);

    for (int8_t i = 1; i < SCHAR_MAX; i++) {
        TEST_ASSERT_EQUAL(i, oha_tpht_add_timeout_slot(table, i, 100, false));
    }

    oha_tpht_destroy(table);
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_create_destroy);
    RUN_TEST(test_create_destroy_add_slot);

    return UNITY_END();
}
