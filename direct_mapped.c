#include <stdint.h>
#include <limits.h>
#include <stdio.h>
#include "memory_block.h"
#include "direct_mapped.h"
#include <math.h>

direct_mapped_cache* dmc_init(main_memory* mm)
{
    direct_mapped_cache* result = malloc(sizeof(direct_mapped_cache));
    result->mm = mm;
    result->cs = cs_init();

    int i;
    for(i = 0; i < DIRECT_MAPPED_NUM_SETS; i++)
    {
        result->blocks[i] = malloc(sizeof(struct memory_block));
        result->blocks[i]->size = MAIN_MEMORY_BLOCK_SIZE;
        result->valid[i] = 0;
        result->dirty[i] = 0;
    }
    return result;
}

char *getBinary(unsigned int num)
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

// Optional
static int addr_to_set(void* addr)
{
	int intaddr = (uintptr_t)addr;
	char* baddr = getBinary(intaddr);

	int setbits = log2(DIRECT_MAPPED_NUM_SETS);
	int setindex = 0;
	int i;
	for (i = 0; i<setbits; i++){
		int x = baddr[26 + i] - '0';
		setindex += x * pow(2, (setbits-i));
	}
    
    return setindex;
}


void dmc_store_word(direct_mapped_cache* dmc, void* addr, unsigned int val)
{
	size_t addr_offt = (size_t) (addr - MAIN_MEMORY_START_ADDR) % MAIN_MEMORY_BLOCK_SIZE;
    void* mb_start_addr = addr - addr_offt;

    int index = addr_to_set(addr);

    printf("%02x\n", ((uint8_t*) mb_start_addr));
    printf("%d", index);
    printf("\n");
    printf("%02x\n", ((uint8_t*) dmc->blocks[index]->start_addr));
    
    if (mb_start_addr == dmc->blocks[index]->start_addr){
    	unsigned int* mb_addr = dmc->blocks[index]->data + addr_offt;
    	*mb_addr = val;
    	dmc->dirty[index] = 1;
    	++dmc->cs.w_queries;

    }
    else{
    	if (dmc->dirty[index] == 1){
    		mm_write(dmc->mm, dmc->blocks[index]->start_addr, dmc->blocks[index]);
    	}
    	memory_block* mb = mm_read(dmc->mm, mb_start_addr);
    	unsigned int* mb_addr = mb->data + addr_offt;
    	*mb_addr = val;
    	dmc->blocks[index] = mb;
    	dmc->valid[index] = 1;
    	dmc->dirty[index] = 1;
    	++dmc->cs.w_misses;
    	++dmc->cs.w_queries;
    	mb_free(mb);
    }
}

unsigned int dmc_load_word(direct_mapped_cache* dmc, void* addr)
{   
    // Precompute start address of memory block
    size_t addr_offt = (size_t) (addr - MAIN_MEMORY_START_ADDR) % MAIN_MEMORY_BLOCK_SIZE;
    void* mb_start_addr = addr - addr_offt;

    int index = addr_to_set(addr);

    printf("%02x\n", ((uint8_t*) mb_start_addr));
    printf("%d", index);
    printf("\n");
    printf("%02x\n", ((uint8_t*) dmc->blocks[index]->start_addr));

    if (mb_start_addr == dmc->blocks[index]->start_addr){
    	unsigned int* mb_addr = dmc->blocks[index]->data + addr_offt;
   		unsigned int result = *mb_addr;
   		++dmc->cs.r_queries;
   		return result;
    }

    else{
    	if (dmc->dirty[index] == 1){
    		mm_write(dmc->mm, dmc->blocks[index]->start_addr, dmc->blocks[index]);
    	}
    	memory_block* mb = mm_read(dmc->mm, mb_start_addr);
    	unsigned int* mb_addr = mb->data + addr_offt;
    	unsigned int result = *mb_addr;
    	dmc->blocks[index] = mb;
    	
    	dmc->valid[index] = 1;
    	dmc->dirty[index] = 0;

    	++dmc->cs.r_misses;
    	++dmc->cs.r_queries;

    	mb_free(mb);
    	return result;
    }	
}


void dmc_free(direct_mapped_cache* dmc)
{
    free(dmc);
}
