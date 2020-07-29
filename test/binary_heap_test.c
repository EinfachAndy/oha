#include <stdlib.h>
#include <time.h>
#include <unity.h>

#include "oha.h"

/* Is run before every test, put unit init calls here. */
void setUp(void)
{
    unsigned int seed = time(NULL);
    srand(seed);
    fprintf(stderr, "pseudo-random number: %u\n", seed);
}
/* Is run after every test, put unit clean-up calls here. */
void tearDown(void)
{
}

int compare_int64_t(const void * a, const void * b)
{
    int64_t int_a = *((int64_t *)a);
    int64_t int_b = *((int64_t *)b);

    // an easy expression for comparing
    return (int_a > int_b) - (int_a < int_b);
}

static int64_t * get_random_array(size_t num_elems, unsigned int bound)
{
    int64_t * array = calloc(sizeof(uint64_t), num_elems);
    if (array == NULL) {
        return NULL;
    }

    for (size_t i = 0; i < num_elems; i++) {
        array[i] = rand() % bound;
    }

    return array;
}

void test_create_destroy()
{
    const struct oha_bh_config config = {
        .value_size = 0,
        .max_elems = 100,
    };

    struct oha_bh * heap = oha_bh_create(&config);
    oha_bh_destroy(heap);
}

void test_insert_delete_min()
{
    const size_t array_size = 100 * 1000;
    const struct oha_bh_config config = {
        .value_size = 0,
        .max_elems = array_size,
    };

    struct oha_bh * heap = oha_bh_create(&config);
    int64_t * big_rand_array = get_random_array(array_size, 100 * 1000 * 1000);

    for (size_t i = 0; i < array_size; i++) {
        oha_bh_insert(heap, big_rand_array[i]);
    }

    qsort(big_rand_array, array_size, sizeof(int64_t), compare_int64_t);

    for (size_t i = 0; i < array_size; i++) {
        int64_t next_min = oha_bh_find_min(heap);
        TEST_ASSERT_EQUAL_INT64(big_rand_array[i], next_min);
        TEST_ASSERT_NOT_NULL(oha_bh_delete_min(heap));
    }

    oha_bh_destroy(heap);
    free(big_rand_array);
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_create_destroy);
    RUN_TEST(test_insert_delete_min);

    return UNITY_END();
}
