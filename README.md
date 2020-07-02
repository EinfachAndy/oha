# Ordered HAshing

## Description

This project combines a hash table and a binary heap to a so called "prioritized hash table".
The basic idea is to connect the advantages of both data structures together. You can effective
use this data structure to insert, remove, look up keys in nearly O(1) run time and at the same
time has every key in the hash table also a connected entry in the heap. At the same time every
entry in the heap has a connection to the keys of the hash table. That allows a fast searching
of the key with the smallest priority, too. The entry with the smallest value is on the top of
the heap. It is possible to get it in O(1) (see priority queue).

A common use case is to insert some key value pairs (located in hash table) and prioritize them
regarding a time stamp.

## Getting started

```bash
git clone git@github.com:EinfachAndy/oha.git
cd oha
mkdir build
cd build
cmake ..
make
ctest
sudo make install
```

## Build modifiers

- to set a fixed hash table key size at compile time set the following defintion at the target:
    `target_compile_definitions(oha PRIVATE OHA_FIX_KEY_SIZE_IN_BYTES=<n>)`
    `target_compile_definitions(oha_static PRIVATE OHA_FIX_KEY_SIZE_IN_BYTES=<n>)`
