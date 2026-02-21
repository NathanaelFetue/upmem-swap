#ifndef WORKLOAD_H
#define WORKLOAD_H

#include <stdint.h>
#include "config.h"
#include "memory_sim.h"
#include "page_table.h"
#include "upmem_swap.h"

/* ===== Générateur de Workload ===== */

typedef struct {
    ram_simulator_t *ram;
    page_table_t *pt;
    upmem_swap_manager_t *swap;
    
    workload_type_t type;
    uint32_t nr_accesses;
    uint32_t working_set_size;
    
    /* Statistiques */
    uint64_t page_hits;
    uint64_t page_faults;
    uint64_t swapouts;
    uint64_t swapins;
    double total_fault_time_us;
    
    /* Pour patterns */
    uint32_t sequential_index;
} workload_simulator_t;

/* Initialise workload */
workload_simulator_t* workload_init(ram_simulator_t *ram, page_table_t *pt,
                                    upmem_swap_manager_t *swap,
                                    workload_type_t type,
                                    uint32_t nr_accesses,
                                    uint32_t working_set_size);

/* Lance simulation de workload */
int workload_run(workload_simulator_t *wl);

/* Accès une page (gère page faults automatiquement) */
int workload_access_page(workload_simulator_t *wl, uint32_t page_id);

/* Print résultats */
void workload_print_results(workload_simulator_t *wl);

/* Cleanup */
void workload_destroy(workload_simulator_t *wl);

/* Get hitrate */
double workload_get_hitrate(workload_simulator_t *wl);

#endif /* WORKLOAD_H */
