#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "config.h"
#include "memory_sim.h"
#include "page_table.h"
#include "upmem_swap.h"

/* ===== Benchmark: Batch vs Serial Swap Performance ===== */

typedef struct {
    uint32_t batch_size;
    uint32_t num_batches;
    double total_time_us;
    double avg_per_page_us;
    double throughput_pages_per_ms;
} batch_result_t;

/* Helper: mesure latence */
static double measure_time_us(struct timeval start, struct timeval end)
{
    return (end.tv_sec - start.tv_sec) * 1000000.0 + (end.tv_usec - start.tv_usec);
}

/* Benchmark serial swap-out (baseline) */
static batch_result_t benchmark_serial_swapout(upmem_swap_manager_t *mgr,
                                               page_table_t *pt,
                                               uint32_t num_pages)
{
    batch_result_t result = {0};
    result.batch_size = 1;
    result.num_batches = num_pages;
    
    struct timeval start, end;
    gettimeofday(&start, NULL);
    
    /* Allocate test pages */
    page_entry_t **pages = (page_entry_t **)malloc(num_pages * sizeof(page_entry_t*));
    void **data = (void **)malloc(num_pages * sizeof(void*));
    
    for (uint32_t i = 0; i < num_pages; i++) {
        pages[i] = page_table_lookup(pt, i);
        data[i] = malloc(PAGE_SIZE);
        memset(data[i], 0xAA, PAGE_SIZE);
    }
    
    /* Serial swap-out */
    for (uint32_t i = 0; i < num_pages; i++) {
        if (pages[i]->status == PAGE_IN_RAM) {
            upmem_swap_out(mgr, pages[i], data[i], PAGE_SIZE);
        }
    }
    
    gettimeofday(&end, NULL);
    result.total_time_us = measure_time_us(start, end);
    result.avg_per_page_us = result.total_time_us / num_pages;
    result.throughput_pages_per_ms = (num_pages * 1000.0) / result.total_time_us;
    
    /* Cleanup */
    for (uint32_t i = 0; i < num_pages; i++) {
        free(data[i]);
    }
    free(pages);
    free(data);
    
    return result;
}

/* Benchmark batch swap-out */
static batch_result_t benchmark_batch_swapout(upmem_swap_manager_t *mgr,
                                              page_table_t *pt,
                                              uint32_t batch_size,
                                              uint32_t total_pages)
{
    batch_result_t result = {0};
    result.batch_size = batch_size;
    result.num_batches = (total_pages + batch_size - 1) / batch_size;
    
    struct timeval start, end;
    gettimeofday(&start, NULL);
    
    /* Allocate test pages */
    page_entry_t **pages = (page_entry_t **)malloc(total_pages * sizeof(page_entry_t*));
    void **data = (void **)malloc(total_pages * sizeof(void*));
    
    for (uint32_t i = 0; i < total_pages; i++) {
        pages[i] = page_table_lookup(pt, i);
        data[i] = malloc(PAGE_SIZE);
        memset(data[i], 0xBB, PAGE_SIZE);
    }
    
    /* Batch swap-out */
    for (uint32_t i = 0; i < total_pages; i += batch_size) {
        uint32_t current_batch_size = (i + batch_size > total_pages) ? 
                                      (total_pages - i) : batch_size;
        
        page_entry_t **batch_pages = &pages[i];
        void **batch_data = &data[i];
        
        upmem_swap_out_batch(mgr, batch_pages, batch_data, current_batch_size);
    }
    
    gettimeofday(&end, NULL);
    result.total_time_us = measure_time_us(start, end);
    result.avg_per_page_us = result.total_time_us / total_pages;
    result.throughput_pages_per_ms = (total_pages * 1000.0) / result.total_time_us;
    
    /* Cleanup */
    for (uint32_t i = 0; i < total_pages; i++) {
        free(data[i]);
    }
    free(pages);
    free(data);
    
    return result;
}

/* Main benchmark */
int main(int argc, char *argv[])
{
    uint32_t num_dpus = 8;
    uint32_t total_pages = 100;
    
    if (argc > 1) num_dpus = atoi(argv[1]);
    if (argc > 2) total_pages = atoi(argv[2]);
    
    printf("=== UPMEM Swap Batch Operations Benchmark ===\n");
    printf("DPUs: %u, Total Pages: %u\n\n", num_dpus, total_pages);
    
    /* Initialize managers */
    ram_simulator_t *ram = ram_init(total_pages * 2);
    page_table_t *pt = page_table_init(total_pages);
    upmem_swap_manager_t *mgr = upmem_swap_init(num_dpus);
    
    /* Open results CSV */
    FILE *csv = fopen("results/benchmark_batch.csv", "w");
    fprintf(csv, "batch_size,total_pages,total_time_us,avg_per_page_us,throughput_pages_per_ms,speedup\n");
    
    printf("%-12s %-16s %-18s %-18s %-25s %s\n",
           "Batch Size", "Total Time (µs)", "Avg/Page (µs)", "Throughput", "Speedup", "");
    printf("%-12s %-16s %-18s %-18s %-25s %s\n",
           "───────────", "────────────────", "────────────────", "─────────────────", 
           "─────────────────", "");
    
    /* Benchmark serial (baseline) */
    batch_result_t baseline = benchmark_serial_swapout(mgr, pt, total_pages);
    printf("%-12u %-16.2f %-18.2f %-18.2f %-25s %s\n",
           baseline.batch_size, baseline.total_time_us, baseline.avg_per_page_us,
           baseline.throughput_pages_per_ms, "1.00x (baseline)", "");
    fprintf(csv, "1,%u,%.2f,%.2f,%.2f,1.00\n",
            total_pages, baseline.total_time_us, baseline.avg_per_page_us, 
            baseline.throughput_pages_per_ms);
    
    /* Benchmark batch sizes */
    uint32_t batch_sizes[] = {2, 5, 10, 20, 50};
    int num_sizes = sizeof(batch_sizes) / sizeof(batch_sizes[0]);
    
    for (int i = 0; i < num_sizes; i++) {
        uint32_t batch_size = batch_sizes[i];
        if (batch_size > total_pages) continue;
        
        batch_result_t result = benchmark_batch_swapout(mgr, pt, batch_size, total_pages);
        double speedup = baseline.total_time_us / result.total_time_us;
        
        printf("%-12u %-16.2f %-18.2f %-18.2f %-25.2fx %s\n",
               batch_size, result.total_time_us, result.avg_per_page_us,
               result.throughput_pages_per_ms, speedup, "");
        fprintf(csv, "%u,%u,%.2f,%.2f,%.2f,%.2f\n",
                batch_size, total_pages, result.total_time_us, result.avg_per_page_us,
                result.throughput_pages_per_ms, speedup);
    }
    
    fclose(csv);
    
    printf("\nResults saved to: results/benchmark_batch.csv\n");
    
    /* Note: Memory cleanup is optional for short-lived benchmark */
    
    return 0;
}
