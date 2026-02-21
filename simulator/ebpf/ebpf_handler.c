#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include "ebpf_handler.h"

/*
 * User-space eBPF handler implementation
 * Processes page fault events and redirects to UPMEM swap manager
 */

ebpf_handler_stats_t* ebpf_handler_init(void)
{
    ebpf_handler_stats_t *stats = (ebpf_handler_stats_t *)malloc(sizeof(ebpf_handler_stats_t));
    if (!stats) {
        fprintf(stderr, "Error allocating ebpf_handler_stats_t\n");
        return NULL;
    }
    
    stats->total_faults = 0;
    stats->total_write_faults = 0;
    stats->total_read_faults = 0;
    stats->total_notification_latency_us = 0.0;
    stats->total_handling_latency_us = 0.0;
    stats->max_concurrent_threads = 0;
    stats->current_active_threads = 0;
    
    printf("eBPF Handler initialized\n");
    return stats;
}

int ebpf_handler_process_fault(ebpf_handler_stats_t *stats, 
                                page_fault_event_t *event)
{
    if (!stats || !event) {
        return -1;
    }
    
    struct timeval start, end;
    gettimeofday(&start, NULL);
    
    /* Here we would:
     * 1. Lookup the faulted address in our page table
     * 2. If page in SWAP: trigger upmem_swap_in()
     * 3. If page not yet allocated: allocate from RAM
     * 
     * For now, just collect statistics
     */
    
    if (event->is_write) {
        stats->total_write_faults++;
    } else {
        stats->total_read_faults++;
    }
    stats->total_faults++;
    
    gettimeofday(&end, NULL);
    double handling_us = (end.tv_sec - start.tv_sec) * 1000000.0 +
                        (end.tv_usec - start.tv_usec);
    
    stats->total_handling_latency_us += handling_us;
    
    return 0;
}

int ebpf_handler_start_listening(ebpf_handler_stats_t *stats)
{
    if (!stats) {
        return -1;
    }
    
    printf("eBPF handler listening for page faults...\n");
    /* In real implementation, we would use bpf_buffer_poll() here */
    
    return 0;
}

void ebpf_handler_stop(ebpf_handler_stats_t *stats)
{
    if (!stats) return;
    printf("eBPF handler stopped\n");
}

void ebpf_handler_print_stats(ebpf_handler_stats_t *stats)
{
    if (!stats) return;
    
    double avg_notification = 0.0;
    double avg_handling = 0.0;
    
    if (stats->total_faults > 0) {
        avg_notification = stats->total_notification_latency_us / stats->total_faults;
        avg_handling = stats->total_handling_latency_us / stats->total_faults;
    }
    
    printf("\n=== eBPF Handler Statistics ===\n");
    printf("Total page faults: %lu\n", stats->total_faults);
    printf("  Write faults: %lu (%.2f%%)\n", stats->total_write_faults,
           stats->total_faults > 0 ? 100.0 * stats->total_write_faults / stats->total_faults : 0);
    printf("  Read faults: %lu (%.2f%%)\n", stats->total_read_faults,
           stats->total_faults > 0 ? 100.0 * stats->total_read_faults / stats->total_faults : 0);
    printf("\nLatencies:\n");
    printf("  Avg notification (kernel→userspace): %.2f µs\n", avg_notification);
    printf("  Avg handling (userspace): %.2f µs\n", avg_handling);
    printf("  Max concurrent threads: %u\n", stats->max_concurrent_threads);
}

double ebpf_handler_get_avg_notification_us(ebpf_handler_stats_t *stats)
{
    if (!stats || stats->total_faults == 0) {
        return 0.0;
    }
    return stats->total_notification_latency_us / stats->total_faults;
}

void ebpf_handler_destroy(ebpf_handler_stats_t *stats)
{
    if (!stats) return;
    free(stats);
}
