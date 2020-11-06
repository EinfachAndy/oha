#include <stdlib.h>
#include <string.h>
#include <unity.h>

#include "oha.h"

// good for testing to create collisions
#define LOAF_FACTOR 0.9

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
    struct oha_lpht_config config;
    memset(&config, 0, sizeof(config));
    config.load_factor = LOAF_FACTOR;
    config.key_size = sizeof(uint64_t);
    config.value_size = sizeof(uint64_t);
    config.max_elems = 100;

    struct oha_lpht * table = oha_lpht_create(&config);
    oha_lpht_destroy(table);
}

void test_insert_look_up()
{
    struct oha_lpht_config config;
    memset(&config, 0, sizeof(config));
    config.load_factor = LOAF_FACTOR;
    config.key_size = sizeof(uint64_t);
    config.value_size = sizeof(uint64_t);
    config.max_elems = 100;

    struct oha_lpht * table = oha_lpht_create(&config);

    for (uint64_t i = 0; i < config.max_elems; i++) {
        for (uint64_t j = 0; j < i; j++) {
            uint64_t * value_lool_up = oha_lpht_look_up(table, &j);
            TEST_ASSERT_NOT_NULL(value_lool_up);
            TEST_ASSERT_EQUAL_UINT64(*value_lool_up, j);
        }
        for (uint64_t j = i; j < config.max_elems; j++) {
            uint64_t * value_lool_up = oha_lpht_look_up(table, &j);
            TEST_ASSERT_NULL(value_lool_up);
        }

        struct oha_lpht_status status;
        TEST_ASSERT_TRUE(oha_lpht_get_status(table, &status));
        TEST_ASSERT_EQUAL_UINT64(i, status.elems_in_use);
        TEST_ASSERT_EQUAL_UINT64(config.max_elems, status.max_elems);

        uint64_t * value_insert = oha_lpht_insert(table, &i);
        TEST_ASSERT_NOT_NULL(value_insert);
        *value_insert = i;
        uint64_t * value_lool_up = oha_lpht_look_up(table, &i);
        TEST_ASSERT_EQUAL_PTR(value_insert, value_lool_up);
        TEST_ASSERT_EQUAL_UINT64(*value_lool_up, i);

        TEST_ASSERT_TRUE(oha_lpht_get_status(table, &status));
        TEST_ASSERT_EQUAL_UINT64(i + 1, status.elems_in_use);
        TEST_ASSERT_EQUAL_UINT64(config.max_elems, status.max_elems);
    }

    // Table is full
    uint64_t full = config.max_elems + 1;
    TEST_ASSERT_NULL(oha_lpht_insert(table, &full));

    for (uint64_t i = 0; i < config.max_elems; i++) {
        uint64_t * value_lool_up = oha_lpht_look_up(table, &i);
        TEST_ASSERT_EQUAL_UINT64(*value_lool_up, i);
    }

    oha_lpht_destroy(table);
}

void test_insert_look_up_remove()
{
    for (size_t elems = 1; elems < 500; elems++) {

        struct oha_lpht_config config;
        memset(&config, 0, sizeof(config));
        config.load_factor = LOAF_FACTOR;
        config.key_size = sizeof(uint64_t);
        config.value_size = sizeof(uint64_t);
        config.max_elems = elems;

        struct oha_lpht * table = oha_lpht_create(&config);

        for (uint64_t i = 0; i < elems; i++) {
            uint64_t * value_insert = oha_lpht_insert(table, &i);
            TEST_ASSERT_NOT_NULL(value_insert);
            *value_insert = i;
        }

        TEST_ASSERT_NULL(oha_lpht_insert(table, &elems));

        for (uint64_t i = 0; i < elems; i++) {
            for (uint64_t j = 0; j < i; j++) {
                uint64_t * value_lool_up = oha_lpht_look_up(table, &j);
                TEST_ASSERT_NULL(value_lool_up);
            }
            for (uint64_t j = i; j < elems; j++) {
                uint64_t * value_lool_up = oha_lpht_look_up(table, &j);
                TEST_ASSERT_NOT_NULL(value_lool_up);
            }
            uint64_t * removed_value = oha_lpht_remove(table, &i);
            TEST_ASSERT_NOT_NULL(removed_value);
            TEST_ASSERT_EQUAL_UINT64(*removed_value, i);
            *removed_value = (uint64_t)-1;
            TEST_ASSERT_NULL(oha_lpht_look_up(table, &i));
            TEST_ASSERT_NULL(oha_lpht_remove(table, &i));
        }

        oha_lpht_destroy(table);
    }
}

void test_insert_look_up_resize()
{
    const size_t iterations = 500;
    for (size_t elems = 1; elems < iterations; elems++) {

        struct oha_lpht_config config;
        memset(&config, 0, sizeof(config));
        config.load_factor = LOAF_FACTOR;
        config.key_size = sizeof(uint64_t);
        config.value_size = sizeof(uint64_t);
        config.max_elems = elems;
        config.resizable = true;

        struct oha_lpht * table = oha_lpht_create(&config);
        TEST_ASSERT_NOT_NULL(table);

        for (uint64_t i = 0; i < iterations; i++) {
            uint64_t * value_insert = oha_lpht_insert(table, &i);
            TEST_ASSERT_NOT_NULL(value_insert);
            *value_insert = i;
        }

        for (uint64_t j = 0; j < iterations; j++) {
            uint64_t * value_lool_up = oha_lpht_look_up(table, &j);
            TEST_ASSERT_NOT_NULL(value_lool_up);
            TEST_ASSERT_EQUAL_UINT64(*value_lool_up, j);
        }

        oha_lpht_destroy(table);
    }
}

void test_clear_remove()
{
    struct oha_lpht_config config;
    memset(&config, 0, sizeof(config));
    config.load_factor = LOAF_FACTOR;
    config.key_size = sizeof(uint64_t);
    config.value_size = sizeof(uint64_t);
    config.max_elems = 100;

    struct oha_lpht * table = oha_lpht_create(&config);
    TEST_ASSERT_NOT_NULL(table);

    for (uint64_t i = 1; i <= config.max_elems; i++) {
        uint64_t * value_insert = oha_lpht_insert(table, &i);
        TEST_ASSERT_NOT_NULL(value_insert);
        *value_insert = i;
    }

    oha_lpht_clear(table);

    struct oha_key_value_pair pair;
    for (uint64_t i = 1; i <= config.max_elems; i++) {
        TEST_ASSERT(oha_lpht_get_next_element_to_remove(table, &pair));
        TEST_ASSERT_NOT_NULL(pair.key);
        TEST_ASSERT_NOT_NULL(pair.value);
        TEST_ASSERT(*(uint64_t *)pair.value > 0);
        TEST_ASSERT(*(uint64_t *)pair.value <= 100);
    }

    TEST_ASSERT(oha_lpht_get_next_element_to_remove(table, &pair) == false);

    oha_lpht_destroy(table);
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_create_destroy);
    RUN_TEST(test_insert_look_up_resize);
    RUN_TEST(test_insert_look_up);
    RUN_TEST(test_insert_look_up_remove);
    RUN_TEST(test_clear_remove);

    return UNITY_END();
}
