#ifndef UPMEM_SWAP_H
#define UPMEM_SWAP_H

#include <stdint.h>
#include <sys/types.h>
#include "config.h"
#include "page_table.h"

/* ===== Gestionnaire UPMEM Swap ===== */

typedef struct {
    uint32_t dpu_id;
    uint64_t free_offset;       /* Prochain offset libre dans MRAM */
} dpu_swap_state_t;

typedef struct {
    /* DPU set (simulé) */
    uint32_t nr_dpus;
    dpu_swap_state_t *dpu_states;
    uint32_t next_dpu;          /* Round-robin allocation */
    
    /* Statistiques */
    uint64_t total_swapouts;
    uint64_t total_swapins;
    double total_swapout_time_us;
    double total_swapin_time_us;
} upmem_swap_manager_t;

/* Initialise gestionnaire swap */
upmem_swap_manager_t* upmem_swap_init(uint32_t nr_dpus);

/* Swap out: RAM → DPU MRAM (mesure latence) */
int upmem_swap_out(upmem_swap_manager_t *mgr, page_entry_t *page,
                   void *data, uint32_t data_size);

/* Swap in: DPU MRAM → RAM (mesure latence) */
int upmem_swap_in(upmem_swap_manager_t *mgr, page_entry_t *page,
                  void *data, uint32_t data_size);

/* Print statistiques */
void upmem_swap_stats_print(upmem_swap_manager_t *mgr);

/* Cleanup */
void upmem_swap_destroy(upmem_swap_manager_t *mgr);

/* Get moyenne latence swap out */
double upmem_swap_get_avg_swapout_us(upmem_swap_manager_t *mgr);

/* Get moyenne latence swap in */
double upmem_swap_get_avg_swapin_us(upmem_swap_manager_t *mgr);

#endif /* UPMEM_SWAP_H */
