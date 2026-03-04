/**
 * src/zram_backend.c - zram swap backend
 * Intégré dans simulateur UPMEM existant
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include "config.h"

/* zram latency model (component-based, 4KB page) */
#define ZRAM_KERNEL_US 13.0
#define ZRAM_COMPRESS_US 3.0
#define ZRAM_DECOMPRESS_US 3.0
#define ZRAM_MEMCPY_US 5.0

/* zram simulation state */
typedef struct {
    uint64_t total_swapouts;
    uint64_t total_swapins;
    double total_swapout_us;
    double total_swapin_us;
    double total_cpu_overhead_us;
} zram_state_t;

static zram_state_t g_zram_state = {0};

/**
 * Simulate zram swap-out latency
 * Components: compression + overhead
 */
double zram_swap_out_latency_us(void)
{
    double latency = ZRAM_KERNEL_US + ZRAM_COMPRESS_US + ZRAM_MEMCPY_US;
    
    /* Add realistic jitter (±5%) */
    double jitter = (latency * 0.05) * ((rand() % 101 - 50) / 50.0);
    
    g_zram_state.total_swapouts++;
    g_zram_state.total_swapout_us += latency + jitter;
    g_zram_state.total_cpu_overhead_us += ZRAM_COMPRESS_US;
    
    return latency + jitter;
}

/**
 * Simulate zram swap-in latency
 * Components: decompression + overhead
 */
double zram_swap_in_latency_us(void)
{
    double latency = ZRAM_KERNEL_US + ZRAM_DECOMPRESS_US + ZRAM_MEMCPY_US;
    
    /* Add realistic jitter (±5%) */
    double jitter = (latency * 0.05) * ((rand() % 101 - 50) / 50.0);
    
    g_zram_state.total_swapins++;
    g_zram_state.total_swapin_us += latency + jitter;
    g_zram_state.total_cpu_overhead_us += ZRAM_DECOMPRESS_US;
    
    return latency + jitter;
}

/**
 * Get average zram swap-out latency
 */
double zram_get_avg_swapout_us(void)
{
    if (g_zram_state.total_swapouts == 0) return 0.0;
    return g_zram_state.total_swapout_us / g_zram_state.total_swapouts;
}

/**
 * Get average zram swap-in latency
 */
double zram_get_avg_swapin_us(void)
{
    if (g_zram_state.total_swapins == 0) return 0.0;
    return g_zram_state.total_swapin_us / g_zram_state.total_swapins;
}

/**
 * Reset zram statistics
 */
void zram_reset_stats(void)
{
    g_zram_state.total_swapouts = 0;
    g_zram_state.total_swapins = 0;
    g_zram_state.total_swapout_us = 0.0;
    g_zram_state.total_swapin_us = 0.0;
    g_zram_state.total_cpu_overhead_us = 0.0;
}

double zram_get_avg_cpu_overhead_us(void)
{
    uint64_t total_ops = g_zram_state.total_swapouts + g_zram_state.total_swapins;
    if (total_ops == 0) return 0.0;
    return g_zram_state.total_cpu_overhead_us / total_ops;
}

/**
 * Print zram statistics
 */
void zram_print_stats(void)
{
    printf("\n=== zram Baseline Stats ===\n");
    printf("Total swap outs: %lu\n", g_zram_state.total_swapouts);
    printf("Total swap ins: %lu\n", g_zram_state.total_swapins);
    printf("Avg swap out latency: %.2f µs\n", zram_get_avg_swapout_us());
    printf("Avg swap in latency: %.2f µs\n", zram_get_avg_swapin_us());
    printf("Avg CPU overhead: %.2f µs\n", zram_get_avg_cpu_overhead_us());
}