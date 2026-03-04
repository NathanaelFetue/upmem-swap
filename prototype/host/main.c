/**
 * UPMEM Swap - Host Application
 * 
 * This application orchestrates DPU co-processors to optimize
 * memory swapping operations.
 * 
 * Build: make (when UPMEM SDK is available)
 * Run: ./build/host
 */

#include "main.h"
#include <time.h>

/* Prototype buffer size - MUST MATCH DPU kernel!
 * Start with 256 bytes for reliable testing */
#define PROTO_BUFFER_SIZE 256

void* allocate_swap_buffer(size_t size) {
    void* buffer = malloc(size);
    if (!buffer) {
        fprintf(stderr, "ERROR: Failed to allocate swap buffer\n");
        return NULL;
    }
    return buffer;
}

void free_swap_buffer(void* buffer) {
    if (buffer) {
        free(buffer);
    }
}

static long diff_nsec(struct timespec a, struct timespec b) {
    return (b.tv_sec - a.tv_sec) * 1000000000L + (b.tv_nsec - a.tv_nsec);
}

int main(int argc, char* argv[]) {
    printf("=== UPMEM Swap Prototype ===\n");
    printf("UPMEM SDK Status: ");

#ifdef HAVE_DPU_H
    printf("AVAILABLE\n");
#else
    printf("NOT AVAILABLE\n");
    printf("Running in development mode (simulated transfer)\n");
#endif

    /* Use a small buffer for quick iteration */
    size_t buffer_size = PROTO_BUFFER_SIZE;
    void* swap_buffer = allocate_swap_buffer(buffer_size);

    if (!swap_buffer) {
        return 1;
    }

    /* Initialize test pattern */
    memset(swap_buffer, 0xA5, buffer_size);

#ifdef HAVE_DPU_H
    /* Real DPU path using UPMEM Host API */
    struct dpu_set_t dpu_set, dpu;
    struct dpu_program_t *program = NULL;
    dpu_error_t err;

    /* Get DPU profile from environment (default: simulator) */
    const char *dpu_profile = getenv("DPU_PROFILE");
    if (!dpu_profile) {
        dpu_profile = "backend=simulator";
    }
    printf("DPU Profile: %s\n", dpu_profile);

    /* Try to allocate one rank (NR_DPUS may be 1) */
    //Just to precise that , its the Update of the SDK , that permit this change to dpu_copy_to for dpu_alloc_ranks... (voir Docu dans header)
    err = dpu_alloc_ranks(1, dpu_profile, &dpu_set);
    if (err != DPU_OK) {
        fprintf(stderr, "DPU allocation failed: %s\n", dpu_error_to_string(err));
        fprintf(stderr, "Falling back to simulation path.\n");
        goto simulated_transfer;
    }
    printf("✓ DPU allocated successfully\n");
    uint32_t nr_dpus;
    DPU_ASSERT(dpu_get_nr_dpus(dpu_set, &nr_dpus));
    printf("Number of DPUs: %u\n", nr_dpus);

    /* Load program onto DPUs */
    err = dpu_load(dpu_set, "build/dpu", &program);
    if (err != DPU_OK) {
        fprintf(stderr, "DPU load failed: %s\n", dpu_error_to_string(err));
        dpu_free(dpu_set);
        goto simulated_transfer;
    }
    printf("✓ DPU program loaded\n");

    /* CRITICAL: Verify the symbol exists and get its size */
    struct dpu_symbol_t symbol;
    err = dpu_get_symbol(program, "mram_buffer", &symbol);
    if (err != DPU_OK) {
        fprintf(stderr, "Symbol 'mram_buffer' not found: %s\n", dpu_error_to_string(err));
        dpu_free(dpu_set);
        goto simulated_transfer;
    }
    printf("✓ Symbol 'mram_buffer' found: size=%u bytes\n", symbol.size);

    /* Adjust buffer_size if necessary to match symbol */
    if (buffer_size > symbol.size) {
        printf("⚠ Reducing buffer_size from %zu to %u to match symbol\n", 
               buffer_size, symbol.size);
        buffer_size = symbol.size;
    }

    /* ALTERNATIVE APPROACH: Use DPU_FOREACH for per-DPU transfers
     * This is more robust with the simulator */
    printf("Using per-DPU transfer approach...\n");
    
    struct timespec t0_dpu, t1_dpu;
    clock_gettime(CLOCK_MONOTONIC, &t0_dpu);

    /* Copy to each DPU individually */
    DPU_FOREACH(dpu_set, dpu) {
        err = dpu_prepare_xfer(dpu, swap_buffer);
        if (err != DPU_OK) {
            fprintf(stderr, "dpu_prepare_xfer failed: %s\n", dpu_error_to_string(err));
            dpu_free(dpu_set);
            goto simulated_transfer;
        }
    }
    
    err = dpu_push_xfer(dpu_set, DPU_XFER_TO_DPU, "mram_buffer", 0, buffer_size, DPU_XFER_DEFAULT);
    if (err != DPU_OK) {
        fprintf(stderr, "dpu_push_xfer (TO_DPU) failed: %s\n", dpu_error_to_string(err));
        dpu_free(dpu_set);
        goto simulated_transfer;
    }
    printf("✓ Data copied to DPU MRAM (%zu bytes)\n", buffer_size);

    /* Launch DPUs synchronously */
    err = dpu_launch(dpu_set, DPU_SYNCHRONOUS);
    if (err != DPU_OK) {
        fprintf(stderr, "dpu_launch failed: %s\n", dpu_error_to_string(err));
        dpu_free(dpu_set);
        goto simulated_transfer;
    }
    printf("✓ DPU execution complete\n");

    /* Retrieve processed data back into swap_buffer using per-DPU approach */
    DPU_FOREACH(dpu_set, dpu) {
        err = dpu_prepare_xfer(dpu, swap_buffer);
        if (err != DPU_OK) {
            fprintf(stderr, "dpu_prepare_xfer (readback) failed: %s\n", dpu_error_to_string(err));
            dpu_free(dpu_set);
            goto simulated_transfer;
        }
    }
    
    err = dpu_push_xfer(dpu_set, DPU_XFER_FROM_DPU, "mram_buffer", 0, buffer_size, DPU_XFER_DEFAULT);
    
    clock_gettime(CLOCK_MONOTONIC, &t1_dpu);

    if (err != DPU_OK) {
        fprintf(stderr, "dpu_push_xfer (FROM_DPU) failed: %s\n", dpu_error_to_string(err));
        dpu_free(dpu_set);
        goto simulated_transfer;
    }
    printf("✓ Data copied from DPU MRAM (%zu bytes)\n", buffer_size);

    long ns_dpu = diff_nsec(t0_dpu, t1_dpu);
    printf("DPU round-trip transfer: %zu bytes in %ld ns (%.3f us)\n",
        buffer_size, ns_dpu, ns_dpu / 1000.0);

    /* Verify expected transformation: first byte inverted by DPU */
    uint8_t expected = (uint8_t)~0xA5;
    uint8_t actual = ((uint8_t*)swap_buffer)[0];
    printf("Verification (first byte): expected=0x%02X, actual=0x%02X → %s\n",
           expected, actual, (actual == expected) ? "✓ OK" : "✗ FAIL");

    dpu_free(dpu_set);
    free_swap_buffer(swap_buffer);
    printf("DPU prototype run complete\n");
    return 0;

simulated_transfer:
    printf("--- Falling back to simulated transfer ---\n");
#endif /* HAVE_DPU_H */

    /* Simulated host-only transfer path */
    void* device_buffer = malloc(buffer_size);
    if (!device_buffer) {
        fprintf(stderr, "ERROR: Failed to allocate device buffer\n");
        free_swap_buffer(swap_buffer);
        return 1;
    }

    struct timespec t0_sim, t1_sim;
    clock_gettime(CLOCK_MONOTONIC, &t0_sim);
    memcpy(device_buffer, swap_buffer, buffer_size);
    memcpy(swap_buffer, device_buffer, buffer_size);
    clock_gettime(CLOCK_MONOTONIC, &t1_sim);

    long ns_sim = diff_nsec(t0_sim, t1_sim);
    printf("Simulated round-trip transfer: %zu bytes in %ld ns (%.3f us)\n",
           buffer_size, ns_sim, ns_sim / 1000.0);

    /* Verify pattern (basic check) */
    uint8_t* p = (uint8_t*)swap_buffer;
    int mismatch = 0;
    for (size_t i = 0; i < buffer_size; i++) {
        if (p[i] != 0xA5) { mismatch = 1; break; }
    }
    printf("Verification: %s\n", mismatch ? "FAIL" : "OK");

    free(device_buffer);
    free_swap_buffer(swap_buffer);
    printf("Prototype run complete\n");

    
    return 0;
}