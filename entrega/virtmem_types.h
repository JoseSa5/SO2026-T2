#ifndef VIRTMEM_TYPES_H
#define VIRTMEM_TYPES_H

#include <stdint.h>
#include "uthash.h"

#define INVALID_PFN -1

 
typedef struct pte_st {
    int32_t  pfn;         
    uint8_t  present:1;        
    uint8_t  referenced:1;     
    uint8_t  dirty:1;          
    uint32_t last_accessed;    
} pte_t;

 
typedef struct page_table_st {
    uint32_t nvalid;          
    pte_t   *ptes;                
} page_table_t;

 
typedef struct frame_desc_st {
    pte_t    *pte;                
    int32_t   pid;             
    uint32_t  vpn;             
} frame_desc_t;

 
typedef struct free_stack_st {
    uint32_t *ids;
    int       max_size;
    int       top;
} free_stack_t;

 
typedef struct array_elem_st {
    uint32_t pfn;
    uint8_t  use_bit;          
    uint32_t last_accessed;    
} array_elem_t;

typedef struct array_st {
    array_elem_t *ids;
    int       max_size;
    int       last_index;      
    int       top;             
} array_t;

 
typedef struct frame_table_st {
    int           min_free_frames;
    int           num_pframes;
    frame_desc_t *frames;
    free_stack_t  free_stack;
    array_t       eviction_order;
} frame_table_t;

 
typedef struct swapped_page_st {
    uint64_t page_id;          
    uint8_t  dirty:1;
    uint32_t last_accessed;
    UT_hash_handle hh;
} swapped_page_t;

typedef struct swap_hash_st {
    int      num_swapped;
    uint32_t last_swap_time_ms;
    swapped_page_t *pages;    
} swap_hash_t;

#endif
