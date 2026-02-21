/*
 * Real Swap Stress Test
 * 
 * Program that:
 * 1. Allocates real memory (malloc)
 * 2. Fills RAM beyond available capacity
 * 3. Triggers real kernel swap
 * 4. Measures actual page fault latencies
 * 
 * Usage:
 *   gcc -O2 -o test_real_swap test_real_swap.c -lm
 *   ./test_real_swap --mb 5000 --access-mb 1000
 * 
 * This shows:
 * - Real RAM usage (with top/free)
 * - Real page faults (with vmstat)
 * - Actual latencies measured
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/mman.h>
#include <math.h>

#define PAGE_SIZE 4096
#define CACHE_LINE 64

/* Timing utilities */
typedef struct {
    struct timespec start;
    struct timespec end;
} my_timer_t;

static void timer_start(my_timer_t *t) {
    clock_gettime(CLOCK_MONOTONIC, &t->start);
}

static double timer_elapsed_us(my_timer_t *t) {
    clock_gettime(CLOCK_MONOTONIC, &t->end);
    long sec_diff = t->end.tv_sec - t->start.tv_sec;
    long nsec_diff = t->end.tv_nsec - t->start.tv_nsec;
    return (sec_diff * 1e6) + (nsec_diff / 1e3);
}

/* Touch memory to force fault */
static inline void touch_page(void *page) {
    volatile char *p = (char*)page;
    *p = *p + 1;  /* Read-modify-write forces fault */
}

/* Random page access */
static uint32_t simple_rand(uint32_t *seed) {
    *seed = (*seed * 1103515245 + 12345) & 0x7fffffff;
    return *seed;
}

int main(int argc, char **argv) {
    uint32_t total_mb = 5000;      /* Total allocation */
    uint32_t access_mb = 1000;     /* How much to actively access */
    uint32_t pattern = 0;          /* 0=random, 1=sequential */
    
    /* Parse args */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--mb") == 0 && i+1 < argc) {
            total_mb = atoi(argv[i+1]);
            i++;
        }
        if (strcmp(argv[i], "--access-mb") == 0 && i+1 < argc) {
            access_mb = atoi(argv[i+1]);
            i++;
        }
        if (strcmp(argv[i], "--sequential") == 0) {
            pattern = 1;
        }
    }
    
    printf("=== REAL SWAP STRESS TEST ===\n");
    printf("Total allocation: %u MB\n", total_mb);
    printf("Active access: %u MB\n", access_mb);
    printf("Pattern: %s\n", pattern ? "sequential" : "random");
    printf("\n");
    
    /* Show system state before */
    printf("Before allocation:\n");
    (void)system("free -h | head -2");
    printf("\n");
    
    /* Calculate sizes */
    uint32_t total_pages = (total_mb * 1024 * 1024) / PAGE_SIZE;
    uint32_t access_pages = (access_mb * 1024 * 1024) / PAGE_SIZE;
    
    printf("Allocating %u pages (%u MB)...\n", total_pages, total_mb);
    
    /* Allocate all pages */
    void **pages = malloc(sizeof(void*) * total_pages);
    if (!pages) {
        perror("malloc pages array");
        return 1;
    }
    
    /* Try to allocate pages (may fail if system runs out) */
    uint32_t allocated = 0;
    for (uint32_t i = 0; i < total_pages; i++) {
        pages[i] = malloc(PAGE_SIZE);
        if (!pages[i]) {
            printf("Failed to allocate page %u (system limit reached)\n", i);
            total_pages = i;
            break;
        }
        allocated++;
        
        /* Touch every 10th page during allocation to start faults */
        if ((i % 10) == 0) {
            touch_page(pages[i]);
        }
        
        if ((i % 100000) == 0 && i > 0) {
            printf("  Allocated %u MB...\n", (i * PAGE_SIZE) / (1024*1024));
        }
    }
    
    printf("Successfully allocated: %u pages (%u MB)\n", allocated, 
           (allocated * PAGE_SIZE) / (1024*1024));
    
    /* Show system state after allocation */
    printf("\nAfter allocation (before access):\n");
    (void)system("free -h | head -2");
    printf("\n");
    
    /* Phase 2: Access pattern (triggers swap) */
    printf("Starting access pattern with %u MB active region...\n", access_mb);
    printf("This will trigger page faults and swapping.\n\n");
    
    /* Measure latencies for different access patterns */
    uint32_t num_accesses = access_pages > 1000 ? 1000 : access_pages;
    double *latencies = malloc(sizeof(double) * num_accesses);
    
    printf("Measuring %u page access latencies:\n", num_accesses);
    
    my_timer_t loop_timer;
    timer_start(&loop_timer);
    
    uint32_t seed = 12345;
    uint32_t seq_idx = 0;
    
    for (uint32_t i = 0; i < num_accesses; i++) {
        uint32_t page_idx;
        
        /* Select page */
        if (pattern == 1) {
            /* Sequential */
            page_idx = seq_idx;
            seq_idx = (seq_idx + 1) % access_pages;
        } else {
            /* Random within active region */
            page_idx = (simple_rand(&seed) % access_pages);
        }
        
        if (page_idx >= allocated) {
            printf("  Skipping page %u (not allocated)\n", page_idx);
            continue;
        }
        
        /* Measure access latency */
        my_timer_t access_timer;
        timer_start(&access_timer);
        touch_page(pages[page_idx]);
        latencies[i] = timer_elapsed_us(&access_timer);
        
        if (i % 100 == 0) {
            printf("  Access %u: %.1f µs\n", i, latencies[i]);
        }
    }
    
    double total_access_time = timer_elapsed_us(&loop_timer);
    
    /* Calculate statistics */
    double min_latency = 1e9, max_latency = 0, sum_latency = 0;
    for (uint32_t i = 0; i < num_accesses; i++) {
        if (latencies[i] < min_latency) min_latency = latencies[i];
        if (latencies[i] > max_latency) max_latency = latencies[i];
        sum_latency += latencies[i];
    }
    double avg_latency = sum_latency / num_accesses;
    
    /* Percentiles */
    double p50 = 0, p99 = 0;
    // Simple histogram approach
    uint32_t count_under_100us = 0, count_over_1000us = 0;
    for (uint32_t i = 0; i < num_accesses; i++) {
        if (latencies[i] < 100) count_under_100us++;
        if (latencies[i] > 1000) count_over_1000us++;
    }
    
    printf("\n=== RESULTS ===\n");
    printf("Total accesses: %u\n", num_accesses);
    printf("Total time: %.2f ms\n", total_access_time / 1000);
    printf("Throughput: %.2f ops/ms\n", num_accesses / (total_access_time / 1000));
    printf("\n");
    
    printf("Latency Statistics:\n");
    printf("  Min: %.2f µs\n", min_latency);
    printf("  Max: %.2f µs\n", max_latency);
    printf("  Avg: %.2f µs\n", avg_latency);
    printf("  <100 µs: %u (%.1f%%)\n", count_under_100us, 
           100.0 * count_under_100us / num_accesses);
    printf("  >1000 µs: %u (%.1f%%)\n", count_over_1000us,
           100.0 * count_over_1000us / num_accesses);
    printf("\n");
    
    /* Show final system state */
    printf("After access:\n");
    (void)system("free -h | head -2");
    printf("\n");
    
    printf("=== INTERPRETATION ===\n");
    printf("If avg latency ~40 µs:    UPMEM-like (in-memory)\n");
    printf("If avg latency ~100 µs:   SSD-like (fast storage)\n");
    printf("If avg latency >1000 µs:  Swap/HDD (slow storage)\n");
    printf("\n");
    
    printf("If >50%% pages <100 µs:    RAM not full, no real swap\n");
    printf("If >50%% pages >1000 µs:   Real swap active (likely HDD-based)\n");
    printf("\n");
    
    /* Cleanup */
    for (uint32_t i = 0; i < allocated; i++) {
        free(pages[i]);
    }
    free(pages);
    free(latencies);
    
    printf("Test complete.\n");
    return 0;
}
