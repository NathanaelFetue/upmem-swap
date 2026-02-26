/**
 * src/zswap_backend.c - zswap swap backend
 * Intégré dans simulateur UPMEM existant
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include "config.h"

/* zswap latency model (component-based, 4KB page) */
#define ZSWAP_KERNEL_US 10.0
#define ZSWAP_COMPRESS_US 5.0
#define ZSWAP_DECOMPRESS_US 6.0
#define ZSWAP_MEMCPY_US 4.0
#define ZSWAP_SSD_FALLBACK_US 85.0
#define ZSWAP_HIT_RATE 0.70

/* zswap simulation state */
typedef struct {
    uint64_t total_swapouts;
    uint64_t total_swapins;
    uint64_t cache_hits;
    uint64_t cache_misses;
    double total_swapout_us;
    double total_swapin_us;
    double total_cpu_overhead_us;
} zswap_state_t;

static zswap_state_t g_zswap_state = {0};

/**
 * Simulate zswap swap-out latency
 * Write to compressed cache (always fast)
 */
double zswap_swap_out_latency_us(void)
{
    double latency = ZSWAP_KERNEL_US + ZSWAP_COMPRESS_US + ZSWAP_MEMCPY_US;
    
    /* Add realistic jitter (±5%) */
    double jitter = (latency * 0.05) * ((rand() % 101 - 50) / 50.0);
    
    g_zswap_state.total_swapouts++;
    g_zswap_state.total_swapout_us += latency + jitter;
    g_zswap_state.total_cpu_overhead_us += ZSWAP_COMPRESS_US;
    
    return latency + jitter;
}

/**
 * Simulate zswap swap-in latency
 * Cache hit (fast) vs cache miss (fall to SSD)
 */
double zswap_swap_in_latency_us(void)
{
    double latency;
    double r = (double)rand() / RAND_MAX;
    
    if (r < ZSWAP_HIT_RATE) {
        /* Cache hit - decompress from RAM */
        latency = ZSWAP_KERNEL_US + ZSWAP_DECOMPRESS_US + ZSWAP_MEMCPY_US;
        g_zswap_state.cache_hits++;
        g_zswap_state.total_cpu_overhead_us += ZSWAP_DECOMPRESS_US;
    } else {
        /* Cache miss - read from SSD */
        latency = ZSWAP_KERNEL_US + ZSWAP_SSD_FALLBACK_US;
        g_zswap_state.cache_misses++;
        g_zswap_state.total_cpu_overhead_us += 1.0;
    }
    
    /* Add realistic jitter (±5%) */
    double jitter = (latency * 0.05) * ((rand() % 101 - 50) / 50.0);
    
    g_zswap_state.total_swapins++;
    g_zswap_state.total_swapin_us += latency + jitter;
    
    return latency + jitter;
}

/**
 * Get average zswap swap-out latency
 */
double zswap_get_avg_swapout_us(void)
{
    if (g_zswap_state.total_swapouts == 0) return 0.0;
    return g_zswap_state.total_swapout_us / g_zswap_state.total_swapouts;
}

/**
 * Get average zswap swap-in latency
 */
double zswap_get_avg_swapin_us(void)
{
    if (g_zswap_state.total_swapins == 0) return 0.0;
    return g_zswap_state.total_swapin_us / g_zswap_state.total_swapins;
}

/**
 * Get cache hit rate
 */
double zswap_get_hit_rate(void)
{
    uint64_t total = g_zswap_state.cache_hits + g_zswap_state.cache_misses;
    if (total == 0) return 0.0;
    return (double)g_zswap_state.cache_hits / total;
}

/**
 * Reset zswap statistics
 */
void zswap_reset_stats(void)
{
    g_zswap_state.total_swapouts = 0;
    g_zswap_state.total_swapins = 0;
    g_zswap_state.cache_hits = 0;
    g_zswap_state.cache_misses = 0;
    g_zswap_state.total_swapout_us = 0.0;
    g_zswap_state.total_swapin_us = 0.0;
    g_zswap_state.total_cpu_overhead_us = 0.0;
}

double zswap_get_avg_cpu_overhead_us(void)
{
    uint64_t total_ops = g_zswap_state.total_swapouts + g_zswap_state.total_swapins;
    if (total_ops == 0) return 0.0;
    return g_zswap_state.total_cpu_overhead_us / total_ops;
}

/**
 * Print zswap statistics
 */
void zswap_print_stats(void)
{
    printf("\n=== zswap Baseline Stats ===\n");
    printf("Total swap outs: %lu\n", g_zswap_state.total_swapouts);
    printf("Total swap ins: %lu\n", g_zswap_state.total_swapins);
    printf("Cache hits: %lu\n", g_zswap_state.cache_hits);
    printf("Cache misses: %lu\n", g_zswap_state.cache_misses);
    printf("Hit rate: %.1f%%\n", zswap_get_hit_rate() * 100.0);
    printf("Avg swap out latency: %.2f µs\n", zswap_get_avg_swapout_us());
    printf("Avg swap in latency: %.2f µs\n", zswap_get_avg_swapin_us());
    printf("Avg CPU overhead: %.2f µs\n", zswap_get_avg_cpu_overhead_us());
}