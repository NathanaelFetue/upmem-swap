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

/* ===== Helpers: allocation and free-list management per DPU ===== */
static int allocate_block_from_dpu(dpu_swap_state_t *dpu, uint32_t size, uint64_t *offset_out)
{
    /* Try free list first (first-fit) */
    free_block_t *prev = NULL;
    free_block_t *b = dpu->free_list;
    while (b) {
        if (b->size >= size) {
            *offset_out = b->offset;
            if (b->size == size) {
                /* remove block */
                if (prev) prev->next = b->next;
                else dpu->free_list = b->next;
                free(b);
            } else {
                /* carve from head */
                b->offset += size;
                b->size -= size;
            }
            dpu->nr_pages_stored += 1;
            return 0;
        }
        prev = b;
        b = b->next;
    }

    /* Fallback: use free_offset if space */
    if (dpu->free_offset + size <= DPU_MRAM_SIZE) {
        *offset_out = dpu->free_offset;
        dpu->free_offset += size;
        dpu->nr_pages_stored += 1;
        return 0;
    }

    return -1; /* no space */
}

static void mark_space_free(upmem_swap_manager_t *mgr, uint32_t dpu_id, uint64_t offset, uint32_t size)
{
    if (!mgr || dpu_id >= mgr->nr_dpus) return;
    dpu_swap_state_t *dpu = &mgr->dpu_states[dpu_id];
    free_block_t *b = (free_block_t *)malloc(sizeof(free_block_t));
    if (!b) return;
    b->offset = offset;
    b->size = size;
    b->next = dpu->free_list;
    dpu->free_list = b;
    if (dpu->nr_pages_stored > 0) dpu->nr_pages_stored -= 1;
}

/* Find a DPU with space for one page; returns UINT32_MAX if none */
static uint32_t find_available_dpu(upmem_swap_manager_t *mgr)
{
    if (!mgr) return UINT32_MAX;
    uint32_t start = mgr->next_dpu;
    for (uint32_t i = 0; i < mgr->nr_dpus; i++) {
        uint32_t id = (start + i) % mgr->nr_dpus;
        dpu_swap_state_t *dpu = &mgr->dpu_states[id];
        /* Check free list or free_offset */
        if (dpu->free_list != NULL) return id;
        if (dpu->free_offset + PAGE_SIZE <= DPU_MRAM_SIZE) return id;
    }
    return UINT32_MAX;
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
        mgr->dpu_states[i].free_list = NULL;
        mgr->dpu_states[i].nr_pages_stored = 0;
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
    
    /* Find a DPU with space (search & skip full DPUs) */
    uint32_t dpu_id = find_available_dpu(mgr);
    if (dpu_id == UINT32_MAX) {
        fprintf(stderr, "Erreur: Tous les DPUs sont pleins\n");
        return -1;
    }
    dpu_swap_state_t *dpu = &mgr->dpu_states[dpu_id];
    
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
    
    /* Allocate space (from free-list or free_offset) */
    uint64_t alloc_offset = 0;
    if (allocate_block_from_dpu(dpu, PAGE_SIZE, &alloc_offset) != 0) {
        fprintf(stderr, "Erreur: impossible d'allouer espace sur DPU %u\n", dpu_id);
        return -1;
    }

    /* Update page entry */
    page->status = PAGE_IN_SWAP;
    page->dpu_id = dpu_id;
    page->dpu_offset = alloc_offset;

    /* Increment stats */
    mgr->total_swapouts++;
    mgr->total_swapout_time_us += total_us;

    /* Advance next_dpu to continue round-robin from next slot */
    mgr->next_dpu = (dpu_id + 1) % mgr->nr_dpus;
    
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
    /* Free MRAM space previously used by this page */
    mark_space_free(mgr, dpu_id, page->dpu_offset, PAGE_SIZE);
    
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
    if (mgr->dpu_states) {
        /* Free per-DPU free lists */
        for (uint32_t i = 0; i < mgr->nr_dpus; i++) {
            free_block_t *b = mgr->dpu_states[i].free_list;
            while (b) {
                free_block_t *n = b->next;
                free(b);
                b = n;
            }
        }
        free(mgr->dpu_states);
    }
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

/* ===== BATCH OPERATIONS (Optimization) ===== */

/*
 * Batch swap out: Transfer multiple pages in single operation
 * 
 * Performance model:
 *   Single page:  12 + 6 + 12.4 = 30.4 µs (overhead-per-page)
 *   Batch N pages: 12 + 6 + (12.4 * N) = 18 + 12.4N µs (amortized)
 *   
 *   Example: 10 pages
 *   - Sequential: 10 × 30 = 300 µs
 *   - Batch:     18 + 124 = 142 µs (2.1× speedup!)
 */
int upmem_swap_out_batch(upmem_swap_manager_t *mgr, 
                        page_entry_t **pages,
                        void **data, 
                        uint32_t count)
{
    if (!mgr || !pages || !data || count == 0) {
        return -1;
    }
    
    /* Validate all pages */
    for (uint32_t i = 0; i < count; i++) {
        if (!pages[i] || !data[i]) {
            fprintf(stderr, "Erreur batch: page %u invalide\n", i);
            return -1;
        }
    }
    
    struct timeval start, end;
    gettimeofday(&start, NULL);
    
    /* Calculate batch transfer latency
     * Batch reduces per-page overhead by amortizing kernel/setup cost
     */
    uint32_t total_size = count * PAGE_SIZE;
    
    /* Latency model for batch:
     * - Kernel overhead: 12 µs (paid once, not per-page)
     * - MRAM latency: 6 µs (per first page access)
     * - Transfer: depends on total size
     */
    double overhead = 12.0;  /* Paid once for whole batch */
    double mram_lat = upmem_mram_latency_us(PAGE_SIZE, 0);  /* First page */
    double batch_transfer_us = upmem_host_transfer_latency_us(total_size, 0);
    
    double latency_us = overhead + mram_lat + batch_transfer_us;
    
    /* Add jitter */
    double jitter = (latency_us * 0.05) * ((rand() % 101 - 50) / 50.0);
    double total_us = latency_us + jitter;
    
    gettimeofday(&end, NULL);
    
    /* Allocate space and update pages */
    uint32_t last_alloc = mgr->next_dpu;
    for (uint32_t i = 0; i < count; i++) {
        uint32_t dpu_id = find_available_dpu(mgr);
        if (dpu_id == UINT32_MAX) {
            fprintf(stderr, "Erreur batch: aucun DPU avec espace disponible\n");
            return -1;
        }
        dpu_swap_state_t *dpu = &mgr->dpu_states[dpu_id];

        uint64_t alloc_offset = 0;
        if (allocate_block_from_dpu(dpu, PAGE_SIZE, &alloc_offset) != 0) {
            fprintf(stderr, "Erreur batch: impossible d'allouer sur DPU %u\n", dpu_id);
            return -1;
        }

        pages[i]->status = PAGE_IN_SWAP;
        pages[i]->dpu_id = dpu_id;
        pages[i]->dpu_offset = alloc_offset;

        DEBUG_PRINT("Batch swap out: page %u → DPU %u", pages[i]->page_id, dpu_id);
        last_alloc = dpu_id;
    }

    /* Update next_dpu for round-robin after batch */
    mgr->next_dpu = (last_alloc + 1) % mgr->nr_dpus;
    
    /* Statistics */
    mgr->batch_swapouts++;
    mgr->total_batch_swapout_time_us += total_us;
    
    DEBUG_PRINT("Batch swap out: %u pages in %.2f µs", count, total_us);
    
    return 0;
}

/*
 * Batch swap in: Transfer multiple pages from MRAM back to RAM
 */
int upmem_swap_in_batch(upmem_swap_manager_t *mgr,
                       page_entry_t **pages,
                       void **data,
                       uint32_t count)
{
    if (!mgr || !pages || !data || count == 0) {
        return -1;
    }
    
    /* Validate all pages */
    for (uint32_t i = 0; i < count; i++) {
        if (!pages[i] || !data[i]) {
            fprintf(stderr, "Erreur batch: page %u invalide\n", i);
            return -1;
        }
        if (pages[i]->status != PAGE_IN_SWAP) {
            fprintf(stderr, "Erreur batch: page %u not in SWAP\n", pages[i]->page_id);
            return -1;
        }
    }
    
    struct timeval start, end;
    gettimeofday(&start, NULL);
    
    /* Calculate batch transfer latency for read */
    uint32_t total_size = count * PAGE_SIZE;
    
    double overhead = 12.0;  /* Paid once */
    double mram_lat = upmem_mram_latency_us(PAGE_SIZE, 1);  /* First page read */
    double batch_transfer_us = upmem_host_transfer_latency_us(total_size, 1);
    
    double latency_us = overhead + mram_lat + batch_transfer_us;
    
    /* Add jitter */
    double jitter = (latency_us * 0.05) * ((rand() % 101 - 50) / 50.0);
    double total_us = latency_us + jitter;
    
    gettimeofday(&end, NULL);
    
    /* Update pages */
    for (uint32_t i = 0; i < count; i++) {
        pages[i]->status = PAGE_IN_RAM;
        /* Reclaim MRAM space for each page */
        mark_space_free(mgr, pages[i]->dpu_id, pages[i]->dpu_offset, PAGE_SIZE);
        DEBUG_PRINT("Batch swap in: page %u ← DPU %u", 
                   pages[i]->page_id, pages[i]->dpu_id);
    }
    
    /* Statistics */
    mgr->batch_swapins++;
    mgr->total_batch_swapin_time_us += total_us;
    
    DEBUG_PRINT("Batch swap in: %u pages in %.2f µs", count, total_us);
    
    return 0;
}
