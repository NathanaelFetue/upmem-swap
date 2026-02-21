#ifndef SSD_BASELINE_H
#define SSD_BASELINE_H

#include <stdint.h>

/* 
 * SSD Baseline Models for comparison
 * 
 * Based on typical SSD/HDD characteristics from literature:
 * - SATA SSD: 50-150 µs avg
 * - NVMe: 10-30 µs avg
 * - HDD (SATA): 5-15 ms avg
 */

typedef enum {
    SSD_TYPE_SATA,      /* SATA SSDs (Samsung 870, Crucial MX, etc) */
    SSD_TYPE_NVME,      /* NVMe (PCIe) SSDs (Samsung 980, WD Black, etc) */
    SSD_TYPE_HDD        /* Traditional HDD (for reference) */
} ssd_type_t;

typedef struct {
    ssd_type_t type;
    double seek_time_us;
    double rotation_overhead_us; /* 0 for SSD, ~2-4ms for HDD */
    double transfer_latency_us;
    double kernel_overhead_us;
} ssd_baseline_t;

/* Get SSD baseline model */
ssd_baseline_t ssd_get_baseline(ssd_type_t type);

/* Calculate page fault latency for given SSD type */
double ssd_page_fault_latency_us(ssd_type_t type, uint32_t page_size);

/* Detailed breakdown of SSD latency components */
void ssd_print_breakdown(ssd_type_t type);

#endif
