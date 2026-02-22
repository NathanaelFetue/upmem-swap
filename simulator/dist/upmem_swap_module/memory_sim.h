#ifndef MEMORY_SIM_H
#define MEMORY_SIM_H

#include <stdint.h>
#include <stdbool.h>
#include "config.h"

/* ===== Simulateur RAM ===== */

typedef struct {
    uint8_t data[PAGE_SIZE];    /* Données de la page */
    bool occupied;              /* Page occupée? */
    uint32_t page_id;           /* ID of the page stored here */
} physical_frame_t;

typedef struct {
    physical_frame_t *frames;   /* Array of physical frames */
    uint32_t nr_frames;         /* Total frames */
    uint32_t nr_free;           /* Frames libres */
    uint32_t *free_list;        /* List of free frame indices */
    uint32_t free_list_head;    /* Index dans free_list */
} ram_simulator_t;

/* Initialise RAM simulée */
ram_simulator_t* ram_init(uint32_t size_mb);

/* Alloue une frame physique */
int ram_allocate_frame(ram_simulator_t *ram, uint32_t page_id);

/* Libère une frame physique */
void ram_free_frame(ram_simulator_t *ram, uint32_t frame_id);

/* Check si RAM est pleine */
bool ram_is_full(ram_simulator_t *ram);

/* Récupère pointeur vers data d'une frame */
void* ram_get_frame_data(ram_simulator_t *ram, uint32_t frame_id);

/* Récupère page_id stocké dans une frame */
uint32_t ram_get_frame_page_id(ram_simulator_t *ram, uint32_t frame_id);

/* Cleanup */
void ram_destroy(ram_simulator_t *ram);

/* Prints stats */
void ram_print_stats(ram_simulator_t *ram);

#endif /* MEMORY_SIM_H */
