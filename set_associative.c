#include <stdint.h>

#include "memory_block.h"
#include "set_associative.h"

#include <math.h>

set_associative_cache* sac_init(main_memory* mm)
{

    set_associative_cache* result = malloc(sizeof(set_associative_cache));
    result->mm = mm;
    result->cs = cs_init();

    int i;
    int j;
    for(i = 0; i < SET_ASSOCIATIVE_NUM_SETS; i++)
    {
        for(j = 0; j < SET_ASSOCIATIVE_NUM_WAYS; j++)
        {
            result->sets[i].ways[j].block = malloc(sizeof(struct memory_block));
            result->sets[i].ways[j].block->size = MAIN_MEMORY_BLOCK_SIZE;
            result->sets[i].ways[j].valid = 0;
            result->sets[i].ways[j].dirty = 0;
            result->sets[i].ways[j].lru_priority = 999999999;
        }
    }
    return result;
}

static char *getBinary(unsigned int num)
{
    char* bstring;
    int i;
    bstring = (char*) malloc(sizeof(char) * 33);
    bstring[32] = '\0'; 
    for( i = 0; i < 32; i++ )
    {
        bstring[32 - 1 - i] = (num == ((1 << i) | num)) ? '1' : '0';
    }    
    return bstring;
}

static int addr_to_set(void* addr)
{
    int intaddr = (uintptr_t)addr;
    char* baddr = getBinary(intaddr);

    int setbits = log2(SET_ASSOCIATIVE_NUM_SETS);
    int setindex = 0;
    int startidx = 27 - setbits;
    int i;
    for (i = 0; i<setbits; i++){
        int x = baddr[startidx + i] - '0';
        setindex += x * pow(2.0, (setbits -1 - i));
    }
    // printf(baddr);
    // printf("\n");
    return setindex;
}

static void mark_as_used(set_associative_cache* sac, int set, int way)
{
    int i;
    for(i = 0; i < SET_ASSOCIATIVE_NUM_WAYS; i++)
    {
        if(sac->sets[set].ways[i].valid == 1){
            sac->sets[set].ways[i].lru_priority += 1;
        }
    }
    sac->sets[set].ways[way].lru_priority = 0;
}

static int lru(set_associative_cache* sac, int set)
{
    int i;
    int maxp = -1;
    int maxi = -1;
    for(i = 0; i < SET_ASSOCIATIVE_NUM_WAYS; i++)
    {
        if(sac->sets[set].ways[i].lru_priority > maxp){
            maxp = sac->sets[set].ways[i].lru_priority;
            maxi = i;
        }
    }
    return maxi;
}


void sac_store_word(set_associative_cache* sac, void* addr, unsigned int val)
{
    size_t addr_offt = (size_t) (addr - MAIN_MEMORY_START_ADDR) % MAIN_MEMORY_BLOCK_SIZE;
    void* mb_start_addr = addr - addr_offt;

    int setidx = addr_to_set(mb_start_addr);

    int hit = 0;
    int idx;
    int i;
    for(i = 0; i < SET_ASSOCIATIVE_NUM_WAYS; i++)
    {
        if((sac->sets[setidx].ways[i].valid == 1) && (sac->sets[setidx].ways[i].block->start_addr == mb_start_addr)){
            hit = 1;
            idx = i;
        }
    }

    if (hit == 1){
        unsigned int* mb_addr = sac->sets[setidx].ways[idx].block->data + addr_offt;
        *mb_addr = val;
        sac->sets[setidx].ways[idx].dirty = 1;
        mark_as_used(sac, setidx, idx);

        ++sac->cs.w_queries;
    }
    else{
        int lastw = lru(sac, setidx);

        if ((sac->sets[setidx].ways[lastw].valid == 1) && (sac->sets[setidx].ways[lastw].dirty == 1)){
            mm_write(sac->mm, sac->sets[setidx].ways[lastw].block->start_addr, sac->sets[setidx].ways[lastw].block);
            mb_free(sac->sets[setidx].ways[lastw].block);
        }
        memory_block* mb = mm_read(sac->mm, mb_start_addr);
        unsigned int* mb_addr = mb->data + addr_offt;
        *mb_addr = val;
        sac->sets[setidx].ways[lastw].block = mb;
        sac->sets[setidx].ways[lastw].valid = 1;
        sac->sets[setidx].ways[lastw].dirty = 1;
        mark_as_used(sac, setidx, lastw);

        ++sac->cs.w_misses;
        ++sac->cs.w_queries;
        // mb_free(mb);
    }
}


unsigned int sac_load_word(set_associative_cache* sac, void* addr)
{
    size_t addr_offt = (size_t) (addr - MAIN_MEMORY_START_ADDR) % MAIN_MEMORY_BLOCK_SIZE;
    void* mb_start_addr = addr - addr_offt;

    int setidx = addr_to_set(mb_start_addr);

    int hit = 0;
    int idx;
    int i;
    for(i = 0; i < SET_ASSOCIATIVE_NUM_WAYS; i++)
    {

        if((sac->sets[setidx].ways[i].valid == 1) && (sac->sets[setidx].ways[i].block->start_addr == mb_start_addr)){
            hit = 1;
            idx = i;
        }
    }

    if (hit == 1){
        unsigned int* mb_addr = sac->sets[setidx].ways[idx].block->data + addr_offt;
        unsigned int result = *mb_addr;
        mark_as_used(sac, setidx, idx);
        ++sac->cs.r_queries;
        return result;

    }
    else{
        int lastw = lru(sac, setidx);
        if (sac->sets[setidx].ways[lastw].valid == 1 && sac->sets[setidx].ways[lastw].dirty == 1){
            mm_write(sac->mm, sac->sets[setidx].ways[lastw].block->start_addr, sac->sets[setidx].ways[lastw].block);
            mb_free(sac->sets[setidx].ways[lastw].block);
        }
        memory_block* mb = mm_read(sac->mm, mb_start_addr);
        unsigned int* mb_addr = mb->data + addr_offt;
        unsigned int result = *mb_addr;
        sac->sets[setidx].ways[lastw].block = mb;
        sac->sets[setidx].ways[lastw].valid = 1;
        sac->sets[setidx].ways[lastw].dirty = 0;
        mark_as_used(sac, setidx, lastw);

        ++sac->cs.r_misses;
        ++sac->cs.r_queries;
        // mb_free(mb);
        return result;
    }
}

void sac_free(set_associative_cache* sac)
{
    int i;
    int j;
    for(i = 0; i < SET_ASSOCIATIVE_NUM_SETS; i++)
    {
        for(j = 0; j < SET_ASSOCIATIVE_NUM_WAYS; j++)
        {
            mb_free(sac->sets[i].ways[j].block);
        }
    } 
    free(sac);
}