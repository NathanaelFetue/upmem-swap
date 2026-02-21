#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "memory_sim.h"

/* ===== Implémentation: Simulateur RAM ===== */

ram_simulator_t* ram_init(uint32_t size_mb)
{
    ram_simulator_t *ram = (ram_simulator_t *)malloc(sizeof(ram_simulator_t));
    if (!ram) {
        fprintf(stderr, "Erreur allocation ram_simulator_t\n");
        return NULL;
    }
    
    uint32_t total_pages = (size_mb * 1024 * 1024) / PAGE_SIZE;
    ram->nr_frames = total_pages;
    ram->nr_free = total_pages;
    
    ram->frames = (physical_frame_t *)calloc(total_pages, sizeof(physical_frame_t));
    if (!ram->frames) {
        fprintf(stderr, "Erreur allocation frames\n");
        free(ram);
        return NULL;
    }
    
    /* Initialise free_list */
    ram->free_list = (uint32_t *)malloc(total_pages * sizeof(uint32_t));
    if (!ram->free_list) {
        fprintf(stderr, "Erreur allocation free_list\n");
        free(ram->frames);
        free(ram);
        return NULL;
    }
    
    for (uint32_t i = 0; i < total_pages; i++) {
        ram->free_list[i] = i;
    }
    ram->free_list_head = 0;
    
    DEBUG_PRINT("RAM initialisée: %u frames, %u MB total", total_pages, size_mb);
    
    return ram;
}

int ram_allocate_frame(ram_simulator_t *ram, uint32_t page_id)
{
    if (!ram || ram->nr_free == 0) {
        return -1; /* RAM pleine */
    }
    
    /* Pop from free_list */
    uint32_t frame_id = ram->free_list[ram->free_list_head];
    ram->free_list_head++;
    ram->nr_free--;
    
    /* Mark frame as occupied */
    ram->frames[frame_id].occupied = true;
    ram->frames[frame_id].page_id = page_id;
    memset(ram->frames[frame_id].data, 0, PAGE_SIZE);
    
    DEBUG_PRINT("Frame %u allouée pour page %u", frame_id, page_id);
    
    return frame_id;
}

void ram_free_frame(ram_simulator_t *ram, uint32_t frame_id)
{
    if (!ram || frame_id >= ram->nr_frames) {
        return;
    }
    
    if (!ram->frames[frame_id].occupied) {
        return; /* Already free */
    }
    
    /* Mark as free */
    ram->frames[frame_id].occupied = false;
    ram->frames[frame_id].page_id = 0;
    
    /* Push back to free_list */
    if (ram->free_list_head > 0) {
        ram->free_list_head--;
        ram->free_list[ram->free_list_head] = frame_id;
        ram->nr_free++;
    }
    
    DEBUG_PRINT("Frame %u libérée", frame_id);
}

bool ram_is_full(ram_simulator_t *ram)
{
    return (ram && ram->nr_free == 0);
}

void* ram_get_frame_data(ram_simulator_t *ram, uint32_t frame_id)
{
    if (!ram || frame_id >= ram->nr_frames) {
        return NULL;
    }
    
    if (!ram->frames[frame_id].occupied) {
        return NULL;
    }
    
    return (void *)ram->frames[frame_id].data;
}

uint32_t ram_get_frame_page_id(ram_simulator_t *ram, uint32_t frame_id)
{
    if (!ram || frame_id >= ram->nr_frames) {
        return 0;
    }
    
    return ram->frames[frame_id].page_id;
}

void ram_destroy(ram_simulator_t *ram)
{
    if (!ram) return;
    
    if (ram->frames) free(ram->frames);
    if (ram->free_list) free(ram->free_list);
    free(ram);
}

void ram_print_stats(ram_simulator_t *ram)
{
    if (!ram) return;
    
    uint32_t used = ram->nr_frames - ram->nr_free;
    
    printf("\n=== RAM Simulator Stats ===\n");
    printf("Total frames: %u\n", ram->nr_frames);
    printf("Used frames: %u\n", used);
    printf("Free frames: %u\n", ram->nr_free);
    printf("Utilization: %.2f%%\n", 100.0 * used / ram->nr_frames);
}
