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
    /* Free list for reclaimed MRAM blocks (offset, size) */
    struct free_block *free_list;
    uint32_t nr_pages_stored;
} dpu_swap_state_t;

/* Free block descriptor for MRAM per-DPU free list */
typedef struct free_block {
    uint64_t offset;
    uint32_t size;
    struct free_block *next;
} free_block_t;

typedef enum {
    COMPRESS_NONE = 0,
    COMPRESS_CPU = 1,
    COMPRESS_DPU = 2
} compress_mode_t;

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

    compress_mode_t compress_mode;
    double total_cpu_compress_us;
    double total_dpu_compress_us;
    uint64_t total_bytes_raw;
    uint64_t total_bytes_stored;
    
    /* Batch statistics */
    uint64_t batch_swapouts;
    uint64_t batch_swapins;
    double total_batch_swapout_time_us;
    double total_batch_swapin_time_us;
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

/* Batch swap out: Multiple pages in single operation */
int upmem_swap_out_batch(upmem_swap_manager_t *mgr, 
                        page_entry_t **pages,
                        void **data, 
                        uint32_t count);

/* Batch swap in: Multiple pages in single operation */
int upmem_swap_in_batch(upmem_swap_manager_t *mgr,
                       page_entry_t **pages,
                       void **data,
                       uint32_t count);

void upmem_swap_set_compress_mode(upmem_swap_manager_t *mgr, compress_mode_t mode);
const char* upmem_swap_mode_str(compress_mode_t mode);
double upmem_swap_get_avg_cpu_overhead_us(upmem_swap_manager_t *mgr);
double upmem_swap_get_avg_dpu_compress_us(upmem_swap_manager_t *mgr);
double upmem_swap_get_compression_ratio(upmem_swap_manager_t *mgr);

#endif /* UPMEM_SWAP_H */
