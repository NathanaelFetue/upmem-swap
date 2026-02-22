/*
 * ===== PARALLEL LATENCY MODEL =====
 * 
 * This is what upmem_swap_out_batch SHOULD do:
 * 1. Distribute pages across DPUs (already works ✓)  
 * 2. Simulate PARALLEL transfers (not sequential)
 * 3. Latency = kernel + mram + MAX(transfer_times) [not SUM]
 * 4. Add contention model for high DPU counts
 * 
 * EXPECTED GAINS:
 *   1 DPU:   ~30 µs/page (baseline)
 *   8 DPUs:  ~3.8 µs/page (7.9× speedup)
 *   64 DPUs: ~0.6 µs/page (50× speedup, with contention penalty)
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define PAGE_SIZE 4096
#define KERNEL_OVERHEAD_US 12.0
#define MRAM_LATENCY_US 6.0
#define HOST_WRITE_BANDWIDTH_GBS 0.33  /* CPU→DPU */
#define HOST_READ_BANDWIDTH_GBS 0.12   /* DPU→CPU */

typedef struct {
    uint32_t pages_on_dpu;
    double transfer_time_us;
} dpu_transfer_info_t;

/**
 * Calculate transfer time for bytes at given bandwidth
 */
static double calc_transfer_time_us(uint32_t size_bytes, double bandwidth_gbps)
{
    if (bandwidth_gbps <= 0) return 0;
    return (size_bytes / (bandwidth_gbps * 1e9)) * 1e6;
}

/**
 * Simulate PARALLEL batch swap-out
 * Pages are distributed across multiple DPUs and transferred in parallel
 */
double simulate_parallel_batch_swapout(uint32_t num_dpus, uint32_t batch_size, 
                                       int operation)
{
    if (num_dpus == 0 || batch_size == 0) return 0;
    
    double bandwidth_gbps = (operation == 0) ? 
        HOST_WRITE_BANDWIDTH_GBS : HOST_READ_BANDWIDTH_GBS;
    
    /* Allocate pages round-robin across DPUs */
    dpu_transfer_info_t *dpus = (dpu_transfer_info_t *)malloc(sizeof(dpu_transfer_info_t) * num_dpus);
    if (!dpus) return 0;
    
    memset(dpus, 0, sizeof(dpu_transfer_info_t) * num_dpus);
    
    /* Distribute pages round-robin */
    for (uint32_t i = 0; i < batch_size; i++) {
        uint32_t dpu_id = i % num_dpus;
        dpus[dpu_id].pages_on_dpu++;
    }
    
    /* Count actually active DPUs (that will have pages) */
    uint32_t nr_active = (batch_size < num_dpus) ? batch_size : num_dpus;
    
    /* ETH contention model: real max speedup is 20.24× for 64 DPUs */
    double max_speedup = 20.24;  /* From ETH: 6.68 GB/s / 0.33 GB/s */
    double actual_speedup = (nr_active < max_speedup) ? nr_active : max_speedup;
    double effective_bandwidth = bandwidth_gbps * actual_speedup;
    
    /* Calculate transfer time for each DPU independently with effective bandwidth */
    double max_transfer_time = 0;
    for (uint32_t i = 0; i < num_dpus; i++) {
        if (dpus[i].pages_on_dpu > 0) {
            uint32_t dpu_size = dpus[i].pages_on_dpu * PAGE_SIZE;
            dpus[i].transfer_time_us = calc_transfer_time_us(dpu_size, effective_bandwidth);
            if (dpus[i].transfer_time_us > max_transfer_time) {
                max_transfer_time = dpus[i].transfer_time_us;
            }
        }
    }
    
    /* Total latency = kernel + mram + max(dpu_transfers) */
    double total_latency = KERNEL_OVERHEAD_US + MRAM_LATENCY_US + max_transfer_time;
    
    free(dpus);
    return total_latency;
}

/**
 * Compare sequential vs parallel latency models
 */
void compare_models(void)
{
    printf("\n=== COMPARING SEQUENTIAL vs PARALLEL LATENCY MODELS ===\n\n");
    
    uint32_t dpus[] = {1, 2, 4, 8, 16, 32, 64};
    uint32_t batch_sizes[] = {1, 10, 50};
    
    for (int b = 0; b < 3; b++) {
        uint32_t batch = batch_sizes[b];
        printf("Batch size: %u pages\n", batch);
        printf("DPUs | Sequential/Page | Parallel/Page | Speedup\n");
        printf("-----|-----------------|---------------|--------\n");
        
        for (int i = 0; i < 7; i++) {
            uint32_t n = dpus[i];
            
            /* Current (sequential) model */
            uint32_t total_size = batch * PAGE_SIZE;
            double seq_transfer = calc_transfer_time_us(total_size, HOST_WRITE_BANDWIDTH_GBS);
            double seq_latency = (KERNEL_OVERHEAD_US + MRAM_LATENCY_US + seq_transfer) / batch;
            
            /* Parallel model */
            double par_latency_total = simulate_parallel_batch_swapout(n, batch, 0);
            double par_latency = par_latency_total / batch;
            
            double speedup = seq_latency / par_latency;
            
            printf(" %2u  | %14.2f | %13.2f | %6.2f×\n", 
                   n, seq_latency, par_latency, speedup);
        }
        printf("\n");
    }
}

int main(void)
{
    compare_models();
    return 0;
}
