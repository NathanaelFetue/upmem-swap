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

int main() {
    printf("=== UPMEM 4KB Page Swap Test ===\n");
    printf("Page size: %d bytes (Linux standard)\n", PAGE_SIZE);
    printf("Iterations: %d\n\n", NUM_ITERATIONS);
    
    // Allocate page buffer
    uint8_t *page_buffer = malloc(PAGE_SIZE);
    if (!page_buffer) {
        fprintf(stderr, "Failed to allocate page buffer\n");
        return 1;
    }
    
    // Initialize with pattern
    memset(page_buffer, 0xA5, PAGE_SIZE);
    
    // Allocate DPU
    struct dpu_set_t dpu_set;
    DPU_ASSERT(dpu_alloc(1, "backend=simulator", &dpu_set));
    printf("✓ DPU allocated (simulator)\n");
    
    uint32_t nr_dpus;
    DPU_ASSERT(dpu_get_nr_dpus(dpu_set, &nr_dpus));
    printf("✓ Number of DPUs: %u\n", nr_dpus);
    
    // Load DPU program
    DPU_ASSERT(dpu_load(dpu_set, "build/dpu", NULL));
    printf("✓ DPU program loaded\n\n");
    
    // Arrays for statistics
    long latencies[NUM_ITERATIONS];
    
    printf("Running %d iterations...\n", NUM_ITERATIONS);
    
    for (int iter = 0; iter < NUM_ITERATIONS; iter++) {
        struct timespec t_start, t_end;
        struct dpu_set_t dpu;
        
        clock_gettime(CLOCK_MONOTONIC, &t_start);
        
        // === TRANSFER 4KB PAGE (2 chunks of 2048 bytes) ===
        
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
        
        // === LAUNCH DPU ===
        DPU_ASSERT(dpu_launch(dpu_set, DPU_SYNCHRONOUS));
        
        // === RETRIEVE 4KB PAGE ===
        
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
        
        clock_gettime(CLOCK_MONOTONIC, &t_end);
        
        struct timespec diff = diff_time(t_start, t_end);
        latencies[iter] = timespec_to_ns(diff);
        
        if ((iter + 1) % 5 == 0) {
            printf("  [%d/%d] %ld ns (%.2f µs)\n", 
                   iter + 1, NUM_ITERATIONS, 
                   latencies[iter], latencies[iter] / 1000.0);
        }
    }
    
    // === CALCULATE STATISTICS ===
    long min = latencies[0], max = latencies[0], sum = 0;
    for (int i = 0; i < NUM_ITERATIONS; i++) {
        if (latencies[i] < min) min = latencies[i];
        if (latencies[i] > max) max = latencies[i];
        sum += latencies[i];
    }
    long mean = sum / NUM_ITERATIONS;
    
    // Calculate standard deviation
    long variance_sum = 0;
    for (int i = 0; i < NUM_ITERATIONS; i++) {
        long diff = latencies[i] - mean;
        variance_sum += diff * diff;
    }
    long stddev = (long)sqrt(variance_sum / NUM_ITERATIONS);
    
    printf("\n=== RESULTS (4KB Page Transfer) ===\n");
    printf("Min:    %ld ns (%.2f µs)\n", min, min / 1000.0);
    printf("Max:    %ld ns (%.2f µs)\n", max, max / 1000.0);
    printf("Mean:   %ld ns (%.2f µs)\n", mean, mean / 1000.0);
    printf("StdDev: %ld ns (%.2f µs)\n", stddev, stddev / 1000.0);
    
    // Verification
    printf("\n=== VERIFICATION ===\n");
    printf("First byte: 0x%02X (expected: 0xA5 unchanged (pure swap))\n", page_buffer[0]);
    if (page_buffer[0] == 0xA5) {
        printf("✓ Pure swap OK\n");
    } else {
        printf("✗ Pure swap FAILED\n");
    }
    
    // Cleanup
    DPU_ASSERT(dpu_free(dpu_set));
    free(page_buffer);
    
    printf("\n=== Test complete ===\n");
    return 0;
}
