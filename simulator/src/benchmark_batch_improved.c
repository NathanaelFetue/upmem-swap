#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "config.h"
#include "memory_sim.h"
#include "page_table.h"
#include "upmem_swap.h"

/* ===== Benchmark Improved: Batch vs Serial with proper page state setup ===== */

typedef struct {
    uint32_t batch_size;
    double total_swapout_us;
    double total_swapin_us;
    double avg_swapout_per_page_us;
    double avg_swapin_per_page_us;
    double speedup_out;
    double speedup_in;
} improved_batch_result_t;

static double measure_time_us(struct timeval start, struct timeval end)
{
    return (end.tv_sec - start.tv_sec) * 1000000.0 + (end.tv_usec - start.tv_usec);
}

/* Benchmark serial swap with actual page fault simulation */
static improved_batch_result_t benchmark_serial(upmem_swap_manager_t *mgr,
                                               page_table_t *pt,
                                               uint32_t num_pages)
{
    improved_batch_result_t result = {0};
    result.batch_size = 1;
    
    /* Allocate pages and force them to SWAP state */
    page_entry_t **pages = (page_entry_t **)malloc(num_pages * sizeof(page_entry_t*));
    void **data = (void **)malloc(num_pages * sizeof(void*));
    
    for (uint32_t i = 0; i < num_pages; i++) {
        pages[i] = page_table_lookup(pt, i);
        data[i] = malloc(PAGE_SIZE);
        memset(data[i], 0xAA + i, PAGE_SIZE);
        pages[i]->status = PAGE_IN_RAM;  /* Ensure in RAM initially */
    }
    
    /* Reset stats */
    uint64_t swapout_baseline = mgr->total_swapouts;
    double swapout_time_baseline = mgr->total_swapout_time_us;
    uint64_t swapin_baseline = mgr->total_swapins;
    double swapin_time_baseline = mgr->total_swapin_time_us;
    
    /* Perform SWAP-OUT (serial) */
    for (uint32_t i = 0; i < num_pages; i++) {
        upmem_swap_out(mgr, pages[i], data[i], PAGE_SIZE);
    }
    
    /* Calculate time delta */
    result.total_swapout_us = mgr->total_swapout_time_us - swapout_time_baseline;
    result.avg_swapout_per_page_us = result.total_swapout_us / num_pages;
    
    /* Perform SWAP-IN (serial) */
    swapout_baseline = mgr->total_swapouts;  /* Update for swapin baseline */
    swapin_time_baseline = mgr->total_swapin_time_us;
    
    for (uint32_t i = 0; i < num_pages; i++) {
        upmem_swap_in(mgr, pages[i], data[i], PAGE_SIZE);
    }
    
    result.total_swapin_us = mgr->total_swapin_time_us - swapin_time_baseline;
    result.avg_swapin_per_page_us = result.total_swapin_us / num_pages;
    
    result.speedup_out = 1.0;
    result.speedup_in = 1.0;
    
    /* Cleanup */
    for (uint32_t i = 0; i < num_pages; i++) {
        free(data[i]);
    }
    free(pages);
    free(data);
    
    return result;
}

/* Benchmark batch swap */
static improved_batch_result_t benchmark_batch(upmem_swap_manager_t *mgr,
                                              page_table_t *pt,
                                              uint32_t batch_size,
                                              uint32_t total_pages,
                                              improved_batch_result_t baseline)
{
    improved_batch_result_t result = {0};
    result.batch_size = batch_size;
    
    /* Allocate pages and RESET to RAM state (not in SWAP) */
    page_entry_t **pages = (page_entry_t **)malloc(total_pages * sizeof(page_entry_t*));
    void **data = (void **)malloc(total_pages * sizeof(void*));
    
    for (uint32_t i = 0; i < total_pages; i++) {
        pages[i] = page_table_lookup(pt, i);
        data[i] = malloc(PAGE_SIZE);
        memset(data[i], 0xBB + i, PAGE_SIZE);
        pages[i]->status = PAGE_IN_RAM;  /* Reset to RAM state */
    }
    
    /* Reinitialize DPU states for clean batch test */
    for (uint32_t d = 0; d < mgr->nr_dpus; d++) {
        mgr->dpu_states[d].free_offset = 0;  /* Reset MRAM offsets */
    }
    mgr->next_dpu = 0;
    
    /* Reset stats */
    uint64_t swapout_baseline_count = mgr->total_swapouts;
    double swapout_time_baseline = mgr->total_batch_swapout_time_us;
    
    /* Perform BATCH SWAP-OUT */
    for (uint32_t i = 0; i < total_pages; i += batch_size) {
        uint32_t current_batch_size = (i + batch_size > total_pages) ? 
                                      (total_pages - i) : batch_size;
        upmem_swap_out_batch(mgr, &pages[i], &data[i], current_batch_size);
    }
    
    result.total_swapout_us = mgr->total_batch_swapout_time_us - swapout_time_baseline;
    result.avg_swapout_per_page_us = result.total_swapout_us / total_pages;
    result.speedup_out = baseline.total_swapout_us / result.total_swapout_us;
    
    /* Reset stats for swapin and page states */
    for (uint32_t i = 0; i < total_pages; i++) {
        pages[i]->status = PAGE_IN_SWAP;  /* All pages now in SWAP */
    }
    double swapin_time_baseline = mgr->total_batch_swapin_time_us;
    
    /* Perform BATCH SWAP-IN */
    for (uint32_t i = 0; i < total_pages; i += batch_size) {
        uint32_t current_batch_size = (i + batch_size > total_pages) ? 
                                      (total_pages - i) : batch_size;
        upmem_swap_in_batch(mgr, &pages[i], &data[i], current_batch_size);
    }
    
    result.total_swapin_us = mgr->total_batch_swapin_time_us - swapin_time_baseline;
    result.avg_swapin_per_page_us = result.total_swapin_us / total_pages;
    result.speedup_in = baseline.total_swapin_us / result.total_swapin_us;
    
    /* Cleanup */
    for (uint32_t i = 0; i < total_pages; i++) {
        free(data[i]);
    }
    free(pages);
    free(data);
    
    return result;
}

int main(int argc, char *argv[])
{
    uint32_t num_dpus = 8;
    uint32_t total_pages = 100;
    
    if (argc > 1) num_dpus = atoi(argv[1]);
    if (argc > 2) total_pages = atoi(argv[2]);
    
    printf("\n=== UPMEM Swap: Batch vs Serial Performance (Improved) ===\n");
    printf("DPUs: %u, Total Pages: %u\n\n", num_dpus, total_pages);
    
    /* Initialize */
    page_table_t *pt = page_table_init(total_pages * 2);
    upmem_swap_manager_t *mgr = upmem_swap_init(num_dpus);
    
    /* Benchmark serial (baseline) */
    printf("Running serial baseline (page-by-page)...\n");
    improved_batch_result_t baseline = benchmark_serial(mgr, pt, total_pages);
    
    printf("\n%-12s %-16s %-16s %-16s %-16s %-12s %-12s\n",
           "Batch Size", "Swap-Out (µs)", "Swap-In (µs)", 
           "Out/Page (µs)", "In/Page (µs)", "Out Speedup", "In Speedup");
    printf("%-12s %-16s %-16s %-16s %-16s %-12s %-12s\n",
           "───────────", "─────────────", "────────────", 
           "───────────", "──────────────", "────────────", "─────────────");
    
    printf("%-12u %-16.2f %-16.2f %-16.2f %-16.2f %-12.2f %-12.2f\n",
           1, baseline.total_swapout_us, baseline.total_swapin_us,
           baseline.avg_swapout_per_page_us, baseline.avg_swapin_per_page_us,
           1.0, 1.0);
    
    /* Save CSV */
    FILE *csv = fopen("results/benchmark_batch_improved.csv", "w");
    fprintf(csv, "batch_size,total_swapout_us,total_swapin_us,avg_swapout_per_page_us,avg_swapin_per_page_us,speedup_swapout,speedup_swapin\n");
    fprintf(csv, "1,%.2f,%.2f,%.2f,%.2f,1.00,1.00\n",
            baseline.total_swapout_us, baseline.total_swapin_us,
            baseline.avg_swapout_per_page_us, baseline.avg_swapin_per_page_us);
    
    /* Batch sizes to test */
    uint32_t batch_sizes[] = {2, 5, 10, 20, 50};
    int num_sizes = sizeof(batch_sizes) / sizeof(batch_sizes[0]);
    
    for (int i = 0; i < num_sizes; i++) {
        uint32_t batch_size = batch_sizes[i];
        if (batch_size > total_pages) continue;
        
        improved_batch_result_t result = benchmark_batch(mgr, pt, batch_size, total_pages, baseline);
        
        printf("%-12u %-16.2f %-16.2f %-16.2f %-16.2f %-12.2f %-12.2f\n",
               batch_size, result.total_swapout_us, result.total_swapin_us,
               result.avg_swapout_per_page_us, result.avg_swapin_per_page_us,
               result.speedup_out, result.speedup_in);
        
        fprintf(csv, "%u,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f\n",
                batch_size, result.total_swapout_us, result.total_swapin_us,
                result.avg_swapout_per_page_us, result.avg_swapin_per_page_us,
                result.speedup_out, result.speedup_in);
    }
    
    fclose(csv);
    printf("\n✓ Results saved to: results/benchmark_batch_improved.csv\n\n");
    
    return 0;
}
