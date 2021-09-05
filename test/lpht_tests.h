#include <stdlib.h>
#include <string.h>
#include <unity.h>

// good for testing to create collisions
#define LOAF_FACTOR 0.9

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

// fake some memory wrapper stuff
#define MEMORY_SIZE_OFFSET (sizeof(void *) * 7)

static inline void *
get_origin_ptr(void * user_memory_ptr)
{
    TEST_ASSERT(user_memory_ptr);

    return ((uint8_t *)user_memory_ptr) - MEMORY_SIZE_OFFSET;
}

static inline void *
get_user_memory_ptr(void * origin_ptr)
{
    TEST_ASSERT(origin_ptr);

    return ((uint8_t *)origin_ptr) + MEMORY_SIZE_OFFSET;
}

static void *
malloc_wrapper_oha(size_t size, void * user_ptr)
{
    (void)user_ptr;
    return get_user_memory_ptr(malloc(size + MEMORY_SIZE_OFFSET));
}

static void
free_wrapper_oha(void * ptr, void * user_ptr)
{
    (void)user_ptr;
    free(get_origin_ptr(ptr));
}

static void *
realloc_wrapper_oha(void * ptr, size_t size, void * user_ptr)
{
    (void)user_ptr;
    return get_user_memory_ptr(realloc(get_origin_ptr(ptr), size + MEMORY_SIZE_OFFSET));
}

void
test_create_destroy()
{
    struct oha_lpht_config config;
    memset(&config, 0, sizeof(config));
    config.max_load_factor = LOAF_FACTOR;
    config.key_size = sizeof(uint64_t);
    config.value_size = sizeof(uint64_t);
    config.max_elems = 100;

    struct oha_lpht * table = oha_lpht_create(&config);
    oha_lpht_destroy(table);
}

void
test_create_destroy_alloc_ptr()
{
    struct oha_memory_fp memory = {
        .malloc = malloc_wrapper_oha,
        .realloc = realloc_wrapper_oha,
        .free = free_wrapper_oha,
        .alloc_user_ptr = (void *)-1,
    };

    struct oha_lpht_config config;
    memset(&config, 0, sizeof(config));
    config.max_load_factor = LOAF_FACTOR;
    config.key_size = sizeof(uint64_t);
    config.value_size = sizeof(uint64_t);
    config.max_elems = 100;
    config.memory = memory;

    struct oha_lpht * table = oha_lpht_create(&config);
    oha_lpht_destroy(table);
}

void
test_insert_look_up()
{
    struct oha_lpht_config config;
    memset(&config, 0, sizeof(config));
    config.max_load_factor = LOAF_FACTOR;
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

        struct oha_lpht_status status = {0};
        TEST_ASSERT_EQUAL(0, oha_lpht_get_status(table, &status));
        TEST_ASSERT_EQUAL_UINT64(i, status.elems_in_use);
        TEST_ASSERT_EQUAL_UINT64(config.max_elems, status.max_elems);

        uint64_t * value_insert = oha_lpht_insert(table, &i);
        TEST_ASSERT_NOT_NULL(value_insert);
        *value_insert = i;
        uint64_t * value_lool_up = oha_lpht_look_up(table, &i);
        TEST_ASSERT_EQUAL_PTR(value_insert, value_lool_up);
        TEST_ASSERT_EQUAL_UINT64(*value_lool_up, i);

        TEST_ASSERT_EQUAL(0, oha_lpht_get_status(table, &status));
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

void
test_double_inserts()
{
    struct oha_lpht_config config;
    memset(&config, 0, sizeof(config));
    config.max_load_factor = LOAF_FACTOR;
    config.key_size = sizeof(uint64_t);
    config.value_size = sizeof(uint64_t);
    config.max_elems = 10;

    struct oha_lpht * table = oha_lpht_create(&config);

    for (uint64_t i = 0; i < config.max_elems; i++) {

        TEST_ASSERT_NULL(oha_lpht_look_up(table, &i));

        uint64_t * value_insert = oha_lpht_insert(table, &i);
        TEST_ASSERT_NOT_NULL(value_insert);
        *value_insert = i;

        uint64_t * double_insert = oha_lpht_insert(table, &i);
        TEST_ASSERT_NOT_NULL(double_insert);
        TEST_ASSERT_EQUAL_PTR(value_insert, double_insert);
        TEST_ASSERT_EQUAL_UINT64(*double_insert, i);
    }

    for (uint64_t i = 0; i < config.max_elems; i++) {
        uint64_t * value_lool_up = oha_lpht_look_up(table, &i);
        TEST_ASSERT_EQUAL_UINT64(*value_lool_up, i);
    }

    oha_lpht_destroy(table);
}

void
test_insert_look_up_remove()
{
    for (size_t elems = 1; elems < 500; elems++) {

        struct oha_lpht_config config;
        memset(&config, 0, sizeof(config));
        config.max_load_factor = LOAF_FACTOR;
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
                if (value_lool_up != NULL) {
                    fprintf(stderr, "\n## failure: i=%lu j=%lu  elems=%lu\n", i, j, elems);
                }
                TEST_ASSERT_NULL(value_lool_up);
            }
            for (uint64_t j = i; j < elems; j++) {
                uint64_t * value_lool_up = oha_lpht_look_up(table, &j);
                if (value_lool_up == NULL) {
                    fprintf(stderr, "\n## failure: i=%lu j=%lu  elems=%lu\n", i, j, elems);
                }
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

void
test_insert_look_up_resize()
{
    size_t init_elems = 1;
    const size_t resizes = pow(2, 6);
    struct oha_lpht_config config;
    memset(&config, 0, sizeof(config));
    config.max_load_factor = LOAF_FACTOR;
    config.key_size = sizeof(uint64_t);
    config.value_size = sizeof(void *);
    config.max_elems = init_elems;
    config.resizable = true;

    struct oha_lpht * table = oha_lpht_create(&config);
    TEST_ASSERT_NOT_NULL(table);

    for (uint64_t i = 0; i < resizes; i++) {
        void ** value_insert = oha_lpht_insert(table, &i);
        TEST_ASSERT_NOT_NULL(value_insert);
        *value_insert = value_insert;

        for (uint64_t j = 0; j < i; j++) {
            void ** value_lool_up = oha_lpht_look_up(table, &j);
            TEST_ASSERT_NOT_NULL(value_lool_up);
            TEST_ASSERT_EQUAL_PTR(*value_lool_up, value_lool_up);
        }
        for (uint64_t j = i + 1; j < resizes; j++) {
            TEST_ASSERT_NULL(oha_lpht_look_up(table, &j));
        }
    }

    for (uint64_t j = 0; j < resizes; j++) {
        void ** value_lool_up = oha_lpht_look_up(table, &j);
        TEST_ASSERT_NOT_NULL(value_lool_up);
        TEST_ASSERT_EQUAL_PTR(*value_lool_up, value_lool_up);
    }

    oha_lpht_destroy(table);
}

void
test_resize_stress_test()
{
    struct oha_memory_fp memory = {
        .malloc = malloc_wrapper_oha,
        .realloc = realloc_wrapper_oha,
        .free = free_wrapper_oha,
        .alloc_user_ptr = (void *)-1,
    };

    struct oha_lpht_config config;
    memset(&config, 0, sizeof(config));
    config.max_load_factor = 0.6;
    config.key_size = sizeof(uint64_t);
    config.value_size = sizeof(uint64_t);
    config.max_elems = 1;
    config.resizable = true;
    config.memory = memory;

    struct oha_lpht * table = oha_lpht_create(&config);
    TEST_ASSERT_NOT_NULL(table);

    for (uint64_t i = 0; i < 1000 * 1000; i++) {
        uint64_t * value_insert = oha_lpht_insert(table, &i);
        if (value_insert == NULL) {
            fprintf(stderr, "insertion of '%lu' failed!\n", i);
        }
        TEST_ASSERT_NOT_NULL(value_insert);
        *value_insert = i;
    }

    for (uint64_t i = 0; i < 1000 * 1000; i++) {
        uint64_t * look_up = oha_lpht_look_up(table, &i);
        TEST_ASSERT_NOT_NULL(look_up);
        *look_up = i;
    }

    oha_lpht_destroy(table);
}

void
test_clear_remove()
{
    struct oha_lpht_config config;
    memset(&config, 0, sizeof(config));
    config.max_load_factor = LOAF_FACTOR;
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

    TEST_ASSERT_EQUAL(0, oha_lpht_iter_init(table));

    struct oha_key_value_pair pair = {0};
    for (uint64_t i = 1; i <= config.max_elems; i++) {
        TEST_ASSERT_EQUAL(0, oha_lpht_iter_next(table, &pair));
        TEST_ASSERT_NOT_NULL(pair.key);
        TEST_ASSERT_NOT_NULL(pair.value);
        TEST_ASSERT(*(uint64_t *)pair.value > 0);
        TEST_ASSERT(*(uint64_t *)pair.value <= 100);
    }

    TEST_ASSERT_EQUAL(1, oha_lpht_iter_next(table, &pair));

    oha_lpht_destroy(table);
}

void
test_insert_with_resize()
{
    struct oha_lpht_config config;
    memset(&config, 0, sizeof(config));
    config.max_load_factor = LOAF_FACTOR;
    config.key_size = sizeof(uint64_t);
    config.value_size = sizeof(uint64_t);
    config.max_elems = 3;
    config.resizable = true;

    struct oha_lpht * table = oha_lpht_create(&config);
    TEST_ASSERT_NOT_NULL(table);

    uint64_t key = 1;
    TEST_ASSERT_NOT_NULL(oha_lpht_insert(table, &key));
    key = 2;
    TEST_ASSERT_NOT_NULL(oha_lpht_insert(table, &key));
    key = 3;
    TEST_ASSERT_NOT_NULL(oha_lpht_insert(table, &key));

    // table is full -> next insert leads to resize
    key = 4;
    TEST_ASSERT_NOT_NULL(oha_lpht_insert(table, &key));

    key = 0;
    TEST_ASSERT_NULL(oha_lpht_look_up(table, &key));
    for (uint64_t i = 1; i <= 4; i++) {
        key = i;
        TEST_ASSERT_NOT_NULL(oha_lpht_look_up(table, &key));
    }
    key = 5;
    TEST_ASSERT_NULL(oha_lpht_look_up(table, &key));

    oha_lpht_destroy(table);
}

int
main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_create_destroy);
    RUN_TEST(test_insert_look_up);
    RUN_TEST(test_insert_look_up_remove);
    RUN_TEST(test_double_inserts);
    RUN_TEST(test_clear_remove);
    RUN_TEST(test_insert_with_resize);
    RUN_TEST(test_insert_look_up_resize);
    RUN_TEST(test_resize_stress_test);

    return UNITY_END();
}
