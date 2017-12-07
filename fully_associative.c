#include "memory_block.h"
#include "fully_associative.h"
#include <math.h>

fully_associative_cache* fac_init(main_memory* mm)
{
    fully_associative_cache* result = malloc(sizeof(fully_associative_cache));
    result->mm = mm;
    result->cs = cs_init();

    int i;
    for(i = 0; i < FULLY_ASSOCIATIVE_NUM_WAYS; i++)
    {
        result->ways[i].block = malloc(sizeof(struct memory_block));
        result->ways[i].block->size = MAIN_MEMORY_BLOCK_SIZE;
        result->ways[i].valid = 0;
        result->ways[i].dirty = 0;
        result->ways[i].lru_priority = 999999999;
    }
    return result;
}

// Marks used block to update LRU priority
static void mark_as_used(fully_associative_cache* fac, int way)
{
    int i;
    for(i = 0; i < FULLY_ASSOCIATIVE_NUM_WAYS; i++)
    {
        if(fac->ways[i].valid == 1){
            fac->ways[i].lru_priority += 1;
        }
    }
    fac->ways[way].lru_priority = 0;
}

// Returns LRU priority
static int lru(fully_associative_cache* fac)
{
    int i;
    int maxp = -1;
    int maxi = -1;
    for(i = 0; i < FULLY_ASSOCIATIVE_NUM_WAYS; i++)
    {
        if(fac->ways[i].lru_priority > maxp){
            maxp = fac->ways[i].lru_priority;
            maxi = i;
        }
    }
    return maxi;
}


void fac_store_word(fully_associative_cache* fac, void* addr, unsigned int val)
{
    // Precompute start address of memory block
    size_t addr_offt = (size_t) (addr - MAIN_MEMORY_START_ADDR) % MAIN_MEMORY_BLOCK_SIZE;
    void* mb_start_addr = addr - addr_offt;

    // Precompute LRU block
    int lastw = lru(fac);

    // If a hit exists in cache find the index
    int hit = 0;
    int idx;
    int i;
    for(i = 0; i < FULLY_ASSOCIATIVE_NUM_WAYS; i++)
    {
        if((fac->ways[i].valid == 1) && (fac->ways[i].block->start_addr == mb_start_addr)){
            hit = 1;
            idx = i;
        }
    }

    // Updates cache on hit 
    if (hit == 1){
        unsigned int* mb_addr = fac->ways[idx].block->data + addr_offt;
        *mb_addr = val;
        fac->ways[idx].dirty = 1;
        fac->ways[idx].dirty = 1;
        mark_as_used(fac, idx);

        ++fac->cs.w_queries;
    }

    // Updates cache and memory on miss 
    else{
        if (fac->ways[lastw].valid == 1 && fac->ways[lastw].dirty == 1){
            mm_write(fac->mm, fac->ways[lastw].block->start_addr, fac->ways[lastw].block);
        }
        if(fac->ways[lastw].valid == 1){
            mb_free(fac->ways[lastw].block);
        }
        else{
            free(fac->ways[lastw].block);
        }
        memory_block* mb = mm_read(fac->mm, mb_start_addr);
        unsigned int* mb_addr = mb->data + addr_offt;
        *mb_addr = val;
        fac->ways[lastw].block = mb;
        fac->ways[lastw].valid = 1;
        fac->ways[lastw].dirty = 1;
        mark_as_used(fac, lastw);

        ++fac->cs.w_misses;
        ++fac->cs.w_queries;
    }
}

unsigned int fac_load_word(fully_associative_cache* fac, void* addr)
{
    // Precompute start address of memory block
    size_t addr_offt = (size_t) (addr - MAIN_MEMORY_START_ADDR) % MAIN_MEMORY_BLOCK_SIZE;
    void* mb_start_addr = addr - addr_offt;

    // Precompute LRU block
    int lastw = lru(fac);

    // If a hit exists in cache find the index
    int hit = 0;
    int idx;
    int i;
    for(i = 0; i < FULLY_ASSOCIATIVE_NUM_WAYS; i++)
    {
        if((fac->ways[i].valid == 1) && (fac->ways[i].block->start_addr == mb_start_addr)){
            hit = 1;
            idx = i;
        }
    }

    // Updates cache on hit 
    if (hit == 1){
        unsigned int* mb_addr = fac->ways[idx].block->data + addr_offt;
        unsigned int result = *mb_addr;
        mark_as_used(fac, idx);
        ++fac->cs.r_queries;
        return result;

    }

    // Updates cache and memory on miss 
    else{
        if (fac->ways[lastw].valid == 1 && fac->ways[lastw].dirty == 1){
            mm_write(fac->mm, fac->ways[lastw].block->start_addr, fac->ways[lastw].block);
            fac->ways[lastw].dirty = 0;
        }
        if(fac->ways[lastw].valid == 1){
            mb_free(fac->ways[lastw].block);
        }
        else{
            free(fac->ways[lastw].block);
        }
        memory_block* mb = mm_read(fac->mm, mb_start_addr);
        unsigned int* mb_addr = mb->data + addr_offt;
        unsigned int result = *mb_addr;
        fac->ways[lastw].block = mb;
        fac->ways[lastw].valid = 1;
        fac->ways[lastw].dirty = 0;
        mark_as_used(fac, lastw);

        ++fac->cs.r_misses;
        ++fac->cs.r_queries;
        return result;
    }
}

// Free all allocated memory
void fac_free(fully_associative_cache* fac)
{
    int i;
    for(i = 0; i < FULLY_ASSOCIATIVE_NUM_WAYS; i++)
    {
        if(fac->ways[i].valid == 1){
            mb_free(fac->ways[i].block);
        }
        else{
            free(fac->ways[i].block);
        }
    }
    free(fac);
}