#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "virtmem.h"

static void access_page(pcb_t *p, frame_table_t *ft, swap_hash_t *sw, int vpn, int write) {
    pte_t *pte = page_request(p, ft, sw, vpn);
    assert(pte);
    pte->present = 1;
    pte->referenced = 1;
    if (write) pte->dirty = 1;
    int used = 0;
    for (int i = 0; i < ft->num_pframes; i++) {
        if (ft->frames[i].pte) {
            used++;
            assert(ft->frames[i].pte->present);
            assert(ft->frames[i].pte->pfn == i);
        }
    }
    assert(used + ft->free_stack.top + 1 == ft->num_pframes);
    assert(used == ft->eviction_order.top + 1);
    for (int i = 0; i <= ft->free_stack.top; i++)
        for (int j = i + 1; j <= ft->free_stack.top; j++)
            assert(ft->free_stack.ids[i] != ft->free_stack.ids[j]);
}

int main(void) {
    for (int algo = 0; algo < 5; algo++) {
        assert(set_eviction_algo(eviction_algo_str[algo]) == algo);
        frame_table_t *ft = create_frame_table(3);
        swap_hash_t sw = {0};
        pcb_t a = {.pid = 100}, b = {.pid = 200};
        assert(create_page_table(&a.page_table, 8) == 0);
        assert(create_page_table(&b.page_table, 8) == 0);
        assert(find_page(&a.page_table, 0) == NULL);
        assert(find_page(&a.page_table, 9) == NULL);
        assert(find_page(&a.page_table, 8) == &a.page_table.ptes[7]);
         
        for (int round = 0; round < 20; round++) {
            for (int vpn = 1; vpn <= 8; vpn++) {
                access_page(&a, ft, &sw, vpn, vpn % 2);
                access_page(&b, ft, &sw, 9 - vpn, 0);
            }
        }
        free_pages(ft, &sw, &a);
        free(a.page_table.ptes);
        for (int vpn = 1; vpn <= 8; vpn++) access_page(&b, ft, &sw, vpn, 1);
        free_pages(ft, &sw, &b);
        assert(ft->free_stack.top == 2);
        assert(ft->eviction_order.top == -1);
        assert(sw.num_swapped == 0);
        free(b.page_table.ptes);
        destroy_swap(&sw);
        free(ft->frames);
        free(ft->free_stack.ids);
        free(ft->eviction_order.ids);
        free(ft);
    }
    puts("Invariantes de memoria, limites e libertacao com dois processos: OK");
}
