#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "stats.h"

/* ===== Implémentation: Collecteur Statistiques ===== */

void stats_export_csv(swap_stats_t *stats, const char *filename)
{
    if (!stats || !filename) return;
    
    FILE *f = fopen(filename, "w");
    if (!f) {
        fprintf(stderr, "Erreur ouverture %s\n", filename);
        return;
    }
    
    /* Header */
    fprintf(f, "nr_dpus,ram_mb,working_set,pattern,total_accesses,page_faults,");
    fprintf(f, "swapouts,swapins,avg_swapout_us,avg_swapin_us,hit_rate\n");
    
    /* Data */
    fprintf(f, "%u,%u,%u,%s,%lu,%lu,%lu,%lu,%.2f,%.2f,%.2f\n",
            stats->nr_dpus,
            stats->ram_mb,
            stats->working_set,
            stats->pattern ? stats->pattern : "unknown",
            stats->total_accesses,
            stats->page_faults,
            stats->swapouts,
            stats->swapins,
            stats->avg_swapout_us,
            stats->avg_swapin_us,
            stats->hit_rate);
    
    fclose(f);
    printf("Results exported to: %s\n", filename);
}

void stats_print(swap_stats_t *stats)
{
    if (!stats) return;
    
    printf("\n");
    printf("UPMEM Swap Simulator - Results\n");
    printf("================================================\n");
    printf("\n");
    
    printf("Configuration:\n");
    printf("  RAM size: %u MB\n", stats->ram_mb);
    printf("  DPUs: %u\n", stats->nr_dpus);
    printf("  Working set: %u pages\n", stats->working_set);
    printf("  Workload pattern: %s\n", stats->pattern ? stats->pattern : "unknown");
    printf("\n");
    
    printf("Results:\n");
    printf("  Total accesses: %lu\n", stats->total_accesses);
    printf("  Page hits: %lu\n", stats->total_accesses - stats->page_faults);
    printf("  Page faults: %lu\n", stats->page_faults);
    printf("  Hit rate: %.2f%%\n", stats->hit_rate);
    printf("\n");
    
    printf("Swap Operations:\n");
    printf("  Swap outs: %lu\n", stats->swapouts);
    printf("  Swap ins: %lu\n", stats->swapins);
    printf("  Avg swap out latency: %.2f µs\n", stats->avg_swapout_us);
    printf("  Avg swap in latency: %.2f µs\n", stats->avg_swapin_us);
    printf("\n");
    
    printf("Comparison:\n");
    printf("  UPMEM avg: %.2f µs\n", (stats->avg_swapout_us + stats->avg_swapin_us) / 2.0);
    printf("  SSD (literature): 60-200 µs\n");
    printf("  zram (literature): 20-50 µs\n");
    printf("  InfiniSwap (RDMA): ~30 µs\n");
    printf("\n");
    
    double speedup_vs_ssd = 100.0 / ((stats->avg_swapout_us + stats->avg_swapin_us) / 2.0);
    printf("  Speedup vs SSD (100 µs baseline): %.2f×\n", speedup_vs_ssd);
    printf("\n");
}
