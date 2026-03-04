#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <dpu.h>

#define PAGE_SIZE 4096
#define CHUNK_SIZE 2048
#define NUM_ITERATIONS 20

struct timespec diff_time(struct timespec start, struct timespec end) {
    struct timespec temp;
    if ((end.tv_nsec - start.tv_nsec) < 0) {
        temp.tv_sec = end.tv_sec - start.tv_sec - 1;
        temp.tv_nsec = 1000000000 + end.tv_nsec - start.tv_nsec;
    } else {
        temp.tv_sec = end.tv_sec - start.tv_sec;
        temp.tv_nsec = end.tv_nsec - start.tv_nsec;
    }
    return temp;
}

long timespec_to_ns(struct timespec ts) {
    return ts.tv_sec * 1000000000L + ts.tv_nsec;
}

void print_stats(const char* label, long* latencies, int n) {
    long min = latencies[0], max = latencies[0], sum = 0;
    for (int i = 0; i < n; i++) {
        if (latencies[i] < min) min = latencies[i];
        if (latencies[i] > max) max = latencies[i];
        sum += latencies[i];
    }
    long mean = sum / n;
    
    long variance_sum = 0;
    for (int i = 0; i < n; i++) {
        long diff = latencies[i] - mean;
        variance_sum += diff * diff;
    }
    long stddev = (long)sqrt(variance_sum / n);
    
    printf("%s:\n", label);
    printf("  Min:    %6ld ns (%7.2f µs)\n", min, min / 1000.0);
    printf("  Max:    %6ld ns (%7.2f µs)\n", max, max / 1000.0);
    printf("  Mean:   %6ld ns (%7.2f µs)\n", mean, mean / 1000.0);
    printf("  StdDev: %6ld ns (%7.2f µs)\n\n", stddev, stddev / 1000.0);
}

int main() {
    printf("=== UPMEM LATENCY DECOMPOSITION TEST ===\n");
    printf("Measuring: TO_DPU, FROM_DPU, LAUNCH separately\n");
    printf("Page size: %d bytes (4KB)\n", PAGE_SIZE);
    printf("Iterations: %d\n\n", NUM_ITERATIONS);
    
    // Allocate page buffer
    uint8_t *page_buffer = malloc(PAGE_SIZE);
    if (!page_buffer) {
        fprintf(stderr, "Failed to allocate page buffer\n");
        return 1;
    }
    memset(page_buffer, 0xA5, PAGE_SIZE);
    
    // Allocate DPU
    struct dpu_set_t dpu_set;
    DPU_ASSERT(dpu_alloc(1, "backend=simulator", &dpu_set));
    printf("✓ DPU allocated (simulator)\n");
    
    uint32_t nr_dpus;
    DPU_ASSERT(dpu_get_nr_dpus(dpu_set, &nr_dpus));
    printf("✓ Number of DPUs: %u\n\n", nr_dpus);
    
    // Load DPU program
    DPU_ASSERT(dpu_load(dpu_set, "build/dpu", NULL));
    printf("✓ DPU program loaded\n\n");
    
    // Arrays for statistics
    long latencies_to[NUM_ITERATIONS];
    long latencies_from[NUM_ITERATIONS];
    long latencies_launch[NUM_ITERATIONS];
    long latencies_total[NUM_ITERATIONS];
    
    printf("Running %d iterations...\n", NUM_ITERATIONS);
    
    for (int iter = 0; iter < NUM_ITERATIONS; iter++) {
        struct timespec t_start, t_end, t_tmp;
        struct dpu_set_t dpu;
        
        // === TOTAL START ===
        clock_gettime(CLOCK_MONOTONIC, &t_start);
        
        // === MEASURE TO_DPU (WRITE) ===
        struct timespec t_to_start, t_to_end;
        clock_gettime(CLOCK_MONOTONIC, &t_to_start);
        
        // Chunk 1: bytes 0-2047
        DPU_FOREACH(dpu_set, dpu) {
            DPU_ASSERT(dpu_prepare_xfer(dpu, &page_buffer[0]));
        }
        DPU_ASSERT(dpu_push_xfer(dpu_set, DPU_XFER_TO_DPU, 
                                 "mram_buffer", 0, CHUNK_SIZE, 
                                 DPU_XFER_DEFAULT));
        
        // Chunk 2: bytes 2048-4095
        DPU_FOREACH(dpu_set, dpu) {
            DPU_ASSERT(dpu_prepare_xfer(dpu, &page_buffer[CHUNK_SIZE]));
        }
        DPU_ASSERT(dpu_push_xfer(dpu_set, DPU_XFER_TO_DPU, 
                                 "mram_buffer", CHUNK_SIZE, CHUNK_SIZE,
                                 DPU_XFER_DEFAULT));
        
        clock_gettime(CLOCK_MONOTONIC, &t_to_end);
        latencies_to[iter] = timespec_to_ns(diff_time(t_to_start, t_to_end));
        
        // === MEASURE LAUNCH DPU ===
        struct timespec t_launch_start, t_launch_end;
        clock_gettime(CLOCK_MONOTONIC, &t_launch_start);
        
        DPU_ASSERT(dpu_launch(dpu_set, DPU_SYNCHRONOUS));
        
        clock_gettime(CLOCK_MONOTONIC, &t_launch_end);
        latencies_launch[iter] = timespec_to_ns(diff_time(t_launch_start, t_launch_end));
        
        // === MEASURE FROM_DPU (READ) ===
        struct timespec t_from_start, t_from_end;
        clock_gettime(CLOCK_MONOTONIC, &t_from_start);
        
        // Chunk 1: bytes 0-2047
        DPU_FOREACH(dpu_set, dpu) {
            DPU_ASSERT(dpu_prepare_xfer(dpu, &page_buffer[0]));
        }
        DPU_ASSERT(dpu_push_xfer(dpu_set, DPU_XFER_FROM_DPU,
                                 "mram_buffer", 0, CHUNK_SIZE,
                                 DPU_XFER_DEFAULT));
        
        // Chunk 2: bytes 2048-4095
        DPU_FOREACH(dpu_set, dpu) {
            DPU_ASSERT(dpu_prepare_xfer(dpu, &page_buffer[CHUNK_SIZE]));
        }
        DPU_ASSERT(dpu_push_xfer(dpu_set, DPU_XFER_FROM_DPU,
                                 "mram_buffer", CHUNK_SIZE, CHUNK_SIZE,
                                 DPU_XFER_DEFAULT));
        
        clock_gettime(CLOCK_MONOTONIC, &t_from_end);
        latencies_from[iter] = timespec_to_ns(diff_time(t_from_start, t_from_end));
        
        // === TOTAL END ===
        clock_gettime(CLOCK_MONOTONIC, &t_end);
        latencies_total[iter] = timespec_to_ns(diff_time(t_start, t_end));
        
        if ((iter + 1) % 5 == 0) {
            printf("  [%d/%d] TO: %.2f µs | LAUNCH: %.2f µs | FROM: %.2f µs | TOTAL: %.2f µs\n",
                   iter + 1, NUM_ITERATIONS,
                   latencies_to[iter] / 1000.0,
                   latencies_launch[iter] / 1000.0,
                   latencies_from[iter] / 1000.0,
                   latencies_total[iter] / 1000.0);
        }
    }
    
    printf("\n=== DECOMPOSITION RESULTS ===\n\n");
    
    print_stats("TO_DPU (WRITE 4KB)", latencies_to, NUM_ITERATIONS);
    print_stats("LAUNCH DPU", latencies_launch, NUM_ITERATIONS);
    print_stats("FROM_DPU (READ 4KB)", latencies_from, NUM_ITERATIONS);
    print_stats("TOTAL (WRITE+LAUNCH+READ)", latencies_total, NUM_ITERATIONS);
    
    // Calculate percentages
    long mean_to = 0, mean_launch = 0, mean_from = 0, mean_total = 0;
    for (int i = 0; i < NUM_ITERATIONS; i++) {
        mean_to += latencies_to[i];
        mean_launch += latencies_launch[i];
        mean_from += latencies_from[i];
        mean_total += latencies_total[i];
    }
    mean_to /= NUM_ITERATIONS;
    mean_launch /= NUM_ITERATIONS;
    mean_from /= NUM_ITERATIONS;
    mean_total /= NUM_ITERATIONS;
    
    printf("=== BREAKDOWN (Percentages) ===\n");
    printf("TO_DPU:     %.1f%% (%ld ns)\n", 100.0 * mean_to / mean_total, mean_to);
    printf("LAUNCH:     %.1f%% (%ld ns)\n", 100.0 * mean_launch / mean_total, mean_launch);
    printf("FROM_DPU:   %.1f%% (%ld ns)\n", 100.0 * mean_from / mean_total, mean_from);
    printf("─────────────────────────────\n");
    printf("TOTAL:      100.0%% (%ld ns)\n\n", mean_total);
    
    // Cleanup
    DPU_ASSERT(dpu_free(dpu_set));
    free(page_buffer);
    
    printf("=== Analysis complete ===\n");
    return 0;
}
