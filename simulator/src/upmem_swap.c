#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <sys/time.h>
#include "upmem_swap.h"

/* ===== Implémentation: Gestionnaire UPMEM Swap ===== */

/* Helper: mesure latence accès UPMEM (simulation) */
static double simulate_upmem_latency_us(uint32_t size_bytes)
{
    /* 
     * Basé sur les benchmarks:
     * 4KB: ~25-31 µs
     * Approximation linéaire: ~6-7 µs/KB + overhead
     */
    double base_latency = 10.0;      /* Base overhead en µs */
    double per_kb_latency = 6.5;     /* µs par KB */
    double size_kb = size_bytes / 1024.0;
    double latency_us = base_latency + (size_kb * per_kb_latency);
    
    /* Ajoute petit bruit pour réalisme */
    latency_us += (rand() % 5) - 2;  /* ±2 µs */
    
    return latency_us;
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
    
    double latency_us = simulate_upmem_latency_us(PAGE_SIZE);
    
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
    
    double latency_us = simulate_upmem_latency_us(PAGE_SIZE);
    
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
