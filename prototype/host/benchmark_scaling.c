#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <dpu.h>

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

typedef struct {
    long min, max, mean, stddev;
} stats_t;

stats_t calculate_stats(long* latencies, int n) {
    stats_t s;
    s.min = latencies[0];
    s.max = latencies[0];
    long sum = 0;
    
    for (int i = 0; i < n; i++) {
        if (latencies[i] < s.min) s.min = latencies[i];
        if (latencies[i] > s.max) s.max = latencies[i];
        sum += latencies[i];
    }
    s.mean = sum / n;
    
    long variance_sum = 0;
    for (int i = 0; i < n; i++) {
        long diff = latencies[i] - s.mean;
        variance_sum += diff * diff;
    }
    s.stddev = (long)sqrt(variance_sum / n);
    
    return s;
}

void transfer_to_dpu(struct dpu_set_t dpu_set, uint8_t* buffer, size_t size) {
    struct dpu_set_t dpu;
    size_t offset = 0;
    
    while (offset < size) {
        size_t chunk = (size - offset) > 2048 ? 2048 : (size - offset);
        
        DPU_FOREACH(dpu_set, dpu) {
            DPU_ASSERT(dpu_prepare_xfer(dpu, &buffer[offset]));
        }
        DPU_ASSERT(dpu_push_xfer(dpu_set, DPU_XFER_TO_DPU, 
                                 "mram_buffer", offset, chunk, 
                                 DPU_XFER_DEFAULT));
        offset += chunk;
    }
}

void transfer_from_dpu(struct dpu_set_t dpu_set, uint8_t* buffer, size_t size) {
    struct dpu_set_t dpu;
    size_t offset = 0;
    
    while (offset < size) {
        size_t chunk = (size - offset) > 2048 ? 2048 : (size - offset);
        
        DPU_FOREACH(dpu_set, dpu) {
            DPU_ASSERT(dpu_prepare_xfer(dpu, &buffer[offset]));
        }
        DPU_ASSERT(dpu_push_xfer(dpu_set, DPU_XFER_FROM_DPU,
                                 "mram_buffer", offset, chunk,
                                 DPU_XFER_DEFAULT));
        offset += chunk;
    }
}

void benchmark_size(struct dpu_set_t dpu_set, size_t size, const char* label) {
    uint8_t* buffer = malloc(size);
    memset(buffer, 0xA5, size);
    
    long latencies_write[NUM_ITERATIONS];
    long latencies_read[NUM_ITERATIONS];
    
    printf("Testing %s (%zu bytes)...\n", label, size);
    
    for (int iter = 0; iter < NUM_ITERATIONS; iter++) {
        struct timespec t_start, t_end;
        
        // Measure WRITE (TO_DPU)
        clock_gettime(CLOCK_MONOTONIC, &t_start);
        transfer_to_dpu(dpu_set, buffer, size);
        clock_gettime(CLOCK_MONOTONIC, &t_end);
        latencies_write[iter] = timespec_to_ns(diff_time(t_start, t_end));
        
        // Measure READ (FROM_DPU)
        clock_gettime(CLOCK_MONOTONIC, &t_start);
        transfer_from_dpu(dpu_set, buffer, size);
        clock_gettime(CLOCK_MONOTONIC, &t_end);
        latencies_read[iter] = timespec_to_ns(diff_time(t_start, t_end));
    }
    
    stats_t stats_write = calculate_stats(latencies_write, NUM_ITERATIONS);
    stats_t stats_read = calculate_stats(latencies_read, NUM_ITERATIONS);
    
    printf("  WRITE: %.2f µs (min: %.2f, max: %.2f, std: %.2f)\n",
           stats_write.mean / 1000.0, stats_write.min / 1000.0,
           stats_write.max / 1000.0, stats_write.stddev / 1000.0);
    printf("  READ:  %.2f µs (min: %.2f, max: %.2f, std: %.2f)\n\n",
           stats_read.mean / 1000.0, stats_read.min / 1000.0,
           stats_read.max / 1000.0, stats_read.stddev / 1000.0);
    
    free(buffer);
}

void benchmark_batch(struct dpu_set_t dpu_set, size_t page_size, int num_pages, const char* label) {
    size_t total_size = page_size * num_pages;
    uint8_t* buffer = malloc(total_size);
    memset(buffer, 0xA5, total_size);
    
    long latencies_write[NUM_ITERATIONS];
    long latencies_read[NUM_ITERATIONS];
    
    printf("Testing %s (%d × %zu bytes = %.2f KB)...\n", 
           label, num_pages, page_size, total_size / 1024.0);
    
    for (int iter = 0; iter < NUM_ITERATIONS; iter++) {
        struct timespec t_start, t_end;
        
        // Measure BATCH WRITE
        clock_gettime(CLOCK_MONOTONIC, &t_start);
        for (int i = 0; i < num_pages; i++) {
            transfer_to_dpu(dpu_set, &buffer[i * page_size], page_size);
        }
        clock_gettime(CLOCK_MONOTONIC, &t_end);
        latencies_write[iter] = timespec_to_ns(diff_time(t_start, t_end));
        
        // Measure BATCH READ
        clock_gettime(CLOCK_MONOTONIC, &t_start);
        for (int i = 0; i < num_pages; i++) {
            transfer_from_dpu(dpu_set, &buffer[i * page_size], page_size);
        }
        clock_gettime(CLOCK_MONOTONIC, &t_end);
        latencies_read[iter] = timespec_to_ns(diff_time(t_start, t_end));
    }
    
    stats_t stats_write = calculate_stats(latencies_write, NUM_ITERATIONS);
    stats_t stats_read = calculate_stats(latencies_read, NUM_ITERATIONS);
    
    double write_us_per_page = (stats_write.mean / 1000.0) / num_pages;
    double read_us_per_page = (stats_read.mean / 1000.0) / num_pages;
    
    printf("  WRITE: %.2f µs total (%.2f µs/page)\n",
           stats_write.mean / 1000.0, write_us_per_page);
    printf("  READ:  %.2f µs total (%.2f µs/page)\n\n",
           stats_read.mean / 1000.0, read_us_per_page);
    
    free(buffer);
}

int main() {
    printf("=== UPMEM SWAP BENCHMARK - SCALING TEST ===\n");
    printf("Simulator backend, 1 DPU, %d iterations per test\n\n", NUM_ITERATIONS);
    
    // Allocate DPU
    struct dpu_set_t dpu_set;
    DPU_ASSERT(dpu_alloc(1, "backend=simulator", &dpu_set));
    
    uint32_t nr_dpus;
    DPU_ASSERT(dpu_get_nr_dpus(dpu_set, &nr_dpus));
    printf("✓ DPU allocated (simulator)\n");
    printf("✓ Number of DPUs: %u\n\n", nr_dpus);
    
    // Load DPU program
    DPU_ASSERT(dpu_load(dpu_set, "build/dpu", NULL));
    printf("✓ DPU program loaded\n\n");
    
    printf("============================================================\n");
    printf("PART 1: INDIVIDUAL SIZE SCALING\n");
    printf("============================================================\n\n");
    
    benchmark_size(dpu_set, 512, "512 bytes");
    benchmark_size(dpu_set, 1024, "1 KB");
    benchmark_size(dpu_set, 2048, "2 KB");
    benchmark_size(dpu_set, 4096, "4 KB");
    benchmark_size(dpu_set, 8192, "8 KB (2×4KB sequential)");
    
    printf("\n============================================================\n");
    printf("PART 2: BATCH PROCESSING\n");
    printf("============================================================\n\n");
    
    benchmark_batch(dpu_set, 4096, 10, "10 pages batch");
    benchmark_batch(dpu_set, 4096, 100, "100 pages batch");
    benchmark_batch(dpu_set, 4096, 1000, "1000 pages batch");
    
    // Cleanup
    DPU_ASSERT(dpu_free(dpu_set));
    
    printf("\n=== BENCHMARK COMPLETE ===\n");
    printf("\nKEY FINDINGS TO ANALYZE:\n");
    printf("1. Latency vs size relationship (linear/sub-linear?)\n");
    printf("2. Per-page latency improvement with batching\n");
    printf("3. Comparison with SSD baselines (see research results)\n\n");
    
    return 0;
}
