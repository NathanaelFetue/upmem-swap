#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include <sys/time.h>
#include "workload.h"

/* ===== Implémentation: Générateur de Workload ===== */

workload_simulator_t* workload_init(ram_simulator_t *ram, page_table_t *pt,
                                    upmem_swap_manager_t *swap,
                                    workload_type_t type,
                                    uint32_t nr_accesses,
                                    uint32_t working_set_size)
{
    workload_simulator_t *wl = (workload_simulator_t *)malloc(sizeof(workload_simulator_t));
    if (!wl) {
        fprintf(stderr, "Erreur allocation workload_simulator_t\n");
        return NULL;
    }
    
    if (working_set_size > pt->nr_pages) {
        fprintf(stderr, "Erreur: working_set > nr_pages\n");
        free(wl);
        return NULL;
    }
    
    wl->ram = ram;
    wl->pt = pt;
    wl->swap = swap;
    wl->type = type;
    wl->nr_accesses = nr_accesses;
    wl->working_set_size = working_set_size;
    
    wl->page_hits = 0;
    wl->page_faults = 0;
    wl->swapouts = 0;
    wl->swapins = 0;
    wl->prefetch_hits = 0;
    wl->total_fault_time_us = 0.0;
    wl->sequential_index = 0;
    
    /* Prefetch off by default */
    wl->enable_prefetch = 0;
    wl->prefetch_distance = 4;
    
    DEBUG_PRINT("Workload initialisé: type=%d, accesses=%u, ws=%u, prefetch=%d",
                type, nr_accesses, working_set_size, wl->enable_prefetch);
    
    return wl;
}

/* Helper: mesure latence */
static double measure_time_us(struct timeval start, struct timeval end)
{
    return (end.tv_sec - start.tv_sec) * 1000000.0 + (end.tv_usec - start.tv_usec);
}

int workload_access_page(workload_simulator_t *wl, uint32_t page_id)
{
    if (!wl || !wl->ram || !wl->pt || !wl->swap) {
        return -1;
    }
    
    struct timeval start, end;
    gettimeofday(&start, NULL);
    
    /* Lookup page */
    page_entry_t *page = page_table_lookup(wl->pt, page_id);
    if (!page) {
        return -1;
    }
    
    if (page->status == PAGE_IN_RAM) {
        /* Page hit */
        wl->page_hits++;
        page_table_update_access(wl->pt, page_id);
        return 0;
    }
    
    /* Page fault */
    wl->page_faults++;
    
    /* Si RAM pleine: select victim et swap out */
    if (ram_is_full(wl->ram)) {
        DEBUG_PRINT("RAM pleine, selecting LRU victim");
        
        uint32_t victim_id = page_table_select_victim_lru(wl->pt);
        page_entry_t *victim_page = page_table_lookup(wl->pt, victim_id);
        
        if (victim_page && victim_page->status == PAGE_IN_RAM) {
            void *victim_data = ram_get_frame_data(wl->ram, victim_page->frame_id);
            
            /* Swap out victim */
            if (upmem_swap_out(wl->swap, victim_page, victim_data, PAGE_SIZE) == 0) {
                wl->swapouts++;
                DEBUG_PRINT("Swap out victim page %u", victim_id);
            }
            
            /* Free victim's frame */
            ram_free_frame(wl->ram, victim_page->frame_id);
        }
    }
    
    /* Alloue nouvelle frame */
    int frame_id = ram_allocate_frame(wl->ram, page_id);
    if (frame_id < 0) {
        fprintf(stderr, "Erreur allocation frame\n");
        return -1;
    }
    
    void *frame_data = ram_get_frame_data(wl->ram, frame_id);
    
    /* Si page était en SWAP: swap in */
    if (page->status == PAGE_IN_SWAP) {
        if (upmem_swap_in(wl->swap, page, frame_data, PAGE_SIZE) == 0) {
            wl->swapins++;
            DEBUG_PRINT("Swap in page %u from DPU %u", page_id, page->dpu_id);
        }
    } else {
        /* Page vierge: just initialize */
        memset(frame_data, 0, PAGE_SIZE);
    }
    
    /* Update page table */
    page_table_update_page(wl->pt, page_id, PAGE_IN_RAM, frame_id, 0, 0);
    
    gettimeofday(&end, NULL);
    double fault_time_us = measure_time_us(start, end);
    wl->total_fault_time_us += fault_time_us;
    
    return 0;
}

int workload_run(workload_simulator_t *wl)
{
    if (!wl) return -1;
    
    printf("\nRunning workload (%u accesses)...\n", wl->nr_accesses);
    
    /* Seed random */
    srand(time(NULL));
    
    for (uint32_t i = 0; i < wl->nr_accesses; i++) {
        uint32_t page_id;
        
        /* Sélectionne page selon pattern */
        switch (wl->type) {
            case WORKLOAD_RANDOM:
                page_id = rand() % wl->working_set_size;
                break;
            case WORKLOAD_SEQUENTIAL:
                page_id = wl->sequential_index;
                wl->sequential_index = (wl->sequential_index + 1) % wl->working_set_size;
                break;
            case WORKLOAD_MIXED:
                /* 70% local, 30% random */
                if (rand() % 100 < 70) {
                    page_id = (wl->sequential_index + (rand() % 100)) % wl->working_set_size;
                } else {
                    page_id = rand() % wl->working_set_size;
                }
                wl->sequential_index = (wl->sequential_index + 1) % wl->working_set_size;
                break;
            default:
                page_id = rand() % wl->working_set_size;
        }
        
        workload_access_page(wl, page_id);
        
        /* Progress bar */
        if ((i + 1) % (wl->nr_accesses / 20) == 0) {
            int progress = (i + 1) * 100 / wl->nr_accesses;
            printf("[%-20s] %3d%%\r", "####################", progress);
            fflush(stdout);
        }
    }
    printf("\n");
    
    return 0;
}

void workload_print_results(workload_simulator_t *wl)
{
    if (!wl) return;
    
    uint64_t total = wl->page_hits + wl->page_faults;
    double hitrate = (total > 0) ? (100.0 * wl->page_hits / total) : 0.0;
    
    printf("\n=== Workload Results ===\n");
    printf("Total accesses: %lu\n", total);
    printf("Page hits: %lu (%.2f%%)\n", wl->page_hits, 100.0 * wl->page_hits / total);
    printf("Page faults: %lu (%.2f%%)\n", wl->page_faults, 100.0 * wl->page_faults / total);
    printf("Swap outs: %lu\n", wl->swapouts);
    printf("Swap ins: %lu\n", wl->swapins);
    printf("Total fault time: %.2f ms\n", wl->total_fault_time_us / 1000.0);
    printf("Hit rate: %.2f%%\n", hitrate);
}

void workload_destroy(workload_simulator_t *wl)
{
    if (!wl) return;
    free(wl);
}

double workload_get_hitrate(workload_simulator_t *wl)
{
    if (!wl) return 0.0;
    
    uint64_t total = wl->page_hits + wl->page_faults;
    if (total == 0) return 0.0;
    
    return (100.0 * wl->page_hits / total);
}
