 
 
 

#include "virtmem_types.h"
#include "virtmem.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>
#include <time.h>

#include "queue.h"

const char* eviction_algo_str[] = { "FIFO", "RANDOM", "LRU", "NRU", "CLOCK", NULL };

static eviction_algo_en eviction_algo = FIFO;

// Contadores para os resultados finais
static int npageacc = 0;
static int npagefaults = 0;
static int nswapin = 0;
static int nswapout = 0;
 
#define NRU_RESET_ACCESSES 4
static uint32_t access_sequence = 0;

int set_eviction_algo(const char *ev_str) {
    for (int i = 0; eviction_algo_str[i] != NULL; i++) {
        if (strcasecmp(eviction_algo_str[i], ev_str) == 0) {
            eviction_algo = (eviction_algo_en) i;
            return eviction_algo;
        }
    }
    return -1;
}

const char *get_eviction_algo_str(void) {
    return eviction_algo_str[eviction_algo];
}

 
 
 

frame_table_t *create_frame_table(int num_pframes) {
    if (num_pframes <= 0) {
        printf("create_frame_table: invalid num_pframes=%d\n", num_pframes);
        return NULL;
    }

    frame_table_t *ft = malloc(sizeof(frame_table_t));
    if (!ft) {
        printf("Cannot allocate memory for frame table\n");
        return NULL;
    }

    ft->frames = calloc((size_t)num_pframes, sizeof(frame_desc_t));
    if (!ft->frames) {
        printf("Cannot allocate memory for frame descriptors\n");
        free(ft);
        return NULL;
    }
    ft->min_free_frames = 1;
    ft->num_pframes = num_pframes;
    for (int i = 0; i < num_pframes; i++) ft->frames[i].pid = -1;

    if (init_free_stack(&ft->free_stack, num_pframes) < 0) {
        free(ft->frames);
        free(ft);
        return NULL;
    }
    if (init_eviction_arr(&ft->eviction_order, num_pframes) < 0) {
        printf("Cannot allocate memory for eviction order\n");
        free(ft->free_stack.ids);
        free(ft->frames);
        free(ft);
        return NULL;
    }
    return ft;
}

int create_page_table(page_table_t *pt, int num_vpages) {
    if (pt == NULL || num_vpages <= 0) {
        printf("Invalid page table\n");
        return -1;
    }
    pt->ptes = malloc((size_t)num_vpages * sizeof(pte_t));
    if (pt->ptes == NULL) {
        printf("Cannot allocate memory for page table\n");
        return -1;
    }
    pt->nvalid = num_vpages;
    for (int i = 0; i < num_vpages; ++i) {
        pt->ptes[i].pfn = INVALID_PFN;
        pt->ptes[i].present = 0;
        pt->ptes[i].referenced = 0;
        pt->ptes[i].dirty = 0;
        pt->ptes[i].last_accessed = 0;
    }
    return 0;
}

int is_active(pte_t *page) {
    return page != NULL && page->present;
}

int is_valid(pte_t *page) {
    return page != NULL && page->pfn != INVALID_PFN;
}

pte_t *find_page(page_table_t *pt, int32_t vpn) {
    if (pt == NULL || vpn <= 0 || (uint32_t)vpn > pt->nvalid) {
        return NULL;
    }
    return &pt->ptes[vpn - 1];
}

 
 
 

int init_free_stack(free_stack_t *stack, int num_pframes) {
    if (stack == NULL || num_pframes <= 0) {
        printf("Invalid free stack\n");
        return -1;
    }
    stack->ids = malloc((size_t)num_pframes * sizeof(uint32_t));
    if (stack->ids == NULL) {
        printf("Cannot allocate memory for free stack\n");
        return -1;
    }
    stack->top = num_pframes - 1;
    stack->max_size = num_pframes;
    for (int i = 0; i < num_pframes; ++i) stack->ids[i] = i;
    return 0;
}

int push_free_frame(free_stack_t *stack, int pfn) {
    if (stack == NULL || pfn < 0 || pfn >= stack->max_size || stack->top >= (stack->max_size - 1)) {
        return 0;
    }
    stack->ids[++(stack->top)] = (uint32_t)pfn;
    return 1;
}

 
int pop_free_frame(free_stack_t *stack) {
    if (stack == NULL || stack->top < 0) return INVALID_PFN;
    int pfn = stack->ids[0];
    for (int i = 1; i <= stack->top; ++i) stack->ids[i - 1] = stack->ids[i];
    stack->top--;
    return pfn;
}

 
 
 

// Swap-out: guardar a página no swap
int swap_out(swap_hash_t *swap, frame_desc_t *fd) {
    pte_t *pte = fd->pte;
    printf("[SWAP] %s pid=%d vpn=%u\n", __func__, fd->pid, fd->vpn);
    uint64_t page_key = (((uint64_t)fd->pid) << 32) | ((uint64_t)fd->vpn);

     
    swapped_page_t *existing = NULL;
    HASH_FIND(hh, swap->pages, &page_key, sizeof(uint64_t), existing);
    if (existing) {
        existing->dirty = pte->dirty;
        existing->last_accessed = pte->last_accessed;
        nswapout++;
        return 0;
    }

    swapped_page_t *swapped_page = malloc(sizeof(swapped_page_t));
    if (!swapped_page) {
        printf("Cannot allocate memory for swapped page\n");
        return -1;
    }
    swapped_page->page_id = page_key;
    swapped_page->dirty = pte->dirty;
    swapped_page->last_accessed = pte->last_accessed;
    HASH_ADD(hh, swap->pages, page_id, sizeof(uint64_t), swapped_page);
    swap->num_swapped += 1;
    nswapout++;
    return 0;
}

void destroy_swap(swap_hash_t *swap) {
    if (swap == NULL) return;
    swapped_page_t *swapped_page, *tmp;
    HASH_ITER(hh, swap->pages, swapped_page, tmp) {
        HASH_DEL(swap->pages, swapped_page);
        free(swapped_page);
    }
    swap->num_swapped = 0;
}

// Swap-in: trazer a página de volta para a RAM
int swap_in(swap_hash_t *swap, frame_desc_t *fd) {
    pte_t *pte = fd->pte;
    printf("[SWAP] %s pid=%d vpn=%u\n", __func__, fd->pid, fd->vpn);
    uint64_t page_key = (((uint64_t)fd->pid) << 32) | ((uint64_t)fd->vpn);
    swapped_page_t *swapped_page = NULL;
    HASH_FIND(hh, swap->pages, &page_key, sizeof(uint64_t), swapped_page);
    if (!swapped_page) {
        printf("vpn %d of process %d not found in swap\n", fd->vpn, fd->pid);
        return -1;
    }
     
     
    pte->dirty = 0;
    pte->last_accessed = swapped_page->last_accessed;
    nswapin++;
    return 0;
}

 
 
 

pte_t *page_request(pcb_t *pcb, frame_table_t *frame_table, swap_hash_t *swap, int vpn) {
    uint32_t current_time_ms = ++access_sequence;
    pte_t *pte = find_page(&pcb->page_table, vpn);
    if (pte == NULL) {
        fprintf(stderr, "ERROR: invalid vpn %d for process %d\n", vpn, pcb->pid);
        return NULL;
    }
    npageacc++;
    // NRU: limpar os bits R periodicamente
    if (eviction_algo == NRU && (access_sequence - 1) % NRU_RESET_ACCESSES == 0) {
        for (int i = 0; i <= frame_table->eviction_order.top; i++) {
            int pfn = frame_table->eviction_order.ids[i].pfn;
            frame_table->eviction_order.ids[i].use_bit = 0;
            frame_table->frames[pfn].pte->referenced = 0;
        }
        printf("[NRU] reset R at access %u\n", access_sequence);
    }
    printf("[VM] access=%u pid=%d vpn=%d %s\n", current_time_ms, pcb->pid, vpn,
           is_active(pte) ? "HIT" : "FAULT");
    // Hit: a página já está na RAM
    if (is_active(pte)) {                        
        pte->last_accessed = current_time_ms;
        update_eviction_array(&frame_table->eviction_order, pte->pfn, current_time_ms);
        return pte;
    }

    // Page fault: a página não está na RAM
    npagefaults++;
    if (page_eviction(frame_table, swap, frame_table->min_free_frames) < 0) return NULL;

    if (is_valid(pte)) {                         
        int32_t next_pfn = pop_free_frame(&frame_table->free_stack);
        if (next_pfn == INVALID_PFN) {      
            printf("ERROR: no free pframe to swap in vpn %d of process %d\n", vpn, pcb->pid);
            return NULL;
        }
        frame_desc_t *fd = &frame_table->frames[next_pfn];
        fd->pte = pte;
        fd->pid = pcb->pid;
        fd->vpn = vpn;
        if (swap_in(swap, fd) < 0) {
            printf("WARNING: swap-in failed for vpn %d of process %d\n", vpn, pcb->pid);
        }
        pte->pfn = next_pfn;
        pte->last_accessed = current_time_ms;
        update_eviction_array(&frame_table->eviction_order, next_pfn, current_time_ms);
        return pte;
    }

     
    int32_t next_pfn = pop_free_frame(&frame_table->free_stack);
    if (next_pfn == INVALID_PFN) {           
        printf("ERROR: no free pframe to allocate vpn %d of process %d\n", vpn, pcb->pid);
        return NULL;
    }
    frame_desc_t *fd = &frame_table->frames[next_pfn];
    pte->pfn = next_pfn;
    pte->last_accessed = current_time_ms;
    fd->pte = pte;
    fd->pid = pcb->pid;
    fd->vpn = vpn;
    fd->pte->dirty = 0;   
    update_eviction_array(&frame_table->eviction_order, next_pfn, current_time_ms);
    return pte;
}

// Libertar frames e guardar páginas sujas
int page_eviction(frame_table_t *frame_table, swap_hash_t *swap, int32_t min_pages_threshold) {
    while (frame_table->free_stack.top + 1 < min_pages_threshold) {
        int evict_pfn = evict(&frame_table->eviction_order);
        if (evict_pfn == INVALID_PFN) {
            printf("No pframe to evict!\n");
            return -1;
        }
        frame_desc_t *fd = &frame_table->frames[evict_pfn];
        for (int i = 0; i <= frame_table->eviction_order.top; i++) {
            array_elem_t *entry = &frame_table->eviction_order.ids[i];
            frame_table->frames[entry->pfn].pte->referenced = entry->use_bit;
        }
        pte_t *pte = fd->pte;
        if (eviction_algo == CLOCK && pte) pte->referenced = 0;
        if (!pte) {
            printf("pfn %d has no valid vpn to evict!\n", evict_pfn);
            continue;
        }
        printf("[VM] evict algo=%s pid=%d vpn=%u pfn=%d R=%u D=%u\n",
               get_eviction_algo_str(), fd->pid, fd->vpn, evict_pfn,
               pte->referenced, pte->dirty);
        if (pte->dirty && swap_out(swap, fd) < 0) return -1;
        pte->dirty = 0;
        pte->present = 0;
        pte->referenced = 0;
         
        uint64_t key = ((uint64_t)(uint32_t)fd->pid << 32) | fd->vpn;
        swapped_page_t *backing = NULL;
        HASH_FIND(hh, swap->pages, &key, sizeof(key), backing);
        if (!backing) pte->pfn = INVALID_PFN;
        fd->pte = NULL;
        fd->pid = -1;
        fd->vpn = 0;
        push_free_frame(&frame_table->free_stack, evict_pfn);
    }
    return 0;
}

 
 
 

int free_frames_from_memory(frame_table_t *frame_table, int32_t pid) {
    int count = 0;
    for (int i = 0; i < frame_table->num_pframes; i++) {
        frame_desc_t *fd = &frame_table->frames[i];
        if (fd->pid == pid) {
            push_free_frame(&frame_table->free_stack, i);
            remove_from_eviction_array(&frame_table->eviction_order, i);    
            if (fd->pte) fd->pte->pfn = INVALID_PFN;
            fd->pte = NULL;
            fd->pid = -1;
            fd->vpn = 0;
            count++;
        }
    }
    return count;
}

int swap_del(swap_hash_t *swap, int32_t pid, int32_t vpn) {
    uint64_t page_key = (((uint64_t)pid) << 32) | ((uint64_t)vpn);
    swapped_page_t *swapped_page = NULL;
    HASH_FIND(hh, swap->pages, &page_key, sizeof(uint64_t), swapped_page);
    if (!swapped_page) return 0;
    HASH_DEL(swap->pages, swapped_page);
    free(swapped_page);
    swap->num_swapped -= 1;
    return 1;
}

int free_frames_from_swap(swap_hash_t *swap, int32_t pid, int32_t pages) {
    int count = 0;
    for (int i = 0; i < pages; i++) {
        count += swap_del(swap, pid, i + 1);
    }
    return count;
}

// Libertar as páginas do processo e mostrar os totais
void free_pages(frame_table_t *frame_table, swap_hash_t *swap, pcb_t *pcb) {
    int mem = free_frames_from_memory(frame_table, pcb->pid);
    int sw  = free_frames_from_swap(swap, pcb->pid, pcb->page_table.nvalid);
    for (uint32_t i = 0; i < pcb->page_table.nvalid; i++) {
        pcb->page_table.ptes[i].pfn = INVALID_PFN;
        pcb->page_table.ptes[i].present = 0;
    }
    printf("Process %d terminated: freed %d pframes from RAM, %d vpages from swap\n", pcb->pid, mem, sw);
    printf("%s stats: accesses=%d faults=%d swap-in=%d swap-out=%d fault-rate=%.3f\n",
           get_eviction_algo_str(), npageacc, npagefaults, nswapin, nswapout,
           npageacc ? (float)npagefaults / (float)npageacc : 0.0f);
}

 
 
 

int init_eviction_arr(array_t *array, int num_pframes) {
    if (array == NULL || num_pframes <= 0) return -1;
    array->ids = malloc((size_t)num_pframes * sizeof(array_elem_t));
    if (array->ids == NULL) {
        printf("Cannot allocate memory for eviction array\n");
        return -1;
    }
    array->top = -1;
    array->max_size = num_pframes;
    array->last_index = 0;
    const char *seed = getenv("OSSIM_SEED");
    srand(seed ? (unsigned)strtoul(seed, NULL, 10) : (unsigned)time(NULL));
    return 0;
}

 


// FIFO: sai a página que entrou primeiro
int32_t fifo_eviction(array_t *fifo) {
    if (fifo == NULL || fifo->top < 0) return INVALID_PFN;
    int32_t pfn = fifo->ids[0].pfn;
    for (int i = 1; i <= fifo->top; ++i) {
        fifo->ids[i - 1] = fifo->ids[i];
    }
    fifo->top--;
    return pfn;
}

 



int update_eviction_array(array_t *eviction_arr, int pfn, uint32_t current_time_ms) {
    if (eviction_arr == NULL || pfn < 0 || eviction_arr->top >= (eviction_arr->max_size - 1)) {
         
        if (eviction_arr) {
            for (int i = 0; i <= eviction_arr->top; i++) {
                if (eviction_arr->ids[i].pfn == (uint32_t)pfn) {
                    eviction_arr->ids[i].last_accessed = current_time_ms;
                    eviction_arr->ids[i].use_bit = 1;
                    return 1;
                }
            }
        }
        return 0;
    }
    for (int i = 0; i <= eviction_arr->top; i++) {
        if (eviction_arr->ids[i].pfn == (uint32_t)pfn) {
            eviction_arr->ids[i].last_accessed = current_time_ms;
            eviction_arr->ids[i].use_bit = 1;
            return 1;
        }
    }
    eviction_arr->top++;
    eviction_arr->ids[eviction_arr->top].pfn     = pfn;
    eviction_arr->ids[eviction_arr->top].last_accessed = current_time_ms;
    eviction_arr->ids[eviction_arr->top].use_bit       = 1;
    return 1;
}

 



void remove_from_eviction_array(array_t *eviction_arr, int pfn) {
    if (eviction_arr == NULL) return;
    for (int i = 0; i <= eviction_arr->top; i++) {
        if (eviction_arr->ids[i].pfn == (uint32_t)pfn) {
            for (int j = i + 1; j <= eviction_arr->top; j++) {
                eviction_arr->ids[j - 1] = eviction_arr->ids[j];
            }
            eviction_arr->top--;
            if (i < eviction_arr->last_index) eviction_arr->last_index--;
            if (eviction_arr->last_index > eviction_arr->top) {
                eviction_arr->last_index = 0;
            }
            return;
        }
    }
}

static int32_t remove_victim(array_t *array, int index) {
    int32_t pfn = array->ids[index].pfn;
    remove_from_eviction_array(array, pfn);
    return pfn;
}

// RANDOM: escolher uma página ao acaso
int32_t random_eviction(array_t *array) {
    if (!array || array->top < 0) return INVALID_PFN;
    return remove_victim(array, rand() % (array->top + 1));
}

// LRU: sai a página que está há mais tempo sem ser usada
int32_t lru_eviction(array_t *array) {
    if (!array || array->top < 0) return INVALID_PFN;
    int oldest = 0;
    for (int i = 1; i <= array->top; i++) {
        if (array->ids[i].last_accessed < array->ids[oldest].last_accessed) oldest = i;
    }
    return remove_victim(array, oldest);
}

// NRU: procurar primeiro uma página com R a zero
int32_t nru_eviction(array_t *array) {
    if (!array || array->top < 0) return INVALID_PFN;
     
    for (int i = 0; i <= array->top; i++) {
        if (!array->ids[i].use_bit) return remove_victim(array, i);
    }
    return remove_victim(array, 0);
}

// CLOCK: dar segunda oportunidade às páginas com R a um
int32_t clock_eviction(array_t *array) {
    if (!array || array->top < 0) return INVALID_PFN;
    while (array->ids[array->last_index].use_bit) {
        array->ids[array->last_index].use_bit = 0;
        printf("[CLOCK] second chance pfn=%u\n", array->ids[array->last_index].pfn);
        array->last_index = (array->last_index + 1) % (array->top + 1);
    }
     
    return remove_victim(array, array->last_index);
}

// Chamar o algoritmo de substituição escolhido
int evict(array_t *eviction_arr) {
    switch (eviction_algo) {
        case FIFO: return fifo_eviction(eviction_arr);
        case RANDOM: return random_eviction(eviction_arr);
        case LRU: return lru_eviction(eviction_arr);
        case NRU: return nru_eviction(eviction_arr);
        case CLOCK: return clock_eviction(eviction_arr);
        default: return INVALID_PFN;
    }
}
