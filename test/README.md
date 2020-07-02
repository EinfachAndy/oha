# Benchmark linear probing hash table

## Generate a benchmark file

```bash
./generate_benchmark.py >/tmp/benchmark.txt
```

## Benchmark file syntax
* '+' means a insert operation of the following key
* '?' means a lookup operation of the following key
* '-' means a remove operation of the following key

## Run the benchmark

```bash
# linear polling hash table
/usr/bin/time -v ./benchmark_shared /tmp/benchmark.txt 1

# c++ std::unordered map
/usr/bin/time -v ./benchmark_shared /tmp/benchmark.txt 2

# linear polling hash table static linking
/usr/bin/time -v ./benchmark_static /tmp/benchmark.txt 1

# linear polling hash table with fixed compile time key size
/usr/bin/time -v ./benchmark_static_8 /tmp/benchmark.txt 1
```
