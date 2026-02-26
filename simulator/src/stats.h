#ifndef STATS_H
#define STATS_H

#include <stdint.h>

/* ===== Collecteur Statistiques ===== */

typedef struct {
    const char *mode;
    uint64_t total_accesses;
    uint64_t page_hits;
    uint64_t page_faults;
    uint64_t swapouts;
    uint64_t swapins;
    double avg_swapout_us;
    double avg_swapin_us;
    double avg_cpu_overhead_us;
    double avg_dpu_compress_us;
    double compression_ratio;
    double hit_rate;
    uint32_t ram_mb;
    uint32_t nr_dpus;
    uint32_t working_set;
    const char *pattern;
} swap_stats_t;

/* Export CSV */
void stats_export_csv(swap_stats_t *stats, const char *filename);

/* Print pretty */
void stats_print(swap_stats_t *stats);

#endif /* STATS_H */
