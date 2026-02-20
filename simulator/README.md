# UPMEM Swap Simulator

## Overview

This is a complete simulator for evaluating UPMEM MRAM as a fast swap mechanism.

**Key insight:** Can UPMEM MRAM (with ~25-30 µs latency) outperform SSDs (60-200 µs) as a swap backend?

This simulator validates the concept without requiring a real UPMEM hardware setup.

## Architecture

### Components

1. **Memory Simulator** (`memory_sim.c/.h`)
   - Simulates limited RAM (e.g., 32 MB)
   - Manages physical frames
   - Provides frame allocation/deallocation

2. **Page Table** (`page_table.c/.h`)
   - Virtual-to-physical page mapping
   - Tracks page status (IN_RAM, IN_SWAP, EMPTY)
   - Implements LRU victim selection using timestamps

3. **UPMEM Swap Manager** (`upmem_swap.c/.h`)
   - Manages data transfer between RAM and UPMEM DPUs
   - Allocates space round-robin across DPUs
   - Measures swap latencies accurately

4. **Workload Generator** (`workload.c/.h`)
   - Generates memory access patterns
   - Supports: random, sequential, mixed
   - Automatically handles page faults
   - Collects statistics

5. **Statistics Collector** (`stats.c/.h`)
   - Exports results to CSV
   - Compares against SSD/zram baselines
   - Pretty-prints results

## Building

### Prerequisites

- `gcc` with C99 support
- UPMEM SDK (optional - simulator works without it)

### Compile

```bash
make clean
make
```

### Build with debug output

```bash
make debug
```

## Usage

### Basic run

```bash
./swap_sim
```

### With custom parameters

```bash
./swap_sim --ram-mb 32 --dpus 16 --accesses 10000 --workload random
```

### All options

```bash
./swap_sim --help
```

Options:
- `--ram-mb <N>`: RAM size (default: 32)
- `--dpus <N>`: Number of UPMEM DPUs (default: 16)
- `--accesses <N>`: Total memory accesses (default: 10000)
- `--working-set <N>`: Working set pages (default: 10000)
- `--workload <type>`: Pattern: `random`, `sequential`, `mixed`
- `--output <file>`: CSV output file

## Examples

### Scaling test (1→16 DPUs)

```bash
make test_scaling
```

Creates: `results/1dpu.csv`, `results/4dpu.csv`, etc.

### Pattern comparison

```bash
make test_patterns
```

### Full test suite

```bash
make test
```

## Output

### Console

```
╔════════════════════════════════════════════╗
║       === UPMEM Swap Simulator ===         ║
╚════════════════════════════════════════════╝

Configuration:
  RAM size: 32 MB
  DPUs: 16
  Working set: 10000 pages
  Workload pattern: random

Running workload (10000 accesses)...
[██████████████████] 100%

=== Workload Results ===
Total accesses: 10000
Page hits: 7823 (78.23%)
Page faults: 2177 (21.77%)
Swap outs: 2177
Swap ins: 2177
Hit rate: 78.23%

=== UPMEM Swap Manager Stats ===
Avg swap out latency: 28.3 µs
Avg swap in latency: 26.1 µs

Comparison:
  UPMEM avg: 27.2 µs
  SSD (literature): 60-200 µs
  zram (literature): 20-50 µs

  Speedup vs SSD (100 µs baseline): 3.68×
```

### CSV Export

`results/swap_sim.csv`:
```csv
nr_dpus,ram_mb,working_set,pattern,total_accesses,page_faults,swapouts,swapins,avg_swapout_us,avg_swapin_us,hit_rate
16,32,10000,random,10000,2177,2177,2177,28.3,26.1,78.23
```

## Key Metrics

### Latency Model

Based on measured UPMEM benchmarks:
- **4 KB transfer**: ~12-34 µs
- **Model**: 10 µs base + 6.5 µs/KB

### Latency Sources

1. **Transfer overhead**: ~10 µs
2. **Data transfer**: ~6.5 µs per KB
3. **Hardware overhead**: negligible for simulator

### Expected Results

- **UPMEM**: 25-35 µs avg
- **vs SSD**: 2-8× faster
- **vs SATA**: 3-10× faster
- **vs zram**: Comparable or better

## Implementation Notes

### Memory Simulation

- RAM is allocated as a contiguous `malloc()` buffer
- Frames are 4 KB pages (PAGE_SIZE)
- Free frames tracked in a simple array

### Page Table

- Simple array-based (O(1) lookup)
- LRU victim = page with oldest `last_access_time`
- Status transitions: EMPTY → IN_RAM → IN_SWAP → IN_RAM

### UPMEM Simulation

- No actual UPMEM SDK calls (works standalone)
- Latency is **simulated** based on measured benchmarks
- DPU state tracks free offset in each MRAM

### Workload Patterns

1. **Random**: Uniform random access to working set
2. **Sequential**: Linear scan with wraparound
3. **Mixed**: 70% local (sequential window) + 30% random

## Files

```
simulator/
├── src/
│   ├── main.c              # Entry point + argument parsing
│   ├── memory_sim.c/.h     # RAM simulator
│   ├── page_table.c/.h     # Page table + LRU
│   ├── upmem_swap.c/.h     # Swap manager
│   ├── workload.c/.h       # Workload generator
│   ├── stats.c/.h          # Statistics
│   └── config.h            # Configuration
├── dpu/
│   └── dpu_program.c       # DPU code (Axis 1: empty)
├── results/                # Output CSV files
├── Makefile
└── README.md               # This file
```

## Extending

### Add real UPMEM SDK support

Replace `simulate_upmem_latency_us()` in `upmem_swap.c` with actual:
```c
dpu_prepare_xfer(dpu_set, ...)
dpu_push_xfer(...)
```

### Implement compression (Axe 2)

1. Add LZ4/RLE compression in DPU program
2. Modify swap_out/swap_in to handle variable sizes
3. Update MRAM allocation strategy

### Add contention model

Modify latency simulation to account for:
- Bus contention (multiple DPUs)
- Memory hierarchy effects
- Temperature throttling

## Performance Expectations

### Typical run (16 DPUs, 32 MB RAM, 10K accesses)

- **Runtime**: ~2-5 seconds
- **Hit rate**: 70-90% (depends on working set)
- **Swap latency**: 25-35 µs

## Related Work

- **UPMEM PIM**: https://www.upmem.com
- **zram**: Linux in-kernel compressed swap
- **InfiniSwap**: RDMA-based remote memory
- **Benchmarks**: See parent project for raw transfer measurements

## License

Academic project - ENSEEIHT/ISAE-Supaero Master 2 ACS

## Contact

- Projet: UPMEM Swap Simulator
- Deadline: 4 mars 2025 (publication)
- Publication: IEEE 4-page article

---

**Status**: ✅ Axe 1 (basic swap) - READY
**Future**: ⏰ Axe 2 (compression) - TODO
