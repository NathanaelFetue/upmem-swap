/* example_integration.c
 * Minimal example showing how to use upmem_swap API from another project.
 */
#include <stdio.h>
#include <stdlib.h>
#include "src/upmem_swap.h"
#include "src/page_table.h"

int main(int argc, char **argv)
{
    (void)argc; (void)argv;
    uint32_t dpus = 16;

    upmem_swap_manager_t *mgr = upmem_swap_init(dpus);
    if (!mgr) {
        fprintf(stderr, "Failed to init upmem swap manager\n");
        return 1;
    }

    /* Create a dummy page entry */
    page_entry_t page;
    page.page_id = 0;
    page.status = PAGE_IN_RAM;

    char buf[PAGE_SIZE];
    for (int i = 0; i < PAGE_SIZE; i++) buf[i] = (char)(i & 0xFF);

    /* Swap out the page */
    if (upmem_swap_out(mgr, &page, buf, PAGE_SIZE) != 0) {
        fprintf(stderr, "swap_out failed\n");
    } else {
        printf("Page %u swapped to DPU %u offset %lu\n", page.page_id, page.dpu_id, page.dpu_offset);
    }

    /* Swap it back in */
    if (upmem_swap_in(mgr, &page, buf, PAGE_SIZE) != 0) {
        fprintf(stderr, "swap_in failed\n");
    } else {
        printf("Page %u swapped back in\n", page.page_id);
    }

    upmem_swap_destroy(mgr);
    return 0;
}
