#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <sys/time.h>
#include "page_table.h"

/* ===== Implémentation: Table de Pages + LRU ===== */

page_table_t* page_table_init(uint32_t nr_pages)
{
    page_table_t *pt = (page_table_t *)malloc(sizeof(page_table_t));
    if (!pt) {
        fprintf(stderr, "Erreur allocation page_table_t\n");
        return NULL;
    }
    
    pt->nr_pages = nr_pages;
    pt->entries = (page_entry_t *)calloc(nr_pages, sizeof(page_entry_t));
    if (!pt->entries) {
        fprintf(stderr, "Erreur allocation entries\n");
        free(pt);
        return NULL;
    }
    
    /* Initialise toutes les entries */
    for (uint32_t i = 0; i < nr_pages; i++) {
        pt->entries[i].page_id = i;
        pt->entries[i].status = PAGE_EMPTY;
        pt->entries[i].frame_id = 0;
        pt->entries[i].dpu_id = 0;
        pt->entries[i].dpu_offset = 0;
        pt->entries[i].last_access_time = 0;
    }
    
    DEBUG_PRINT("Page table initialisée: %u pages", nr_pages);
    
    return pt;
}

page_entry_t* page_table_lookup(page_table_t *pt, uint32_t page_id)
{
    if (!pt || page_id >= pt->nr_pages) {
        return NULL;
    }
    
    return &pt->entries[page_id];
}

/* Helper: current time en microsecondes */
static uint64_t get_current_time_us(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000000 + tv.tv_usec;
}

uint32_t page_table_select_victim_lru(page_table_t *pt)
{
    if (!pt || pt->nr_pages == 0) {
        return 0;
    }
    
    uint32_t victim_id = 0;
    uint64_t min_time = UINT64_MAX;
    
    /* Cherche page IN_RAM avec plus ancien timestamp */
    for (uint32_t i = 0; i < pt->nr_pages; i++) {
        if (pt->entries[i].status == PAGE_IN_RAM) {
            if (pt->entries[i].last_access_time < min_time) {
                min_time = pt->entries[i].last_access_time;
                victim_id = i;
            }
        }
    }
    
    DEBUG_PRINT("LRU victim selected: page %u (time=%lu)", victim_id, min_time);
    
    return victim_id;
}

void page_table_update_access(page_table_t *pt, uint32_t page_id)
{
    if (!pt || page_id >= pt->nr_pages) {
        return;
    }
    
    pt->entries[page_id].last_access_time = get_current_time_us();
}

void page_table_update_page(page_table_t *pt, uint32_t page_id,
                            page_status_t status, uint32_t frame_id,
                            uint32_t dpu_id, uint64_t dpu_offset)
{
    if (!pt || page_id >= pt->nr_pages) {
        return;
    }
    
    pt->entries[page_id].status = status;
    pt->entries[page_id].frame_id = frame_id;
    pt->entries[page_id].dpu_id = dpu_id;
    pt->entries[page_id].dpu_offset = dpu_offset;
    pt->entries[page_id].last_access_time = get_current_time_us();
    
    DEBUG_PRINT("Page %u updated: status=%d", page_id, status);
}

uint32_t page_table_count_in_ram(page_table_t *pt)
{
    if (!pt) return 0;
    
    uint32_t count = 0;
    for (uint32_t i = 0; i < pt->nr_pages; i++) {
        if (pt->entries[i].status == PAGE_IN_RAM) {
            count++;
        }
    }
    return count;
}

uint32_t page_table_count_in_swap(page_table_t *pt)
{
    if (!pt) return 0;
    
    uint32_t count = 0;
    for (uint32_t i = 0; i < pt->nr_pages; i++) {
        if (pt->entries[i].status == PAGE_IN_SWAP) {
            count++;
        }
    }
    return count;
}

void page_table_destroy(page_table_t *pt)
{
    if (!pt) return;
    
    if (pt->entries) free(pt->entries);
    free(pt);
}

void page_table_print_entry(page_entry_t *entry)
{
    if (!entry) return;
    
    printf("Page %u: status=%d, frame=%u, dpu=%u, offset=%lu, last_access=%lu\n",
           entry->page_id, entry->status, entry->frame_id,
           entry->dpu_id, entry->dpu_offset, entry->last_access_time);
}
