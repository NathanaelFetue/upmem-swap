#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <dpu.h>

#define NUM_ITERATIONS 20
#define MAX_SIZE 65536

typedef enum {
    MODE_SERIAL,
    MODE_PARALLEL
} transfer_mode_t;

typedef struct {
    long min, max, mean, stddev;
    double throughput_mbps;
} stats_t;

typedef struct {
    int nr_dpus;
    int nr_tasklets;
    size_t size;
    transfer_mode_t mode;
    stats_t write_stats;
    stats_t read_stats;
} benchmark_result_t;

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

stats_t calculate_stats(long* latencies, int n, size_t bytes) {
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
    
    // Calculate throughput in MB/s
    double seconds = s.mean / 1e9;
    double megabytes = bytes / (1024.0 * 1024.0);
    s.throughput_mbps = megabytes / seconds;
    
    return s;
}

void transfer_to_dpu_single(struct dpu_set_t dpu, uint8_t* buffer, size_t size) {
    size_t offset = 0;
    while (offset < size) {
        size_t chunk = (size - offset) > 2048 ? 2048 : (size - offset);
        DPU_ASSERT(dpu_prepare_xfer(dpu, &buffer[offset]));
        DPU_ASSERT(dpu_push_xfer(dpu, DPU_XFER_TO_DPU, 
                                 "mram_buffer", offset, chunk, 
                                 DPU_XFER_DEFAULT));
        offset += chunk;
    }
}

void transfer_from_dpu_single(struct dpu_set_t dpu, uint8_t* buffer, size_t size) {
    size_t offset = 0;
    while (offset < size) {
        size_t chunk = (size - offset) > 2048 ? 2048 : (size - offset);
        DPU_ASSERT(dpu_prepare_xfer(dpu, &buffer[offset]));
        DPU_ASSERT(dpu_push_xfer(dpu, DPU_XFER_FROM_DPU,
                                 "mram_buffer", offset, chunk,
                                 DPU_XFER_DEFAULT));
        offset += chunk;
    }
}

void transfer_serial(struct dpu_set_t dpu_set, uint8_t** buffers, 
                     size_t size, int write, int nr_dpus) {
    struct dpu_set_t dpu;
    int i = 0;
    
    DPU_FOREACH(dpu_set, dpu, i) {
        if (write) {
            transfer_to_dpu_single(dpu, buffers[i], size);
        } else {
            transfer_from_dpu_single(dpu, buffers[i], size);
        }
    }
}

void transfer_parallel(struct dpu_set_t dpu_set, uint8_t** buffers,
                       size_t size, int write, int nr_dpus) {
    size_t offset = 0;
    
    while (offset < size) {
        size_t chunk = (size - offset) > 2048 ? 2048 : (size - offset);
        
        struct dpu_set_t dpu;
        int i = 0;
        
        // Prepare all DPUs
        DPU_FOREACH(dpu_set, dpu, i) {
            DPU_ASSERT(dpu_prepare_xfer(dpu, &buffers[i][offset]));
        }
        
        // Push in parallel
        if (write) {
            DPU_ASSERT(dpu_push_xfer(dpu_set, DPU_XFER_TO_DPU,
                                     "mram_buffer", offset, chunk,
                                     DPU_XFER_DEFAULT));
        } else {
            DPU_ASSERT(dpu_push_xfer(dpu_set, DPU_XFER_FROM_DPU,
                                     "mram_buffer", offset, chunk,
                                     DPU_XFER_DEFAULT));
        }
        
        offset += chunk;
    }
}

benchmark_result_t run_benchmark(int nr_dpus, int nr_tasklets, 
                                  size_t size, transfer_mode_t mode) {
    benchmark_result_t result = {0};
    result.nr_dpus = nr_dpus;
    result.nr_tasklets = nr_tasklets;
    result.size = size;
    result.mode = mode;
    
    // Allocate DPUs
    struct dpu_set_t dpu_set;
    char profile[256];
    snprintf(profile, sizeof(profile), 
             "backend=simulator,nr_tasklets=%d", nr_tasklets);
    
    DPU_ASSERT(dpu_alloc(nr_dpus, profile, &dpu_set));
    DPU_ASSERT(dpu_load(dpu_set, "build/dpu_tasklets", NULL));
    
    // Allocate buffers
    uint8_t** buffers = malloc(nr_dpus * sizeof(uint8_t*));
    for (int i = 0; i < nr_dpus; i++) {
        buffers[i] = malloc(size);
        memset(buffers[i], 0xA5, size);
    }
    
    long latencies_write[NUM_ITERATIONS];
    long latencies_read[NUM_ITERATIONS];
    
    // Run iterations
    for (int iter = 0; iter < NUM_ITERATIONS; iter++) {
        struct timespec t_start, t_end;
        
        // Measure WRITE
        clock_gettime(CLOCK_MONOTONIC, &t_start);
        if (mode == MODE_SERIAL) {
            transfer_serial(dpu_set, buffers, size, 1, nr_dpus);
        } else {
            transfer_parallel(dpu_set, buffers, size, 1, nr_dpus);
        }
        clock_gettime(CLOCK_MONOTONIC, &t_end);
        latencies_write[iter] = timespec_to_ns(diff_time(t_start, t_end));
        
        // Measure READ
        clock_gettime(CLOCK_MONOTONIC, &t_start);
        if (mode == MODE_SERIAL) {
            transfer_serial(dpu_set, buffers, size, 0, nr_dpus);
        } else {
            transfer_parallel(dpu_set, buffers, size, 0, nr_dpus);
        }
        clock_gettime(CLOCK_MONOTONIC, &t_end);
        latencies_read[iter] = timespec_to_ns(diff_time(t_start, t_end));
    }
    
    // Calculate stats
    size_t total_bytes = size * nr_dpus;
    result.write_stats = calculate_stats(latencies_write, NUM_ITERATIONS, total_bytes);
    result.read_stats = calculate_stats(latencies_read, NUM_ITERATIONS, total_bytes);
    
    // Cleanup
    for (int i = 0; i < nr_dpus; i++) {
        free(buffers[i]);
    }
    free(buffers);
    DPU_ASSERT(dpu_free(dpu_set));
    
    return result;
}

void save_results_csv(benchmark_result_t* results, int count, const char* filename) {
    FILE* f = fopen(filename, "w");
    fprintf(f, "nr_dpus,nr_tasklets,size,mode,write_mean_us,write_min_us,write_max_us,write_std_us,write_throughput_mbps,read_mean_us,read_min_us,read_max_us,read_std_us,read_throughput_mbps\n");
    
    for (int i = 0; i < count; i++) {
        benchmark_result_t* r = &results[i];
        fprintf(f, "%d,%d,%zu,%s,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f\n",
                r->nr_dpus, r->nr_tasklets, r->size,
                r->mode == MODE_SERIAL ? "serial" : "parallel",
                r->write_stats.mean / 1000.0, r->write_stats.min / 1000.0,
                r->write_stats.max / 1000.0, r->write_stats.stddev / 1000.0,
                r->write_stats.throughput_mbps,
                r->read_stats.mean / 1000.0, r->read_stats.min / 1000.0,
                r->read_stats.max / 1000.0, r->read_stats.stddev / 1000.0,
                r->read_stats.throughput_mbps);
    }
    
    fclose(f);
}

int main() {
    printf("=== UPMEM COMPREHENSIVE BENCHMARK ===\n\n");
    
    int dpu_counts[] = {1, 8, 16, 32, 64};
    int tasklet_counts[] = {1, 4, 8, 16};
    size_t sizes[] = {512, 1024, 2048, 4096, 8192};
    transfer_mode_t modes[] = {MODE_SERIAL, MODE_PARALLEL};
    
    int total_tests = sizeof(dpu_counts)/sizeof(int) * 
                      sizeof(tasklet_counts)/sizeof(int) *
                      sizeof(sizes)/sizeof(size_t) *
                      sizeof(modes)/sizeof(transfer_mode_t);
    
    printf("Total tests to run: %d\n", total_tests);
    printf("Estimated time: ~%d minutes\n\n", total_tests / 4);
    
    benchmark_result_t* results = malloc(total_tests * sizeof(benchmark_result_t));
    int idx = 0;
    
    for (int d = 0; d < sizeof(dpu_counts)/sizeof(int); d++) {
        for (int t = 0; t < sizeof(tasklet_counts)/sizeof(int); t++) {
            for (int s = 0; s < sizeof(sizes)/sizeof(size_t); s++) {
                for (int m = 0; m < sizeof(modes)/sizeof(transfer_mode_t); m++) {
                    printf("[%d/%d] Testing: %d DPUs, %d tasklets, %zu bytes, %s\n",
                           idx+1, total_tests,
                           dpu_counts[d], tasklet_counts[t], sizes[s],
                           modes[m] == MODE_SERIAL ? "serial" : "parallel");
                    
                    results[idx] = run_benchmark(dpu_counts[d], tasklet_counts[t],
                                                 sizes[s], modes[m]);
                    idx++;
                }
            }
        }
    }
    
    // Save results
    save_results_csv(results, total_tests, "benchmark_results.csv");
    printf("\n✓ Results saved to benchmark_results.csv\n");
    
    free(results);
    return 0;
}
