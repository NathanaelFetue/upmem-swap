#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <sys/time.h>
#include "upmem_swap.h"

/* ===== Implémentation: Gestionnaire UPMEM Swap ===== */

/* 
 * Latence UPMEM basée sur ETH Zürich paper:
 * 
 * Source: "Benchmarking a New Paradigm: An Experimental Analysis of a Real
 * Processing-in-Memory Architecture" (Gómez-Luna et al., ETH Zürich)
 * 
 * Modèle MRAM interne (cycles):
 *   Read:  α=77 + β×size, où β=0.5 cycles/B @ 350 MHz
 *   Write: α=61 + β×size, où β=0.5 cycles/B @ 350 MHz
 * 
 * Latence HOST↔DPU (mesuré):
 *   HOST→DPU (write): 0.33 GB/s bandwidth = 12.4 µs pour 4KB
 *   DPU→HOST (read):  0.12 GB/s bandwidth = 34.1 µs pour 4KB (asymétrique)
 * 
 * Pour un swap, compter:
 *   1. MRAM internal latency (6-7 µs)
 *   2. HOST-DPU transfer (12.4 ou 34.1 µs)
 *   3. Overhead kernel (~10-15 µs) - voir kernel_overhead_us()
 */

static double upmem_kernel_overhead_us(void)
{
    /* 
     * Overhead for complete page fault:
     * - Hardware exception: ~1.4 µs
     * - Context save/restore: X2 ~6 µs
     * - Page table lookup: ~1.4 µs
     * - Swap identification: ~0.3 µs
     * - Interrupt handling: ~3 µs
     * Total: ~12 µs constant overhead
     */
    return 12.0;
}

static double upmem_mram_latency_us(uint32_t size_bytes, int is_read)
{
    /* 
     * MRAM internal latency model (ETH paper, Fig. 3.2.1):
     * Latency(cycles) = alpha + beta * size
     * 
     * @ 350 MHz frequency:
     * - read:  alpha=77, beta=0.5 cycles/B
     * - write: alpha=61, beta=0.5 cycles/B
     * 
     * Conversion: 1 cycle @ 350 MHz = 1/(350*1e6) seconds = 1/350 µs
     */
    const double freq_mhz = 350.0;
    const double alpha_read = 77.0;
    const double alpha_write = 61.0;
    const double beta = 0.5;  /* cycles per byte */
    
    double alpha = is_read ? alpha_read : alpha_write;
    double latency_cycles = alpha + (beta * size_bytes);
    
    /* cycles to µs: cycles / (freq_mhz) */
    return latency_cycles / freq_mhz;
}

static double upmem_host_transfer_latency_us(uint32_t size_bytes, int is_read)
{
    /* 
     * HOST↔DPU bandwidth measured (ETH paper, page 14, section 3.3):
     * - HOST→DPU (write): 0.33 GB/s (uses AVX writes, async)
     * - DPU→HOST (read):  0.12 GB/s (uses AVX reads, sync - slower!)
     * 
     * Asymmetry (3x difference) explained: reads are synchronous,
     * forcing CPU to wait. Writes are asynchronous (fire-and-forget).
     */
    double bandwidth_gbs = is_read ? 0.12 : 0.33;
    double size_gb = size_bytes / (1024.0 * 1024.0 * 1024.0);
    
    return (size_gb / bandwidth_gbs) * 1e6;  /* seconds → µs */
}

static double simulate_upmem_latency_us(uint32_t size_bytes, int is_read)
{
    /* 
     * Simplified model combining all components:
     * Total = kernel_overhead + mram_latency + host_transfer
     */
    double overhead = upmem_kernel_overhead_us();
    double mram_lat = upmem_mram_latency_us(size_bytes, is_read);
    double host_lat = upmem_host_transfer_latency_us(size_bytes, is_read);
    
    double total = overhead + mram_lat + host_lat;
    
    /* Add realistic jitter (±5% variation) */
    double jitter = (total * 0.05) * ((rand() % 101 - 50) / 50.0);
    
    return total + jitter;
}

upmem_swap_manager_t* upmem_swap_init(uint32_t nr_dpus)
{
    upmem_swap_manager_t *mgr = (upmem_swap_manager_t *)malloc(sizeof(upmem_swap_manager_t));
    if (!mgr) {
        fprintf(stderr, "Erreur allocation upmem_swap_manager_t\n");
        return NULL;
    }
    
    mgr->nr_dpus = nr_dpus;
    mgr->next_dpu = 0;
    mgr->total_swapouts = 0;
    mgr->total_swapins = 0;
    mgr->total_swapout_time_us = 0.0;
    mgr->total_swapin_time_us = 0.0;
    
    mgr->dpu_states = (dpu_swap_state_t *)malloc(nr_dpus * sizeof(dpu_swap_state_t));
    if (!mgr->dpu_states) {
        fprintf(stderr, "Erreur allocation dpu_states\n");
        free(mgr);
        return NULL;
    }
    
    /* Initialise DPU states */
    for (uint32_t i = 0; i < nr_dpus; i++) {
        mgr->dpu_states[i].dpu_id = i;
        mgr->dpu_states[i].free_offset = 0;
    }
    
    DEBUG_PRINT("UPMEM Swap Manager initialisé: %u DPUs", nr_dpus);
    
    return mgr;
}

int upmem_swap_out(upmem_swap_manager_t *mgr, page_entry_t *page,
                   void *data, uint32_t data_size)
{
    if (!mgr || !page || !data) {
        return -1;
    }
    
    if (data_size != PAGE_SIZE) {
        fprintf(stderr, "Erreur: data_size (%u) != PAGE_SIZE (%u)\n", data_size, PAGE_SIZE);
        return -1;
    }
    
    /* Sélectionne DPU en round-robin */
    uint32_t dpu_id = mgr->next_dpu;
    mgr->next_dpu = (mgr->next_dpu + 1) % mgr->nr_dpus;
    
    dpu_swap_state_t *dpu = &mgr->dpu_states[dpu_id];
    
    /* Check si espace disponible */
    if (dpu->free_offset + PAGE_SIZE > DPU_MRAM_SIZE) {
        fprintf(stderr, "Erreur: MRAM plein sur DPU %u\n", dpu_id);
        return -1;
    }
    
    /* Simule transfer + mesure latence */
    struct timeval start, end;
    gettimeofday(&start, NULL);
    
    double latency_us = simulate_upmem_latency_us(PAGE_SIZE, 0);  /* write=0 (CPU→MRAM) */
    
    /* Simule le transfer (en réalité: dpu_prepare_xfer + dpu_push_xfer) */
    /* Pas de memcpy réel ici */
    
    gettimeofday(&end, NULL);
    double actual_us = (end.tv_sec - start.tv_sec) * 1000000.0 +
                       (end.tv_usec - start.tv_usec);
    
    /* Prend le max pour compter la latence simulée */
    double total_us = (actual_us < latency_us) ? latency_us : actual_us;
    
    /* Update page entry */
    page->status = PAGE_IN_SWAP;
    page->dpu_id = dpu_id;
    page->dpu_offset = dpu->free_offset;
    
    /* Increment stats */
    mgr->total_swapouts++;
    mgr->total_swapout_time_us += total_us;
    dpu->free_offset += PAGE_SIZE;
    
    DEBUG_PRINT("Swap out: page %u → DPU %u offset %lu (latency %.2f µs)",
                page->page_id, dpu_id, dpu->free_offset - PAGE_SIZE, total_us);
    
    return 0;
}

int upmem_swap_in(upmem_swap_manager_t *mgr, page_entry_t *page,
                  void *data, uint32_t data_size)
{
    if (!mgr || !page || !data) {
        return -1;
    }
    
    if (data_size != PAGE_SIZE) {
        fprintf(stderr, "Erreur: data_size (%u) != PAGE_SIZE (%u)\n", data_size, PAGE_SIZE);
        return -1;
    }
    
    if (page->status != PAGE_IN_SWAP) {
        fprintf(stderr, "Erreur: page non en SWAP (status=%d)\n", page->status);
        return -1;
    }
    
    uint32_t dpu_id = page->dpu_id;
    if (dpu_id >= mgr->nr_dpus) {
        fprintf(stderr, "Erreur: DPU ID invalide %u\n", dpu_id);
        return -1;
    }
    
    /* Simule transfer + mesure latence */
    struct timeval start, end;
    gettimeofday(&start, NULL);
    
    double latency_us = simulate_upmem_latency_us(PAGE_SIZE, 1);  /* read=1 (MRAM→CPU) */
    
    /* Simule le transfer (en réalité: dpu_prepare_xfer + dpu_push_xfer) */
    /* Transfer simulé */
    
    gettimeofday(&end, NULL);
    double actual_us = (end.tv_sec - start.tv_sec) * 1000000.0 +
                       (end.tv_usec - start.tv_usec);
    
    /* Prend le max pour la latence */
    double total_us = (actual_us < latency_us) ? latency_us : actual_us;
    
    /* Update page entry */
    page->status = PAGE_IN_RAM;
    
    /* Increment stats */
    mgr->total_swapins++;
    mgr->total_swapin_time_us += total_us;
    
    DEBUG_PRINT("Swap in: page %u ← DPU %u offset %lu (latency %.2f µs)",
                page->page_id, dpu_id, page->dpu_offset, total_us);
    
    return 0;
}

void upmem_swap_stats_print(upmem_swap_manager_t *mgr)
{
    if (!mgr) return;
    
    double avg_out = 0.0, avg_in = 0.0;
    
    if (mgr->total_swapouts > 0) {
        avg_out = mgr->total_swapout_time_us / mgr->total_swapouts;
    }
    if (mgr->total_swapins > 0) {
        avg_in = mgr->total_swapin_time_us / mgr->total_swapins;
    }
    
    printf("\n=== UPMEM Swap Manager Stats ===\n");
    printf("Nr DPUs: %u\n", mgr->nr_dpus);
    printf("Total swap outs: %lu\n", mgr->total_swapouts);
    printf("Total swap ins: %lu\n", mgr->total_swapins);
    printf("Avg swap out latency: %.2f µs\n", avg_out);
    printf("Avg swap in latency: %.2f µs\n", avg_in);
    printf("Total swap out time: %.2f ms\n", mgr->total_swapout_time_us / 1000.0);
    printf("Total swap in time: %.2f ms\n", mgr->total_swapin_time_us / 1000.0);
}

void upmem_swap_destroy(upmem_swap_manager_t *mgr)
{
    if (!mgr) return;
    
    if (mgr->dpu_states) free(mgr->dpu_states);
    free(mgr);
}

double upmem_swap_get_avg_swapout_us(upmem_swap_manager_t *mgr)
{
    if (!mgr || mgr->total_swapouts == 0) {
        return 0.0;
    }
    return mgr->total_swapout_time_us / mgr->total_swapouts;
}

double upmem_swap_get_avg_swapin_us(upmem_swap_manager_t *mgr)
{
    if (!mgr || mgr->total_swapins == 0) {
        return 0.0;
    }
    return mgr->total_swapin_time_us / mgr->total_swapins;
}
