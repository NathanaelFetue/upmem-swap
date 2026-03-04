#include <stdio.h>
#include "ssd_baseline.h"

/* 
 * SSD Latency Models based on published data:
 * 
 * SATA SSD (page-fault baseline used in this project):
 *   - Seek: 0 (no moving parts)
 *   - Kernel overhead: ~10 µs (context switch + I/O scheduler)
 *   - Effective transfer/service: ~150 µs
 *   - Total target baseline: ~160 µs (aligned with InfiniSwap page-fault path references)
 * 
 * NVMe (PCIe gen3):
 *   - Seek: 0
 *   - Kernel overhead: ~10 µs
 *   - Queue depth 1 latency: ~10-30 µs measured
 *   - Source: AnandTech benchmarks, Tom's Hardware
 * 
 * HDD (7200 RPM):
 *   - Seek: average 5-8 ms
 *   - Rotation: 4.17 ms average (half rotation)
 *   - Transfer: 1-2 µs per 4KB (negligible)
 *   - Total: 9-10 ms per random access
 *   - Source: Hard drive spec sheets
 */

ssd_baseline_t ssd_get_baseline(ssd_type_t type)
{
    ssd_baseline_t baseline;
    baseline.kernel_overhead_us = 10.0;  /* Same for all: context switch + scheduler */
    
    switch (type) {
        case SSD_TYPE_SATA:
            baseline.type = SSD_TYPE_SATA;
            baseline.seek_time_us = 0.0;          /* No seek */
            baseline.rotation_overhead_us = 0.0;  /* No rotation */
            baseline.transfer_latency_us = 150.0; /* 10 + 150 = 160 µs total baseline */
            break;
            
        case SSD_TYPE_NVME:
            baseline.type = SSD_TYPE_NVME;
            baseline.seek_time_us = 0.0;
            baseline.rotation_overhead_us = 0.0;
            baseline.transfer_latency_us = 20.0;  /* Average of range 10-30 */
            break;
            
        case SSD_TYPE_HDD:
            baseline.type = SSD_TYPE_HDD;
            baseline.seek_time_us = 6500.0;       /* Average seek time */
            baseline.rotation_overhead_us = 4000.0; /* Half rotation @ 7200 RPM */
            baseline.transfer_latency_us = 100.0; /* Transfer time negligible */
            break;
            
        default:
            baseline.type = SSD_TYPE_SATA;
            baseline.seek_time_us = 0.0;
            baseline.rotation_overhead_us = 0.0;
            baseline.transfer_latency_us = 150.0;
    }
    
    return baseline;
}

double ssd_page_fault_latency_us(ssd_type_t type, uint32_t page_size)
{
    ssd_baseline_t baseline = ssd_get_baseline(type);
    double size_scale = (page_size > 0) ? ((double)page_size / 4096.0) : 1.0;
    double transfer_latency = baseline.transfer_latency_us * size_scale;
    
    /* Total = kernel + seek + rotation + transfer */
    double total = baseline.kernel_overhead_us + 
                   baseline.seek_time_us +
                   baseline.rotation_overhead_us +
                   transfer_latency;
    
    return total;
}

void ssd_print_breakdown(ssd_type_t type)
{
    const char *type_str;
    switch (type) {
        case SSD_TYPE_SATA:  type_str = "SATA SSD"; break;
        case SSD_TYPE_NVME:  type_str = "NVMe SSD"; break;
        case SSD_TYPE_HDD:   type_str = "HDD 7200RPM"; break;
        default:            type_str = "Unknown"; break;
    }
    
    ssd_baseline_t baseline = ssd_get_baseline(type);
    double total = baseline.kernel_overhead_us +
                   baseline.seek_time_us +
                   baseline.rotation_overhead_us +
                   baseline.transfer_latency_us;
    
    printf("\n%s Page Fault Latency Breakdown:\n", type_str);
    printf("  Kernel overhead:       %8.2f µs\n", baseline.kernel_overhead_us);
    printf("  Seek time:             %8.2f µs\n", baseline.seek_time_us);
    printf("  Rotation overhead:     %8.2f µs\n", baseline.rotation_overhead_us);
    printf("  Transfer latency:      %8.2f µs\n", baseline.transfer_latency_us);
    printf("  ─────────────────────────────────\n");
    printf("  TOTAL:                 %8.2f µs\n", total);
}
