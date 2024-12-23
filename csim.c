// Nathan Zhao

/*
 * Simulates a cache using an LRU policy.
 * Outputs statistics on the number of cache hits, misses, and evictions.
 */
#include <assert.h>
#include <errno.h>
#include <getopt.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// Includes externally class-provided file with pre-made functionalities for the purposes of this project.
#include "cachelab.h"

#define ADDRESS_LENGTH 64

#define pow(two, shift) (1 << (shift))

// data type for representing a memory address
typedef unsigned long long int memory_address_t;

// data type for representing a cache line to implement the LRU replacement policy
typedef struct cache_entry {
    char is_valid;
    memory_address_t tag_value;
    unsigned long long int lru_counter;
} cache_entry_t;

typedef cache_entry_t* cache_group_t;
typedef cache_group_t* cache_table_t;

// variables set through command-line arguments
int verbose_mode = 0; // enables verbose tracing
int set_index_bits = 0; // number of bits for set index
int block_offset_bits = 0; // number of bits for block offset
int associativity = 0; // number of cache lines per set
char* trace_filename = NULL;

// variables from command-line arguments
int number_of_sets; // total number of cache sets
int block_size_bytes; // size of cache block in bytes

// counters for cache
int total_misses = 0; 
int total_hits = 0; 
int total_evictions = 0; 
unsigned long long int global_lru_counter = 1; 

// cache
cache_table_t cache_memory;
memory_address_t set_index_mask;

void init_cache() {
    // allocate memory for cache memory with malloc
    cache_memory = (cache_group_t*)malloc(sizeof(cache_group_t) * number_of_sets);

	// per set
    for (int set_index = 0; set_index < number_of_sets; set_index++) {
        cache_memory[set_index] = (cache_entry_t*)malloc(sizeof(cache_entry_t) * associativity);

        // initialize cache lines to 0 manually
        for (int line_index = 0; line_index < associativity; line_index++) {
            cache_memory[set_index][line_index].is_valid = 0;
            cache_memory[set_index][line_index].tag_value = 0;
            cache_memory[set_index][line_index].lru_counter = 0;
        }
    }

    // calculate set index mask for extracting set indices from memory addresses
    set_index_mask = (memory_address_t)(pow(2, set_index_bits) - 1);
}

void free_cache() {
    int set_index;
    for (set_index = 0; set_index < number_of_sets; set_index++) {
        free(cache_memory[set_index]);
    }

    free(cache_memory);
}

void access_data(memory_address_t address) {
    int line_index;
    unsigned long long int min_lru_value = ULONG_MAX;
    unsigned int lru_line_index = 0;

    memory_address_t set_index = (address >> block_offset_bits) & set_index_mask;
    memory_address_t tag_value = address >> (set_index_bits + block_offset_bits);

    cache_group_t current_cache_group = cache_memory[set_index];

    // check if the address is already in cache
    for (line_index = 0; line_index < associativity; ++line_index) {

        if (current_cache_group[line_index].is_valid) {
            if (current_cache_group[line_index].tag_value == tag_value) {

				// increment counters
                current_cache_group[line_index].lru_counter = global_lru_counter++;
                total_hits++;
                return;
            }
        }

    }

    // if not found: cache miss
    total_misses++;

    // find the cache line to evict
    for (line_index = 0; line_index < associativity; ++line_index) {

        if (min_lru_value > current_cache_group[line_index].lru_counter) {
            lru_line_index = line_index;
            min_lru_value = current_cache_group[line_index].lru_counter;
        }

    }

    if (current_cache_group[lru_line_index].is_valid) {
        total_evictions++;
    }

	// store values
    current_cache_group[lru_line_index].is_valid = 1;
    current_cache_group[lru_line_index].tag_value = tag_value;
    current_cache_group[lru_line_index].lru_counter = global_lru_counter++;
}

void replay_trace(char* trace_filename) {

	// read file
    FILE* trace_file_pointer = fopen(trace_filename, "r");
    char command;
    memory_address_t address;
    int size;

    while (fscanf(trace_file_pointer, " %c %llx,%d", &command, &address, &size) == 3) {
        switch(command) {
            case 'L': access_data(address); break;
            case 'S': access_data(address); break;
            case 'M': access_data(address); access_data(address); break;
            default: break;
        }
    }

    fclose(trace_file_pointer);
}

int main(int argc, char* argv[]) {
    char option;

    // parse command-line args
    while ((option = getopt(argc, argv, "s:E:b:t:vh")) != -1) {
        switch (option) {
            case 's':
                set_index_bits = atoi(optarg);
                break;
            case 'E':
                associativity = atoi(optarg);
                break;
            case 'b':
                block_offset_bits = atoi(optarg);
                break;
            case 't':
                trace_filename = optarg;
                break;
            case 'v':
                verbose_mode = 1;
                break;
            case 'h':
                exit(0);
            default:
                exit(1);
        }
    }

    // check required command-line arguments
    if (set_index_bits == 0 || associativity == 0 || block_offset_bits == 0 || trace_filename == NULL) {
        printf("%s: Missing required command-line argument\n", argv[0]);
        exit(1);
    }

    // calculate number of sets + block size
    number_of_sets = (unsigned int)pow(2, set_index_bits);
    block_size_bytes = (unsigned int)pow(2, block_offset_bits);

    init_cache();

    replay_trace(trace_filename);

    free_cache();

    // print final results
    printSummary(total_hits, total_misses, total_evictions);
    return 0;
}
