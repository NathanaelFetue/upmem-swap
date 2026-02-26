#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "stats.h"
#include "ssd_baseline.h"

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
    fprintf(f, "mode,nr_dpus,ram_mb,working_set,pattern,total_accesses,page_faults,");
    fprintf(f, "swapouts,swapins,avg_swapout_us,avg_swapin_us,avg_cpu_overhead_us,avg_dpu_compress_us,compression_ratio,hit_rate\n");
    
    /* Data */
        fprintf(f, "%s,%u,%u,%u,%s,%lu,%lu,%lu,%lu,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f\n",
            stats->mode ? stats->mode : "none",
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
            stats->avg_cpu_overhead_us,
            stats->avg_dpu_compress_us,
            stats->compression_ratio,
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
    printf("  Compression mode: %s\n", stats->mode ? stats->mode : "none");
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
    printf("  Avg CPU compression overhead: %.2f µs\n", stats->avg_cpu_overhead_us);
    printf("  Avg DPU compression overhead (estimated): %.2f µs\n", stats->avg_dpu_compress_us);
    printf("  Compression ratio (raw/stored): %.2f\n", stats->compression_ratio);
    printf("\n");
    
    printf("Comparison:\n");
    double upmem_avg = (stats->avg_swapout_us + stats->avg_swapin_us) / 2.0;
    printf("  UPMEM average: %.2f µs\n", upmem_avg);
    
    double ssd_sata = ssd_page_fault_latency_us(SSD_TYPE_SATA, 4096);
    double ssd_nvme = ssd_page_fault_latency_us(SSD_TYPE_NVME, 4096);
    double ssd_hdd = ssd_page_fault_latency_us(SSD_TYPE_HDD, 4096);
    
    printf("  SSD SATA: %.2f µs (speedup: %.2f×)\n", ssd_sata, ssd_sata / upmem_avg);
    printf("  SSD NVMe: %.2f µs (speedup: %.2f×)\n", ssd_nvme, ssd_nvme / upmem_avg);
    printf("  HDD 7200: %.2f µs (speedup: %.2f×)\n", ssd_hdd, ssd_hdd / upmem_avg);
    printf("\n");
    
    printf("Breakdown (ETH Zürich model):\n");
    ssd_print_breakdown(SSD_TYPE_SATA);
    printf("\n");
}
