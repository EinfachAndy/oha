#include <assert.h>
#include <iostream>
#include <stdint.h>
#include <cstring>

#include <unordered_map>
#include <flat_hash_map.hpp>
#include <google/dense_hash_map>

#include "oha.h"

struct value {
    // use different sizes of value structure to measure performance
    uint64_t array[1];
};

typedef ska::flat_hash_map<int64_t, struct value, ska::power_of_two_std_hash<int64_t> > ska_power_of_two;
typedef ska::flat_hash_map<int64_t, struct value, std::hash<int64_t> > ska_std_hash;
typedef google::dense_hash_map<int64_t, struct value, std::hash<int64_t> > google_dense_hash;

using namespace std;

#define MAX_ELEMENTS 250000


enum command {
    INVALID,
    INSERT,
    LOOKUP,
    REMOVE,
};

enum command
get_cmd(char * line, size_t length, uint64_t & key)
{
    if (length > 0) {
        switch (line[0]) {
            case '+':
                key = *(uint32_t *)(&line[1]);
                // std::cout << "+" << key << std::endl;
                return INSERT;
            case '-':
                key = *(uint32_t *)(&line[1]);
                // std::cout << "-" << key << std::endl;
                return REMOVE;
            case '?':
                key = *(uint32_t *)(&line[1]);
                // std::cout << "?" << key << std::endl;
                return LOOKUP;
            default:
                return INVALID;
        }
    }
    return INVALID;
}

int
main(int argc, char * argv[])
{
    if (argc != 3) {
        fprintf(stderr,
                "missing parameters. Use [benchmark file] [mode]\n"
                " mode:\n"
                "   1: using lpth\n"
                "   2: using c++ std::unordered_map<>\n"
                " example: ./benchmark ../../test/benchmark.txt 1\n");
        return 1;
    }
    unordered_map<uint64_t, struct value> * umap = NULL;
    ska_power_of_two * ska_power_of_tow = NULL;
    google_dense_hash * google_dense = NULL;
    struct oha_lpht * table = NULL;
    char line_buf[5];
    size_t line_buf_size = 0;
    int line_count = 0;
    ssize_t line_size;
    int retval = 0;
    FILE * fp = fopen(argv[1], "r");
    if (!fp) {
        fprintf(stderr, "Error opening file '%s'\n", argv[1]);
        return 2;
    }

    int mode = atoi(argv[2]);

    const struct oha_lpht_config config = {{0}, sizeof(uint64_t), sizeof(struct value), MAX_ELEMENTS, 0.5, false};

    switch (mode) {
        case 1:
            printf("create linear polling hash table\n");
            table = oha_lpht_create(&config);
            break;
        case 2:
            printf("create std::unordered_map\n");
            umap = new unordered_map<uint64_t, struct value>(MAX_ELEMENTS);
            break;
        case 3:
            printf("create ska::power_of_two_std_hash\n");
            ska_power_of_tow = new ska_power_of_two(MAX_ELEMENTS);
            break;
        case 4:
            printf("create googel::dense_hash_map\n");
            google_dense = new google::dense_hash_map<int64_t, struct value, std::hash<int64_t> >(MAX_ELEMENTS);
            google_dense->set_empty_key(-1);
            google_dense->set_deleted_key(-2);
            break;
        default:
            fprintf(stderr, "unsupported mode %s\n", argv[2]);
            exit(1);
    }

    /* Get initial line */
    line_size = fread(line_buf, 5, 1, fp);

    uint64_t key;
    struct value * value;
    uint64_t inserts = 0;
    uint64_t lookups = 0;
    uint64_t removes = 0;
    while (line_size > 0) {
        line_count++;

        enum command cmd = get_cmd(line_buf, line_size, key);
        switch (cmd) {
            case INVALID:
                fprintf(stderr, "invalid command in line %d \n", line_count);
                retval = 3;
                goto EXIT;
                break;
            case INSERT:
                // printf("insert: %lu\n", key);
                struct value tmp;
                tmp.array[0] = key;
                switch (mode) {
                    case 1: {
                        value = (struct value *)oha_lpht_insert(table, &key);
                        // crash if insert failed because of memory
                        memcpy(value, &tmp, sizeof(struct value));
                        break;
                    }
                    case 2: {
                        pair<uint64_t, struct value> tmp_pair(key, tmp);
                        umap->insert(tmp_pair);
                        break;
                    }
                    case 3: {
                        pair<uint64_t, struct value> tmp_pair(key, tmp);
                        ska_power_of_tow->insert(tmp_pair);
                        break;
                    }
                    case 4: {
                        google_dense->insert(google_dense_hash::value_type(key, tmp));
                        break;
                    }
                }
                inserts++;
                break;
            case LOOKUP:
                // printf("lookup: %lu\n", key);
                switch (mode) {
                    case 1: {
                        value = (struct value *)oha_lpht_look_up(table, &key);
                        if (value == NULL) {
                            exit(1);
                        }
                        break;
                    }
                    case 2: {
                        unordered_map<uint64_t, struct value>::const_iterator got = umap->find(key);
                        if (got == umap->end()) {
                            exit(2);
                        }
                        break;
                    }
                    case 3: {
                        if (ska_power_of_tow->find(key) == ska_power_of_tow->end()) {
                            exit(3);
                        }
                        break;
                    }
                    case 4: {
                        if (google_dense->find(key) == google_dense->end()) {
                            exit(3);
                        }
                        break;
                    }
                }
                lookups++;
                break;
            case REMOVE:
                // printf("remove: %lu\n", key);
                switch (mode) {
                    case 1:
                        value = (struct value *)oha_lpht_remove(table, &key);
                        break;
                    case 2:
                        umap->erase(key);
                        break;
                    case 3:
                        ska_power_of_tow->erase(key);
                        break;
                    case 4:
                        google_dense->erase(key);
                        break;
                }
                removes++;
                break;
        }
        /* Get the next item */
        line_size = fread(line_buf, 5, 1, fp);
    }

    printf("test:\n -inserts:\t%lu\n -look ups:\t%lu\n -removes:\t%lu\n", inserts, lookups, removes);
EXIT:
    delete google_dense;
    delete umap;
    delete ska_power_of_tow;
    if (table) {
        oha_lpht_destroy(table);
    }
    /* Close the file now that we are done with it */
    fclose(fp);

    return retval;
}
