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

void test_set_timeout_slot()
{
    const size_t num_timeouts = 100;
    struct oha_key_value_pair next_pairs[num_timeouts];

    struct oha_lpht_config config_lpht;
    memset(&config_lpht, 0, sizeof(config_lpht));
    config_lpht.load_factor = LOAF_FACTOR;
    config_lpht.key_size = sizeof(uint64_t);
    config_lpht.value_size = sizeof(uint64_t);
    config_lpht.max_elems = num_timeouts;
    struct oha_tpht_config config;
    config.lpht_config = config_lpht;

    struct oha_tpht * table = oha_tpht_create(&config);
    TEST_ASSERT_NOT_NULL(table);

    const uint8_t first_slot = 1;
    const uint8_t second_slot = 2;

    TEST_ASSERT_EQUAL(first_slot, oha_tpht_add_timeout_slot(table, 50, 100, false));
    TEST_ASSERT_EQUAL(second_slot, oha_tpht_add_timeout_slot(table, 200, 100, false));

    TEST_ASSERT_TRUE(oha_tpht_increase_global_time(table, 1000));
    for (uint64_t i = 0; i < config_lpht.max_elems; i++) {
        uint64_t * value_insert = oha_tpht_insert(table, &i, 0);
        *value_insert = i;
    }
    TEST_ASSERT_TRUE(oha_tpht_increase_global_time(table, 2000));
    TEST_ASSERT_EQUAL(0, oha_tpht_next_timeout_entries(table, next_pairs, num_timeouts));

    uint64_t key = 5;
    uint64_t * value = oha_tpht_set_timeout_slot(table, &key, first_slot);
    TEST_ASSERT_EQUAL(5, *value);
    TEST_ASSERT_EQUAL(0, oha_tpht_next_timeout_entries(table, next_pairs, num_timeouts));

    // move 10 - 29 element into the first timeout queue
    for (uint64_t i = 10; i < 30; i++) {
        TEST_ASSERT_TRUE(oha_tpht_increase_global_time(table, 2000 + i));
        value = oha_tpht_set_timeout_slot(table, &i, first_slot);
        TEST_ASSERT_EQUAL(i, *value);
        TEST_ASSERT_EQUAL(0, oha_tpht_next_timeout_entries(table, next_pairs, num_timeouts));
    }

    // timeout element 5
    TEST_ASSERT_TRUE(oha_tpht_increase_global_time(table, 2000 + 50));
    TEST_ASSERT_EQUAL(1, oha_tpht_next_timeout_entries(table, next_pairs, num_timeouts));
    TEST_ASSERT_EQUAL(5, *(uint64_t *)next_pairs[0].value);

    TEST_ASSERT_TRUE(oha_tpht_increase_global_time(table, 2000 + 50 + 9));
    TEST_ASSERT_EQUAL(0, oha_tpht_next_timeout_entries(table, next_pairs, num_timeouts));

    // timeout 5 elements from first queue
    TEST_ASSERT_TRUE(oha_tpht_increase_global_time(table, 2000 + 50 + 10 + 4));
    TEST_ASSERT_EQUAL(5, oha_tpht_next_timeout_entries(table, next_pairs, num_timeouts));
    for (uint64_t i = 0; i < 5; i++) {
        TEST_ASSERT_EQUAL(i + 10, *(uint64_t *)next_pairs[i].value);
    }

    // move next 10 elements into second queue
    for (uint64_t i = 15; i < 25; i++) {
        value = oha_tpht_set_timeout_slot(table, &i, second_slot);
        TEST_ASSERT_NOT_NULL(value);
        TEST_ASSERT_EQUAL(i, *value);
        TEST_ASSERT_TRUE(oha_tpht_increase_global_time(table, 2000 + 50 + i));
    }
    TEST_ASSERT_EQUAL(0, oha_tpht_next_timeout_entries(table, next_pairs, num_timeouts));

    // timeout all elements of timeout queue 1
    TEST_ASSERT_TRUE(oha_tpht_increase_global_time(table, 2000 + 50 + 30));
    TEST_ASSERT_EQUAL(5, oha_tpht_next_timeout_entries(table, next_pairs, num_timeouts));
    for (uint64_t i = 0; i < 5; i++) {
        TEST_ASSERT_EQUAL(i + 25, *(uint64_t *)next_pairs[i].value);
    }

    // check elements of second queue, the orgin timestamp keep untouched
    TEST_ASSERT_TRUE(oha_tpht_increase_global_time(table, 2000 + 200 + 14));
    TEST_ASSERT_EQUAL(0, oha_tpht_next_timeout_entries(table, next_pairs, num_timeouts));

    TEST_ASSERT_TRUE(oha_tpht_increase_global_time(table, 2000 + 200 + 14 + 10));
    TEST_ASSERT_EQUAL(10, oha_tpht_next_timeout_entries(table, next_pairs, num_timeouts));
    for (uint64_t i = 0; i < 10; i++) {
        TEST_ASSERT_EQUAL(i + 15, *(uint64_t *)next_pairs[i].value);
    }

    TEST_ASSERT_TRUE(oha_tpht_increase_global_time(table, 2000000));
    TEST_ASSERT_EQUAL(0, oha_tpht_next_timeout_entries(table, next_pairs, num_timeouts));
}

void test_simple_insert_look_up_remove()
{
    for (size_t elems = 1; elems < 100; elems++) {

        struct oha_tpht_config config;
        memset(&config, 0, sizeof(config));
        config.lpht_config.load_factor = LOAF_FACTOR;
        config.lpht_config.key_size = sizeof(uint64_t);
        config.lpht_config.value_size = sizeof(uint64_t);
        config.lpht_config.max_elems = elems;

        struct oha_tpht * table = oha_tpht_create(&config);

        for (uint64_t i = 0; i < elems; i++) {
            TEST_ASSERT_NULL(oha_tpht_look_up(table, &i));
            uint64_t * value_insert = oha_tpht_insert(table, &i, 0);
            TEST_ASSERT_NOT_NULL(value_insert);
            *value_insert = i;
            TEST_ASSERT_NOT_NULL(oha_tpht_look_up(table, &i));
        }

        TEST_ASSERT_NULL(oha_tpht_insert(table, &elems, 0));

        for (uint64_t i = 0; i < elems; i++) {
            for (uint64_t j = 0; j < i; j++) {
                uint64_t * value_lool_up = oha_tpht_look_up(table, &j);
                TEST_ASSERT_NULL(value_lool_up);
            }
            for (uint64_t j = i; j < elems; j++) {
                uint64_t * value_lool_up = oha_tpht_look_up(table, &j);
                TEST_ASSERT_NOT_NULL(value_lool_up);
            }
            uint64_t * removed_value = oha_tpht_remove(table, &i);
            TEST_ASSERT_NOT_NULL(removed_value);
            TEST_ASSERT_EQUAL_UINT64(*removed_value, i);
            *removed_value = (uint64_t)-1;
            TEST_ASSERT_NULL(oha_tpht_look_up(table, &i));
            TEST_ASSERT_NULL(oha_tpht_remove(table, &i));
        }

        oha_tpht_destroy(table);
    }
}

void test_insert_look_up_remove_with_timeout_slot()
{
    for (size_t elems = 1; elems < 200; elems++) {

        struct oha_tpht_config config;
        memset(&config, 0, sizeof(config));
        config.lpht_config.load_factor = LOAF_FACTOR;
        config.lpht_config.key_size = sizeof(uint64_t);
        config.lpht_config.value_size = sizeof(uint64_t);
        config.lpht_config.max_elems = elems;

        struct oha_tpht * table = oha_tpht_create(&config);

        TEST_ASSERT_EQUAL(1, oha_tpht_add_timeout_slot(table, 1000, elems, false));

        for (uint64_t i = 0; i < elems; i++) {
            uint64_t * value_insert = oha_tpht_insert(table, &i, 1);
            TEST_ASSERT_NOT_NULL(value_insert);
            *value_insert = i;
        }

        TEST_ASSERT_NULL(oha_tpht_insert(table, &elems, 1));

        for (uint64_t i = 0; i < elems; i++) {
            for (uint64_t j = 0; j < i; j++) {
                uint64_t * value_lool_up = oha_tpht_look_up(table, &j);
                TEST_ASSERT_NULL(value_lool_up);
            }
            for (uint64_t j = i; j < elems; j++) {
                uint64_t * value_lool_up = oha_tpht_look_up(table, &j);
                TEST_ASSERT_NOT_NULL(value_lool_up);
            }
            uint64_t * removed_value = oha_tpht_remove(table, &i);
            TEST_ASSERT_NOT_NULL(removed_value);
            TEST_ASSERT_EQUAL_UINT64(*removed_value, i);
            *removed_value = (uint64_t)-1;
            TEST_ASSERT_NULL(oha_tpht_look_up(table, &i));
            TEST_ASSERT_NULL(oha_tpht_remove(table, &i));
        }

        oha_tpht_destroy(table);
    }
}

void test_insert_look_up_timeout()
{
    const size_t num_timeouts = 1000;
    struct oha_key_value_pair next_pairs[num_timeouts];
    for (size_t elems = 1; elems < 200; elems++) {

        struct oha_tpht_config config;
        memset(&config, 0, sizeof(config));
        config.lpht_config.load_factor = LOAF_FACTOR;
        config.lpht_config.key_size = sizeof(uint64_t);
        config.lpht_config.value_size = sizeof(uint64_t);
        config.lpht_config.max_elems = elems;

        struct oha_tpht * table = oha_tpht_create(&config);

        TEST_ASSERT_EQUAL(1, oha_tpht_add_timeout_slot(table, 1000, elems, false));

        TEST_ASSERT_TRUE(oha_tpht_increase_global_time(table, 1000));
        for (uint64_t i = 0; i < elems; i++) {
            oha_tpht_increase_global_time(table, 1000 + i);
            uint64_t * value_insert = oha_tpht_insert(table, &i, 1);
            TEST_ASSERT_NOT_NULL(value_insert);
            *value_insert = i;
            TEST_ASSERT_EQUAL(0, oha_tpht_next_timeout_entries(table, next_pairs, num_timeouts));
        }

        // table is full
        TEST_ASSERT_NULL(oha_tpht_insert(table, &elems, 1));

        for (uint64_t i = 0; i < elems; i++) {
            uint64_t * value_lool_up = oha_tpht_look_up(table, &i);
            TEST_ASSERT_NOT_NULL(value_lool_up);
            TEST_ASSERT_EQUAL_UINT64(*value_lool_up, i);
        }
        TEST_ASSERT_NULL(oha_tpht_look_up(table, &elems));

        for (uint64_t i = 0; i < elems; i++) {
            memset(next_pairs, 0, sizeof(struct oha_key_value_pair) * num_timeouts);
            TEST_ASSERT_TRUE(oha_tpht_increase_global_time(table, 2000 + i));
            TEST_ASSERT_NOT_NULL(oha_tpht_look_up(table, &i));
            TEST_ASSERT_EQUAL(1, oha_tpht_next_timeout_entries(table, next_pairs, num_timeouts));
            uint64_t * value_timeout = next_pairs[0].value;
            TEST_ASSERT_NOT_NULL(value_timeout);
            TEST_ASSERT_EQUAL(i, *value_timeout);
            TEST_ASSERT_EQUAL(0, oha_tpht_next_timeout_entries(table, next_pairs, num_timeouts));
            TEST_ASSERT_NULL(oha_tpht_look_up(table, &i));
        }

        oha_tpht_destroy(table);
    }
}

void test_update_time_for_entry()
{
    const size_t num_timeouts = 5;
    struct oha_key_value_pair next_pairs[num_timeouts];
    struct oha_tpht_config config;
    memset(&config, 0, sizeof(config));
    config.lpht_config.load_factor = LOAF_FACTOR;
    config.lpht_config.key_size = sizeof(uint64_t);
    config.lpht_config.value_size = sizeof(uint64_t);
    config.lpht_config.max_elems = num_timeouts;

    struct oha_tpht * table = oha_tpht_create(&config);

    TEST_ASSERT_EQUAL(1, oha_tpht_add_timeout_slot(table, 1000, num_timeouts, false));

    uint64_t key = 7;
    TEST_ASSERT_TRUE(oha_tpht_increase_global_time(table, 1000));
    uint64_t * value_insert = oha_tpht_insert(table, &key, 1);
    *value_insert = key;

    TEST_ASSERT_EQUAL(0, oha_tpht_next_timeout_entries(table, next_pairs, num_timeouts));

    key = 9;
    TEST_ASSERT_TRUE(oha_tpht_increase_global_time(table, 1001));
    value_insert = oha_tpht_insert(table, &key, 1);
    *value_insert = key;

    TEST_ASSERT_EQUAL(0, oha_tpht_next_timeout_entries(table, next_pairs, num_timeouts));

    TEST_ASSERT_TRUE(oha_tpht_increase_global_time(table, 1999));

    TEST_ASSERT_EQUAL(0, oha_tpht_next_timeout_entries(table, next_pairs, num_timeouts));

    key = 7;
    uint64_t * value_update = oha_tpht_update_time_for_entry(table, &key, 2500);
    TEST_ASSERT_NOT_NULL(value_update);
    TEST_ASSERT_EQUAL(7, *value_update);
    TEST_ASSERT_EQUAL(0, oha_tpht_next_timeout_entries(table, next_pairs, num_timeouts));

    TEST_ASSERT_TRUE(oha_tpht_increase_global_time(table, 2000));
    TEST_ASSERT_EQUAL(0, oha_tpht_next_timeout_entries(table, next_pairs, num_timeouts));

    TEST_ASSERT_TRUE(oha_tpht_increase_global_time(table, 2001));
    TEST_ASSERT_EQUAL(1, oha_tpht_next_timeout_entries(table, next_pairs, num_timeouts));
    TEST_ASSERT_EQUAL(9, *(uint64_t *)next_pairs->value);

    oha_tpht_increase_global_time(table, 3499);
    TEST_ASSERT_EQUAL(0, oha_tpht_next_timeout_entries(table, next_pairs, num_timeouts));

    TEST_ASSERT_TRUE(oha_tpht_increase_global_time(table, 3500));
    TEST_ASSERT_EQUAL(1, oha_tpht_next_timeout_entries(table, next_pairs, num_timeouts));
    TEST_ASSERT_EQUAL(7, *(uint64_t *)next_pairs->value);

    TEST_ASSERT_TRUE(oha_tpht_increase_global_time(table, 4000));
    TEST_ASSERT_EQUAL(0, oha_tpht_next_timeout_entries(table, next_pairs, num_timeouts));

    oha_tpht_destroy(table);
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_create_destroy);
    RUN_TEST(test_create_destroy_add_slot);
    RUN_TEST(test_set_timeout_slot);
    RUN_TEST(test_simple_insert_look_up_remove);
    RUN_TEST(test_insert_look_up_remove_with_timeout_slot);
    RUN_TEST(test_insert_look_up_timeout);
    RUN_TEST(test_update_time_for_entry);

    return UNITY_END();
}
