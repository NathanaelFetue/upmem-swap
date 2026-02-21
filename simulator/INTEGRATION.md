# UPMEM Swap Module — Integration Guide

## Quick Start

The packaged module (`dist/upmem_swap_module.tar.gz`) is ready to integrate into any C project using UPMEM or the SDK simulator.

### Extract and Copy

```bash
# In your project directory
tar -xzf upmem_swap_module.tar.gz
cp -r upmem_swap_module/src . 2>/dev/null || true
cp upmem_swap_module/*.h your_project/
cp upmem_swap_module/*.c your_project/
```

### Build Integration

Add to your `Makefile` or CMakeLists.txt:

**Makefile example:**
```makefile
SWAP_SRCS = src/upmem_swap.c src/page_table.c src/memory_sim.c
CFLAGS += -I src
LDFLAGS += -lm

your_program: main.c $(SWAP_SRCS:.c=.o)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
```

**Or with CMake:**
```cmake
add_executable(your_program
    main.c
    src/upmem_swap.c
    src/page_table.c
    src/memory_sim.c
)
target_include_directories(your_program PRIVATE src)
```

### Minimal Usage Example

```c
#include "upmem_swap.h"
#include "page_table.h"

int main() {
    /* Initialize swap manager for 8 DPUs */
    upmem_swap_manager_t *mgr = upmem_swap_init(8);

    /* Create a page table entry */
    page_entry_t page;
    page.page_id = 0;
    page.status = PAGE_IN_RAM;

    /* Some data to swap */
    char buf[PAGE_SIZE];
    memset(buf, 0xAA, PAGE_SIZE);

    /* Swap out */
    if (upmem_swap_out(mgr, &page, buf, PAGE_SIZE) == 0) {
        printf("Swapped page %u to DPU %u\n", page.page_id, page.dpu_id);
    }

    /* Swap in */
    if (upmem_swap_in(mgr, &page, buf, PAGE_SIZE) == 0) {
        printf("Recovered page %u\n", page.page_id);
    }

    upmem_swap_destroy(mgr);
    return 0;
}
```

See `upmem_swap_module/example_integration.c` for a complete minimal program.

## API Reference

### Core Functions

#### `upmem_swap_manager_t* upmem_swap_init(uint32_t nr_dpus)`
Initialize the swap manager for given number of DPUs.

#### `int upmem_swap_out(manager, page, data, size)`
Move a page from RAM to DPU MRAM. Updates `page->dpu_id` and `page->dpu_offset`.

#### `int upmem_swap_in(manager, page, data, size)`
Retrieve a page from MRAM back to RAM. Automatically reclaims MRAM space.

#### `int upmem_swap_out_batch(manager, pages[], data[], count)`
Batch swap out multiple pages in a single amortized operation (faster).

#### `int upmem_swap_in_batch(manager, pages[], data[], count)`
Batch swap in multiple pages.

#### `void upmem_swap_stats_print(manager)`
Print latency and throughput statistics.

#### `void upmem_swap_destroy(manager)`
Cleanup and free all resources.

## Performance Notes

- **Per-page latency** (measured): serial ~30–50 µs; batch-10 ~13–34 µs; batch-50 ~12–32 µs.
- **Throughput** (at 64 DPUs, 50k pages): Batch-50 delivers ~168 pages/s outbound, ~62 pages/s return.
- **Allocation strategy**: Robust round-robin + first-fit free-list. Automatically distributes pages across available DPUs.

## Tested Configurations

- 1–64 DPUs
- 100–50,000 pages
- Batch sizes 1, 2, 5, 10, 20, 50

## Dependencies

- **Compiler**: GCC 7.0+, C99 standard
- **Libraries**: Standard C library, libm (math)
- **SDK** (optional): UPMEM SDK for real hardware tests

## Key Implementation Details

### Multi-DPU Allocation

- Uses `find_available_dpu()` to distribute pages intelligently.
- Falls back to next DPU if current one is full.
- Maintains per-DPU free list to reclaim MRAM after swap-in.

### Latency Model

Based on ETH Zürich characterization paper:
- MRAM latency: α + β×size (cycles @ 350 MHz)
- HOST↔DPU bandwidth: asymmetric (write 0.33 GB/s, read 0.12 GB/s)
- Kernel overhead: ~12 µs per operation
- Jitter: ±5% realistic variation

### Batch Amortization

Kernel overhead paid once per batch, not per-page → 2–2.5× speedup vs serial.

## Troubleshooting

**"All DPUs are full"**: Increase DPU count or set `-DDPU_MRAM_SIZE` larger in `config.h`.

**Compile errors**: Ensure `PAGE_SIZE` (4 KB) and `DPU_MRAM_SIZE` (64 MB) match your environment in `config.h`.

**Latencies seem wrong**: Check that `config.h` constants (frequency, bandwidths) match your UPMEM model.

## License

Academic use under ETH Zürich collaboration license (included in repo).
