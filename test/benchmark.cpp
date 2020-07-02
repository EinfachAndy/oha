#include <assert.h>
#include <iostream>
#include <stdint.h>
#include <oha.h>
#include <unordered_map>

using namespace std;

#define MAX_ELEMENTS 250000

enum command {
    INVALID,
    INSERT,
    LOOKUP,
    REMOVE,
};

enum command get_cmd(char * line, size_t length, uint64_t & key)
{
    if (length > 0) {
        switch (line[0]) {
            case '+':
                key = atoll(&line[1]);
                return INSERT;
            case '-':
                key = atoll(&line[1]);
                return REMOVE;
            case '?':
                key = atoll(&line[1]);
                return LOOKUP;
            default:
                return INVALID;
        }
    }
    return INVALID;
}

struct value {
    // use different sizes of value structure to measure performance
    uint64_t array[1];
};

int main(int argc, char * argv[])
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
    struct oha_lpht * table = NULL;
    char * line_buf = NULL;
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

    const struct oha_lpht_config config = {
        .load_factor = 0.7,
        .key_size = sizeof(uint64_t),
        .value_size = sizeof(struct value),
        .max_elems = MAX_ELEMENTS,
    };

    switch (mode) {
        case 1:
            printf("create linear polling hash table\n");
            table = oha_lpht_create(&config);
            break;
        case 2:
            printf("create std::unordered_map\n");
            umap = new unordered_map<uint64_t, struct value>(MAX_ELEMENTS);
            break;
        default:
            fprintf(stderr, "unsupported mode %s\n", argv[2]);
            exit(1);
    }

    /* Get initial line */
    line_size = getline(&line_buf, &line_buf_size, fp);

    uint64_t key;
    struct value * value;
    uint64_t inserts = 0;
    uint64_t lookups = 0;
    uint64_t removes = 0;
    while (line_size >= 0) {
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
                    case 1:
                        value = (struct value *)oha_lpht_insert(table, &key);
                        // crash if insert failed because of memory
                        *value = tmp;
                        break;
                    case 2:
                        pair<uint64_t, struct value> tmp_pair(key, tmp);
                        umap->insert(tmp_pair);
                        break;
                }
                inserts++;
                break;
            case LOOKUP:
                // printf("lookup: %lu\n", key);
                switch (mode) {
                    case 1:
                        value = (struct value *)oha_lpht_look_up(table, &key);
                        break;
                    case 2:
                        unordered_map<uint64_t, struct value>::const_iterator got = umap->find(key);
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
                }
                removes++;
                break;
        }
        /* Get the next line */
        line_size = getline(&line_buf, &line_buf_size, fp);
    }

    printf("test:\n -inserts:\t%lu\n -look ups:\t%lu\n -removes:\t%lu\n", inserts, lookups, removes);
EXIT:
    delete umap;
    oha_lpht_destroy(table);
    /* Free the allocated line buffer */
    free(line_buf);
    line_buf = NULL;

    /* Close the file now that we are done with it */
    fclose(fp);

    return retval;
}
