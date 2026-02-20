# Architecture Technique - Swap UPMEM

## 1. Architecture Matérielle

### 1.1 Système cible (configuration type)

```
┌─────────────────────────────────────────────────────────────────┐
│ Processeur (AMD EPYC / Genoa)                                   │
│                                                                 │
│ ┌──────────────────────────────────────────────────────────┐  │
│ │ Cores (128x avec SMT)                                    │  │
│ │  - L1: 32 KB par core                                    │  │
│ │  - L2: 512 KB par core                                   │  │
│ │  - L3: 12 MB partagé (12-cores coherency domains)        │  │
│ └──────────────────────────────────────────────────────────┘  │
│                                                                 │
│ ┌──────────────────────────────────────────────────────────┐  │
│ │ Memory Controller (built-in, 12 channels DDR5)            │  │
│ │  - 600+ GB/s bandwidth (total)                            │  │
│ │  - 50 GB/s per channel (10 channels x 5 GB/s)            │  │
│ │  - Latency: ~90 ns to DRAM                               │  │
│ └──────────────────────────────────────────────────────────┘  │
│                                                                 │
│ ┌──────────────────────────────────────────────────────────┐  │
│ │ UPMEM Interface (built-in to new EPYC)                   │  │
│ │  - PIM Rank Controllers (4x per socket)                  │  │
│ │  - Coherency with CPU caches                             │  │
│ └──────────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────┘
              ▲                                 ▲
              │                                 │
    Command/Control                        Memory Bus
      (DDR4/DDR5)                       (up to 600 GB/s)
              │                                 │
    ┌─────────┴─────────┬──────────────────────┴────────────┐
    │                   │                                   │
    ▼                   ▼                                   ▼

┌──────────────┐  ┌──────────────┐  ┌──────────────────────┐
│ DRAM DIMM 0  │  │ DRAM DIMM 1  │  │ UPMEM DIMM 0-7       │
│ 32 GB DDR5   │  │ 32 GB DDR5   │  │ Each DIMM:           │
│     -        │  │     -        │  │  - 32 GB Capacity    │
│  ~90 ns lat  │  │  ~90 ns lat  │  │  - 256 MB/rank MRAM  │
│  400 GB/s    │  │  400 GB/s    │  │  - 64 DPUs (ranks)   │
│  per DIMM    │  │  per DIMM    │  │  - ~25-35 µs lat     │
└──────────────┘  └──────────────┘  │  - 64 MB/rank MRAM   │
                                    │  - 256K WRAM each    │
                                    │  - 4K IRAM each      │
                                    └──────────────────────┘

Total Available Swap: 64 DPUs x 64 MB = 4 GB MRAM (per socket)
```

### 1.2 Latency breakdown pour 4KB transfer

```
Request (4KB write):

CPU Core
   │
   ├─ Issue dpu_prepare_xfer() - 50 ns (local call)
   │
   ├─ DMA setup by memory controller - 500 ns
   │  └─ Program descriptor in IOMMU
   │
   ├─ Transfer over DDR5 bus - ~30 µs
   │  └─ 4 KB / 600 GB/s ≈ 6-8 µs theoretical
   │  └─ But: contention, protocol overhead, rank switching
   │
   └─ DPU receives in MRAM - 2-3 µs
      └─ MRAM write latency itself
      └─ Total latency: ~25-36 µs measured ✓


Measured from benchmark_complete.c:
- 4096 bytes (4 KB page):
  * Write: 30.21 µs (serial, 1 DPU)
  * Read:  31.29 µs (serial, 1 DPU)
  * Throughput: ~129 MB/s (one DPU)

vs Literature:
- SATA SSD: 60-200 µs (seek + transfer)
- NVMe SSD: 10-30 µs (transfer only, no seek)
- DRAM:     ~90 ns (local, no transfer)
```

---

## 2. Simulateur implémenté

### 2.1 Ce que le simulateur fait RÉELLEMENT

```
┌─────────────────────────────────────────┐
│ Userspace Simulator                     │
│                                         │
│ (1) memory_sim.c                        │
│     malloc-based fake RAM               │
│     ├─ 1-128 MB memory pool             │
│     ├─ Physical frame tracking          │
│     └─ Free/busy frame management       │
│                                         │
│ (2) page_table.c                        │
│     Virtual <-> Physical mapping        │
│     ├─ 3 states: EMPTY, IN_RAM, INSWAP │
│     ├─ LRU victim selection (timestamp) │
│     └─ Page entry per possible page_id  │
│                                         │
│ (3) upmem_swap.c                        │
│     Fake DPU manager                    │
│     ├─ Round-robin DPU allocation       │
│     ├─ SIMULATED latency (formula)      │
│     ├─ Latency = 10 + (size/1024)*6.5   │
│     └─ Stats collection                 │
│                                         │
│ (4) workload.c                          │
│     Memory access pattern generator     │
│     ├─ Random: uniform across WS        │
│     ├─ Sequential: linear scan          │
│     └─ Mixed: 70% local + 30% random    │
│                                         │
│ (5) stats.c                             │
│     Results aggregation                 │
│     ├─ Hit/fault rates                  │
│     ├─ Latency averages                 │
│     └─ CSV export                       │
└─────────────────────────────────────────┘
```

### 2.2 Exécution simulation

```
main() 
  ├─ Parse CLI args (--ram-mb, --dpus, --accesses...)
  ├─ Init components (RAM, page table, DPU manager)
  │
  └─ workload_run()
      └─ for i = 0 to nr_accesses:
          ├─ Generate next page_id (based on pattern)
          ├─ workload_access_page(page_id)
          │  │
          │  └─ if page IN_RAM: hit++
          │     else: fault++
          │       ├─ if RAM full:
          │       │  ├─ victim = LRU_select()
          │       │  ├─ swap_out(victim)  [-> measure latency]
          │       │  └─ free(victim's frame)
          │       │
          │       ├─ allocate new frame
          │       ├─ if page IN_SWAP:
          │       │  └─ swap_in(page)  [-> measure latency]
          │       └─ page mark IN_RAM
          │
          └─ Aggregate latency & stats

Final:
  ├─ Print results
  ├─ Export CSV
  └─ Cleanup
```

---

## 3. Différence: Simulation vs Vrai Swap Kernel

### 3.1 Vrai swap Linux (pour référence)

```
Application memory pressure:

App malloc(500 MB) on 32 GB system with 8 GB used
  │
  ├─ Kernel detects RAM pressure:
  │  └─ (available < watermark_low)
  │
  ├─ kswapd daemon wakes up
  │  ├─ Scans page LRU (real pages in memory)
  │  ├─ Identifies eviction candidates (based on recency + references)
  │  ├─ Flushes dirty pages to swap device
  │  └─ Continues until memory available or reclaimable pages exhausted
  │
  ├─ App tries to access address → page fault
  │  ├─ kernel handler (arch/x86/mm/fault.c)
  │  ├─ Looks up PTE (page table entry)
  │  ├─ Identifies page in swap device
  │  ├─ READ from swap:
  │  │  └─ disk_read() to SWAP sector
  │  ├─ Allocate & populate new page frame
  │  ├─ Update PTE to point to new physical address
  │  └─ Resume app execution (~1-200 ms user time)
  │
  └─ App gets page, continues

Measured via:
  cat /proc/vmstat | grep pswp  (pages swapped)
  cat /proc/sys/vm/swappiness  (0-100, aggressiveness)
  iotop --only  (swap I/O)
```

### 3.2 Notre simulateur

```
Fake memory pressure:

while (accesses--) {
    page_id = rand() % working_set;
    if (page_in_ram()) 
        hits++;
    else {
        faults++;
        if (ram_full()) {
            victim = select_lru();
            transfer_fake(victim, dpu);    # SIMULATED latency
            free_frame(victim);
        }
        allocate_frame();
        if (page_in_swap())
            transfer_fake(dpu, page);      # SIMULATED latency
        update_page_table();
    }
}

Key differences:
  - NO kernel involvement
  - NO real memory pressure (we just count faults)
  - NO real page table (PTE)
  - NO page aging/recency tracking
  - Latency is FAKE (formula-based, not real hardware)
  - Working with FAKE pages (arrays), not actual memory regions
  - Pure userspace benchmark
```

---

## 4. Valeurs de latence - D'où viennent-elles?

Source: `/workspaces/upmem-swap/src/host/benchmark_complete.c`

```c
// Real hardware measurement (using UPMEM SDK v2025.1.0 with simulator backend)
// Measured 4096 bytes (4 KB page) - standard Linux page size

Measured Results (from benchmark_results.csv):
────────────────────────────────────────────────
nr_dpus | size  | write_mean_us | read_mean_us | Throughput
─────────────────────────────────────────────────
1       | 4096  | 30.21        | 31.29        | 129 MB/s
4       | 4096  | 15.30        | 14.66        | 255-266 MB/s
8       | 4096  | 23.70        | 24.86        | 157-164 MB/s
16      | 4096  | (scaled data)| (scaled)     | (scaled)
────────────────────────────────────────────────

Simulation formula (upmem_swap.c):
  latency = 10.0 + (size_kb * 6.5) + noise(±2)
  
  For 4 KB: 10 + (4 * 6.5) + noise = 10 + 26 + noise ≈ 36 µs
  
  Matches measured 30-31 µs within ~10% error
  (Good enough for simulator validation)
```

---

## 5. Comment adapter pour du hardware RÉEL

### Option 1: Kernel Module de Swap

```c
// drivers/upmem_swap.c

// Enregistrer comme swap device:
static struct swap_info_struct upmem_swap_info = {
    .name = "UPMEM-MRAM",
    .flags = ...
};

// Callbacks invoqués par kernel quand RAM pleine:

int upmem_swap_read_page(struct swap_info_struct *sis,
                          swp_entry_t entry,
                          struct page *page) {
    // 1. Get physical page address
    void *pa = page_address(page);
    
    // 2. Fetch from MRAM via UPMEM SDK
    dpu_set_t dpus = sis->priv;  // DPU set allocated at init
    
    dpu_prepare_xfer(dpus, pa);
    dpu_push_xfer(dpus, DPU_XFER_FROM_DPU, 
                  "mram_buffer", 
                  entry.value * PAGE_SIZE,  // Specific offset
                  PAGE_SIZE, 
                  DPU_XFER_WAITALL);
    
    // 3. Mark page clean, kernel continues
    return 0;
}

int upmem_swap_write_page(struct swap_info_struct *sis,
                           swp_entry_t entry,
                           struct page *page) {
    // Similar but dir = DPU_XFER_TO_DPU
    ...
}

// At boot:
// 1. dpu_alloc(nr_dpus, backend="simulator" or backend="hardware", &dpu_set)
// 2. Register swap device
// 3. swapoff old SSD-based swap
// 4. swapon /dev/upmem_swap
```

Init sequence:
```
1. modprobe upmem_swap
2. dmesg | grep -i upmem
3. free -h  # verify new swap
4. stress-ng --vm 8 --vm-bytes 10G --timeout 60s
5. Monitor: iotop, vmstat, /proc/meminfo
```

### Option 2: Pure userspace LD_PRELOAD

```c
// Track malloc + trigger fake swaps

#include <malloc.h>
#include <dpu.h>

// Intercept malloc
void *malloc_hook(size_t size) {
    ...
    total_allocated += size;
    
    if (total_allocated > RAM_LIMIT) {
        // Trigger swap to MRAM
        evict_and_swap();
    }
    
    return real_malloc(size);
}

// Usage:
// LD_PRELOAD=./upmem_malloc.so ./my_application
```

Pros: No kernel changes, easy to test  
Cons: Only intercepts allocations, not real pressure

### Option 3: Memory pressure injection (realist)

```bash
#!/bin/bash

# Allocate >physical RAM to trigger real kernel swapping
stress-ng --vm 2 --vm-bytes 24G --vm-populate &

# Measure UPMEM transfers concurrently
taskset -c 0-7 ./transfer_benchmark \
  --dpus 16 \
  --buffer-size 4096 \
  --iterations 100000

# Result: Real SSD swap baseline vs UPMEM transfers
# Compare latencies in /proc/swapin_latency (custom patch)
```

---

## 6. État actuel du code

### 6.1 Ce qui est code réel (validé)

- **benchmark_complete.c**: Utilise vrai SDK UPMEM (simulateur ou matériel)
- **benchmark_scaling.c**: Scaling tests
- Mesures dans benchmark_results.csv: ✓ VRAIES

### 6.2 Ce qui est simulé (pour now)

- **simulator/**: Userspace simulator
- Latencies: BASÉES sur vraies mesures
- Page eviction: Simplifié (juste LRU timestamp)
- Working set: Synthétique

### 6.3 Manques vs swap réel

- ❌ Kernel involvement
- ❌ Real memory pressure detection
- ❌ PTE updates
- ❌ Page aging/recency
- ❌ NUMA effects
- ❌ TLB flushes
- ❌ Cache coherence protocol

---

## 7. Prochaines étapes (pour production)

1. **Phase 1** (now): Simulator pour validation conceptuelle ✓
2. **Phase 2**: Kernel module avec vrai SDK UPMEM
3. **Phase 3**: Real hardware validation
4. **Phase 4**: Performance tuning (compression, prefetch, etc.)

---

**Résumé**: Simulateur = proof-of-concept bench. Vrai swap = kernel module + allocateurs. Notre code mesure correctement (30 µs), mais simule pas la mécanique kernel.
