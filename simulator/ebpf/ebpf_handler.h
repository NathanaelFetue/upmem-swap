#ifndef EBPF_HANDLER_H
#define EBPF_HANDLER_H

#include <stdint.h>
#include <time.h>

/*
 * User-space eBPF handler
 * Reads events from BPF perf buffer and processes page faults in user-space
 * 
 * Inspired by LightSwap - we use eBPF just for notification,
 * actual swap is done in user-space with UPMEM SDK
 */

typedef struct {
    uint32_t tid;
    uint32_t pid;
    uint64_t faulted_addr;
    uint64_t instruction_pointer;
    uint64_t timestamp;
    uint32_t is_write;
} page_fault_event_t;

typedef struct {
    /* Statistics */
    uint64_t total_faults;
    uint64_t total_write_faults;
    uint64_t total_read_faults;
    
    /* Timing */
    double total_notification_latency_us;  /* Kernel → userspace */
    double total_handling_latency_us;      /* Handling in userspace */
    
    /* Per-thread tracking */
    uint32_t max_concurrent_threads;
    uint32_t current_active_threads;
} ebpf_handler_stats_t;

/* Initialize eBPF handler */
ebpf_handler_stats_t* ebpf_handler_init(void);

/* Start listening for page fault events */
int ebpf_handler_start_listening(ebpf_handler_stats_t *stats);

/* Process single page fault event */
int ebpf_handler_process_fault(ebpf_handler_stats_t *stats, 
                                page_fault_event_t *event);

/* Stop listening */
void ebpf_handler_stop(ebpf_handler_stats_t *stats);

/* Print statistics */
void ebpf_handler_print_stats(ebpf_handler_stats_t *stats);

/* Get average notification latency (kernel → userspace) */
double ebpf_handler_get_avg_notification_us(ebpf_handler_stats_t *stats);

/* Cleanup */
void ebpf_handler_destroy(ebpf_handler_stats_t *stats);

#endif /* EBPF_HANDLER_H */
