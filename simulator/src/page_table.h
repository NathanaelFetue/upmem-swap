#ifndef PAGE_TABLE_H
#define PAGE_TABLE_H

#include <stdint.h>
#include <stdbool.h>
#include "config.h"

/* ===== Table de Pages + LRU ===== */

typedef enum {
    PAGE_EMPTY,         /* Page not allocated */
    PAGE_IN_RAM,        /* Page en RAM */
    PAGE_IN_SWAP        /* Page en SWAP (UPMEM) */
} page_status_t;

typedef struct {
    uint32_t page_id;
    page_status_t status;
    uint32_t frame_id;          /* Si PAGE_IN_RAM: frame ID */
    uint32_t dpu_id;            /* Si PAGE_IN_SWAP: DPU ID */
    uint64_t dpu_offset;        /* Si PAGE_IN_SWAP: offset dans MRAM */
    uint32_t swap_size;         /* Taille stockée en swap (compressée ou brute) */
    uint64_t last_access_time;  /* Ts pour LRU (microseconde) */
} page_entry_t;

typedef struct {
    page_entry_t *entries;      /* Array of page entries */
    uint32_t nr_pages;          /* Total pages possibles */
} page_table_t;

/* Initialise table de pages */
page_table_t* page_table_init(uint32_t nr_pages);

/* Lookup une page dans la table */
page_entry_t* page_table_lookup(page_table_t *pt, uint32_t page_id);

/* Sélectionne victim LRU (retourne page_id) */
uint32_t page_table_select_victim_lru(page_table_t *pt);

/* Met à jour timestamp d'accès LRU */
void page_table_update_access(page_table_t *pt, uint32_t page_id);

/* Update page après swap */
void page_table_update_page(page_table_t *pt, uint32_t page_id,
                            page_status_t status, uint32_t frame_id,
                            uint32_t dpu_id, uint64_t dpu_offset);

/* Check combien de pages en RAM */
uint32_t page_table_count_in_ram(page_table_t *pt);

/* Check combien de pages en SWAP */
uint32_t page_table_count_in_swap(page_table_t *pt);

/* Cleanup */
void page_table_destroy(page_table_t *pt);

/* Debug print */
void page_table_print_entry(page_entry_t *entry);

#endif /* PAGE_TABLE_H */
