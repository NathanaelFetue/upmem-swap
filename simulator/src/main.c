#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include "config.h"
#include "memory_sim.h"
#include "page_table.h"
#include "upmem_swap.h"
#include "workload.h"
#include "stats.h"

/* ===== Main Program ===== */

static void print_usage(const char *prog)
{
    printf("Usage: %s [OPTIONS]\n", prog);
    printf("Options:\n");
    printf("  --ram-mb <N>           RAM size in MB (default: %u)\n", DEFAULT_RAM_SIZE_MB);
    printf("  --dpus <N>             Number of DPUs (default: %u)\n", DEFAULT_NR_DPUS);
    printf("  --accesses <N>         Total memory accesses (default: %u)\n", DEFAULT_NR_ACCESSES);
    printf("  --working-set <N>      Working set pages (default: %u)\n", DEFAULT_WORKING_SET);
    printf("  --workload <type>      Workload type: random|sequential|mixed (default: random)\n");
    printf("  --compress-mode <M>    Compression mode: none|cpu|dpu (default: none)\n");
    printf("  --output <file>        Output CSV (default: results/swap_sim.csv)\n");
    printf("  --help                 Show this help\n");
}

int main(int argc, char *argv[])
{
    /* Defaults */
    uint32_t ram_size_mb = DEFAULT_RAM_SIZE_MB;
    uint32_t nr_dpus = DEFAULT_NR_DPUS;
    uint32_t nr_accesses = DEFAULT_NR_ACCESSES;
    uint32_t working_set = DEFAULT_WORKING_SET;
    workload_type_t workload_type = WORKLOAD_RANDOM;
    compress_mode_t compress_mode = COMPRESS_NONE;
    const char *output_file = "results/swap_sim.csv";
    const char *pattern_str = "random";
    
    /* Parse arguments */
    struct option long_options[] = {
        {"ram-mb", required_argument, NULL, 'r'},
        {"dpus", required_argument, NULL, 'd'},
        {"accesses", required_argument, NULL, 'a'},
        {"working-set", required_argument, NULL, 'w'},
        {"workload", required_argument, NULL, 'l'},
        {"compress-mode", required_argument, NULL, 'm'},
        {"output", required_argument, NULL, 'o'},
        {"help", no_argument, NULL, 'h'},
        {NULL, 0, NULL, 0}
    };
    
    int opt;
    while ((opt = getopt_long(argc, argv, "r:d:a:w:l:m:o:h", long_options, NULL)) != -1) {
        switch (opt) {
            case 'r':
                ram_size_mb = atoi(optarg);
                break;
            case 'd':
                nr_dpus = atoi(optarg);
                break;
            case 'a':
                nr_accesses = atoi(optarg);
                break;
            case 'w':
                working_set = atoi(optarg);
                break;
            case 'l':
                if (strcmp(optarg, "random") == 0) {
                    workload_type = WORKLOAD_RANDOM;
                    pattern_str = "random";
                } else if (strcmp(optarg, "sequential") == 0) {
                    workload_type = WORKLOAD_SEQUENTIAL;
                    pattern_str = "sequential";
                } else if (strcmp(optarg, "mixed") == 0) {
                    workload_type = WORKLOAD_MIXED;
                    pattern_str = "mixed";
                } else {
                    fprintf(stderr, "Unknown workload: %s\n", optarg);
                    return 1;
                }
                break;
            case 'm':
                if (strcmp(optarg, "none") == 0) {
                    compress_mode = COMPRESS_NONE;
                } else if (strcmp(optarg, "cpu") == 0) {
                    compress_mode = COMPRESS_CPU;
                } else if (strcmp(optarg, "dpu") == 0) {
                    compress_mode = COMPRESS_DPU;
                } else {
                    fprintf(stderr, "Unknown compress mode: %s\n", optarg);
                    return 1;
                }
                break;
            case 'o':
                output_file = optarg;
                break;
            case 'h':
                print_usage(argv[0]);
                return 0;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }
    
    /* Validation */
    if (working_set > (ram_size_mb * 1024 * 1024) / PAGE_SIZE * 2) {
        fprintf(stderr, "Warning: working_set very large compared to RAM\n");
    }
    
    /* Print configuration */
    printf("\n");
    printf("UPMEM Swap Simulator - Initialization\n");
    printf("=====================================\n");
    printf("\n");
    printf("Configuration:\n");
    printf("  RAM size: %u MB (= %u pages)\n", 
           ram_size_mb, (ram_size_mb * 1024 * 1024) / PAGE_SIZE);
    printf("  DPUs: %u\n", nr_dpus);
    printf("  Working set: %u pages\n", working_set);
    printf("  Total accesses: %u\n", nr_accesses);
    printf("  Workload pattern: %s\n", pattern_str);
    printf("  Compression mode: %s\n", upmem_swap_mode_str(compress_mode));
    printf("  Output file: %s\n", output_file);
    printf("\n");
    
    /* Initialize components */
    printf("Initializing components...\n");
    
    ram_simulator_t *ram = ram_init(ram_size_mb);
    if (!ram) {
        fprintf(stderr, "Error initializing RAM simulator\n");
        return 1;
    }
    printf("  [OK] RAM simulator initialized\n");
    
    page_table_t *pt = page_table_init(working_set);
    if (!pt) {
        fprintf(stderr, "Error initializing page table\n");
        ram_destroy(ram);
        return 1;
    }
    printf("  [OK] Page table initialized\n");
    
    upmem_swap_manager_t *swap = upmem_swap_init(nr_dpus);
    if (!swap) {
        fprintf(stderr, "Error initializing UPMEM swap manager\n");
        page_table_destroy(pt);
        ram_destroy(ram);
        return 1;
    }
    upmem_swap_set_compress_mode(swap, compress_mode);
    printf("  [OK] UPMEM swap manager initialized\n");
    
    workload_simulator_t *wl = workload_init(ram, pt, swap, workload_type,
                                              nr_accesses, working_set);
    if (!wl) {
        fprintf(stderr, "Error initializing workload simulator\n");
        upmem_swap_destroy(swap);
        page_table_destroy(pt);
        ram_destroy(ram);
        return 1;
    }
    printf("  [OK] Workload simulator initialized\n");
    printf("\n");
    
    /* Run simulation */
    if (workload_run(wl) != 0) {
        fprintf(stderr, "Error running workload\n");
        workload_destroy(wl);
        upmem_swap_destroy(swap);
        page_table_destroy(pt);
        ram_destroy(ram);
        return 1;
    }
    
    /* Collect and print stats */
    printf("\n");
    printf("SIMULATION COMPLETE - RESULTS\n");
    printf("================================================\n");
    
    workload_print_results(wl);
    upmem_swap_stats_print(swap);
    ram_print_stats(ram);
    
    /* Prepare stats for export */
    swap_stats_t stats;
    stats.mode = upmem_swap_mode_str(compress_mode);
    stats.total_accesses = wl->page_hits + wl->page_faults;
    stats.page_faults = wl->page_faults;
    stats.page_hits = wl->page_hits;
    stats.swapouts = wl->swapouts;
    stats.swapins = wl->swapins;
    stats.avg_swapout_us = upmem_swap_get_avg_swapout_us(swap);
    stats.avg_swapin_us = upmem_swap_get_avg_swapin_us(swap);
    stats.avg_cpu_compress_us = upmem_swap_get_avg_cpu_compress_us(swap);
    stats.avg_cpu_decompress_us = upmem_swap_get_avg_cpu_decompress_us(swap);
    stats.avg_cpu_overhead_us = upmem_swap_get_avg_cpu_overhead_us(swap);
    stats.avg_dpu_compress_us = upmem_swap_get_avg_dpu_compress_us(swap);
    stats.compression_ratio = upmem_swap_get_compression_ratio(swap);
    stats.hit_rate = workload_get_hitrate(wl);
    stats.ram_mb = ram_size_mb;
    stats.nr_dpus = nr_dpus;
    stats.working_set = working_set;
    stats.pattern = pattern_str;
    
    stats_print(&stats);
    stats_export_csv(&stats, output_file);
    
    printf("\n");
    printf("Simulation Finished - Results exported to: %s\n", output_file);
    printf("\n");
    
    /* Cleanup */
    workload_destroy(wl);
    upmem_swap_destroy(swap);
    page_table_destroy(pt);
    ram_destroy(ram);
    
    return 0;
}
