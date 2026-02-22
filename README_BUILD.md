# UPMEM Swap: User-Space Swap Manager for Processing-in-Memory Architecture

Production-ready simulator and module for integrating fast page swap operations using UPMEM PIM (Processing-in-Memory) devices. 

**Key features:**
- Fast swap latency (~30–50 µs per page, serial; ~12–32 µs with batching)
- Multi-DPU support with intelligent allocation (1–64 DPUs tested)
- Robust free-list management and MRAM reclamation
- Portable C99 implementation (simulator + modular API)
- Ready to integrate into other projects via CMake or tarball

---

## Quick Start

### Build (with CMake)

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make
```

### Run Benchmark

```bash
cd simulator
./benchmark_batch_improved 8 5000
# Outputs: latency and speedup metrics to CSV
```

### Use as Module

**Option 1: CMake-based projects**
```bash
# In your project's CMakeLists.txt
find_package(UPMEMSwap REQUIRED)
target_link_libraries(your_app UPMEMSwap::upmem_swap)
```

**Option 2: Manual linking**
```bash
gcc myapp.c simulator/src/upmem_swap.c simulator/src/page_table.c \
    simulator/src/memory_sim.c -I simulator/src -lm -o myapp
```

**Option 3: One-line install**
```bash
bash quick_install.sh
```

---

## Architecture

### Performance Summary

| Metric | Serial | Batch-10 | Batch-50 |
|--------|--------|----------|----------|
| Swap-out latency | ~30 µs | ~13 µs | ~12 µs |
| Swap-in latency | ~50 µs | ~34 µs | ~32 µs |
| Combined per-page | ~79 µs | ~47 µs | ~44 µs |
| Speedup (out) | 1.0× | 2.2× | 2.5× |
| Speedup (in) | 1.0× | 1.5× | 1.6× |

**Multi-DPU Scaling:**
- Latency per page remains ~44 µs regardless of DPU count (1–64)
- Throughput scales linearly: ~168 pages/s per batch op with 64 DPUs
- No latency increase detected (allocation cost negligible)

### Latency Model

Based on ETH Zürich characterization:
- **Kernel overhead**: ~12 µs (page fault handling, context switch)
- **MRAM internal**: α + β×size @ 350 MHz (7–10 µs for 4 KB)
- **HOST↔DPU transfer**: Asymmetric bandwidth
  - Write: 0.33 GB/s (~12 µs for 4 KB)
  - Read: 0.12 GB/s (~34 µs for 4 KB)
- **Batch amortization**: Overhead paid once per batch, not per-page

### Multi-DPU Allocation

- Per-DPU state: free-offset counter + allocation free-list
- Strategy: Round-robin with first-fit (skip full DPUs automatically)
- MRAM reclamation: Space marked free on swap-in for reuse
- Tested: 1–64 DPUs, 100–50,000 pages simultaneously

---

## Directory Structure

```
.
├── CMakeLists.txt               # Modern build configuration
├── LICENSE                       # MIT License
├── README.md                     # This file
├── quick_install.sh             # One-line installation script
├── cmake/
│   └── UPMEMSwapConfig.cmake.in # CMake config template
├── simulator/
│   ├── src/
│   │   ├── upmem_swap.{c,h}     # Core swap manager
│   │   ├── page_table.{c,h}     # Simple page table
│   │   ├── memory_sim.{c,h}     # RAM simulator
│   │   ├── config.h             # Configuration constants
│   │   └── ... (benchmarks, stats)
│   ├── benchmark_batch_improved.c
│   ├── example_integration.c     # Minimal usage example
│   ├── INTEGRATION.md            # Detailed integration guide
│   ├── package.sh                # Packaging helper
│   ├── dist/
│   │   └── upmem_swap_module.tar.gz  # Pre-packaged tarball
│   └── results/                 # Benchmark outputs (CSV, PNG)
└── docs/
    └── (Additional documentation)
```

---

## Integration Examples

### Example 1: Simple Program

```c
#include <stdio.h>
#include "upmem_swap.h"

int main() {
    upmem_swap_manager_t *mgr = upmem_swap_init(8);
    page_entry_t page = {.page_id = 0, .status = PAGE_IN_RAM};
    char buf[PAGE_SIZE];
    
    // Swap out page 0
    upmem_swap_out(mgr, &page, buf, PAGE_SIZE);
    printf("Page swapped to DPU %u\n", page.dpu_id);
    
    // Swap back in
    upmem_swap_in(mgr, &page, buf, PAGE_SIZE);
    
    upmem_swap_destroy(mgr);
    return 0;
}
```

### Example 2: Batch Operations

```c
page_entry_t *pages[10];
void *data[10];

// Prepare pages...
for (int i = 0; i < 10; i++) {
    pages[i] = &page_array[i];
    data[i] = buffer_array[i];
}

// Batch swap out (amortized overhead)
upmem_swap_out_batch(mgr, pages, data, 10);
```

### Example 3: CMake Integration

**Your project's CMakeLists.txt:**
```cmake
cmake_minimum_required(VERSION 3.16)
project(MyApp C)

find_package(UPMEMSwap REQUIRED)

add_executable(myapp main.c)
target_link_libraries(myapp PRIVATE UPMEMSwap::upmem_swap)
```

---

## Testing

Run all benchmarks:
```bash
cd simulator
./benchmark_batch_improved 1 5000    # Single DPU, 5k pages
./benchmark_batch_improved 8 20000   # 8 DPUs, 20k pages
./benchmark_batch_improved 64 50000  # 64 DPUs, 50k pages (stress)
```

Analyze results:
```bash
python3 scripts/analyze_best_batch.py
python3 scripts/visualize_publication.py
```

---

## Configuration

Edit `simulator/src/config.h` to adjust:
- `PAGE_SIZE`: 4 KB (standard)
- `DPU_MRAM_SIZE`: 64 MB per DPU
- `DPU_FREQUENCY_MHZ`: 350 MHz (MRAM)
- Host bandwidth (read/write)

---

## Performance Tips

1. **Use batch operations**: 2–2.5× speedup vs serial
2. **Multiple DPUs**: Linear aggregate throughput scaling
3. **Batch size**: 10–50 pages recommended (diminishing returns)
4. **Allocation**: Automatic; pre-allocate if known working-set size

---

## API Reference

| Function | Purpose |
|----------|---------|
| `upmem_swap_init(dpus)` | Initialize manager |
| `upmem_swap_out(mgr, page, data, size)` | Single page → MRAM |
| `upmem_swap_in(mgr, page, data, size)` | Single page ← MRAM |
| `upmem_swap_out_batch(...)` | Multiple pages → MRAM (amortized) |
| `upmem_swap_in_batch(...)` | Multiple pages ← MRAM |
| `upmem_swap_stats_print(mgr)` | Print latency summary |
| `upmem_swap_destroy(mgr)` | Cleanup |

Full API documentation: [simulator/INTEGRATION.md](simulator/INTEGRATION.md)

---

## Citation

If you use this work in research, please cite:

```bibtex
@article{upmem_swap_2026,
  author = {Your Name},
  title = {UPMEM Swap: Fast User-Space Swap for Processing-in-Memory},
  year = {2026},
  note = {Based on ETH Zürich UPMEM characterization}
}
```

---

## License

MIT License (see [LICENSE](LICENSE))

Based on ETH Zürich's UPMEM characterization research.

---

## Support

- **Issues**: Open on GitHub
- **Integration Help**: See [INTEGRATION.md](simulator/INTEGRATION.md)
- **Benchmarking**: Use `simulator/benchmark_batch_improved.c` as reference
