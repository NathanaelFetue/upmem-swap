# Guide: Adapter le swap UPMEM pour du hardware réel

## 1. État actuel (Simulator)

```
┌─────────────────────────────────────────────────┐
│ Userspace Benchmark (simulator/)                │
├─────────────────────────────────────────────────┤
│ - Fake RAM (malloc-based)                       │
│ - Fake page faults (synthetic accesses)         │
│ - Simulated latencies (formula-based)           │
│ - NO kernel involvement                         │
│ - Results: CSV with measured latencies          │
└─────────────────────────────────────────────────┘
```

**Utilité**: Valider concept avant hardware  
**Limitation**: Pas de vrai memory pressure, pas d'intégration kernel

---

## 2. Production Path: Kernel Module Swap Device

### 2.1 Architecture kernel

```
┌──────────────────────────────────────────────────────┐
│ Application (memcpy, malloc, fork, etc.)             │
├──────────────────────────────────────────────────────┤
│                                                       │
│ Virtual Address Space (per-process)                  │
│  └─ VAS layout:                                      │
│     ├─ [0x0000_0000 - 0x0800_0000] Stack (down)    │
│     ├─ [0x0800_0000 - 0x7000_0000] Heap (up)       │
│     ├─ [0x7000_0000 - 0x7fff_ffff] Libs            │
│     └─ [0xffff..., 0xffff...] Kernel (unmapped)    │
│                                                       │
│ Page Table (multi-level, arch-dependent)             │
│  └─ PTE fields:                                      │
│     ├─ P (Present): page in RAM?                    │
│     ├─ D (Dirty): modified?                         │
│     ├─ A (Accessed): used recently?                 │
│     ├─ PPN (Physical): physical frame number        │
│     └─ SW (Software): custom flags for swap info    │
│                                                       │
│ Linux Memory Management:                             │
│  ├─ mm_struct (per-process VAS descriptor)          │
│  ├─ vm_area_struct (VMA: contiguous regions)        │
│  ├─ page cache (disk ↔ RAM buffering)               │
│  └─ swap subsystem (defunct page ↔ swap device)     │
│                                                       │
├──────────────────────────────────────────────────────┤
│ Kernel Swap Layer (existing)                         │
│  ├─ mm/page_io.c: read_swap_page() / write_swap()   │
│  ├─ mm/swapfile.c: register swap device             │
│  ├─ /proc/swaps: active swap devices                │
│  └─ /sys/vm/swappiness: eviction aggressiveness     │
└──────────────────────────────────────────────────────┘
         │
         ├─ Traditional: swap_write_page() → /dev/sda3 (SSD)
         │
         └─ UPMEM: swap_write_page() → [OUR MODULE]
            ├─ Find available DPU rank
            ├─ dpu_prepare_xfer(dpu, page_addr)
            ├─ dpu_push_xfer(DPU_XFER_TO_DPU, ...)
            └─ Update PT[entry] = (dpu_id, offset)
```

### 2.2 Kernel module skeleton

```c
// File: drivers/upmemswap/upmem_swap.c

#include <linux/module.h>
#include <linux/swap.h>
#include <linux/mm.h>
#include <dpu.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("UPMEM PIM as Linux Swap Device");

// Global DPU set (allocated at module init)
static dpu_set_t *global_dpu_set = NULL;
static uint32_t nr_dpus = 0;
static struct mutex dpu_lock;

// Per-DPU MRAM offset tracking
struct dpu_rank_info {
    uint32_t dpu_id;
    uint64_t next_available_mram_offset;
    struct mutex alloc_lock;
};
static struct dpu_rank_info *rank_info = NULL;

// ====== SWAP DEVICE CALLBACKS ======

/**
 * Called by kernel when writing page to swap.
 * Page is identified by swp_entry_t (encodes swap device ID + offset).
 */
static int upmem_swap_writepage(struct page *page, 
                                 struct writeback_control *wbc)
{
    swp_entry_t entry = page_private(page);
    void *page_addr = page_address(page);  // Kernel virtual address
    uint64_t swap_offset = entry.val & SWAP_OFFSET_MASK;
    
    // Determine which DPU to use (round-robin)
    uint32_t dpu_id = swap_offset % nr_dpus;
    struct dpu_rank_info *rank = &rank_info[dpu_id];
    
    mutex_lock(&rank->alloc_lock);
    
    // Allocate space within MRAM of this rank
    uint64_t mram_offset = rank->next_available_mram_offset;
    if (mram_offset + PAGE_SIZE > DPU_MRAM_SIZE) {
        mutex_unlock(&rank->alloc_lock);
        return -ENOSPC;  // No space left
    }
    
    // Perform DMA transfer: RAM → DPU MRAM
    struct dpu_set_t dpu = {NULL};  // Your DPU handle
    int ret = dpu_prepare_xfer(&dpu, page_addr);
    if (ret) {
        mutex_unlock(&rank->alloc_lock);
        return ret;
    }
    
    ret = dpu_push_xfer(&dpu, DPU_XFER_TO_DPU,
                        "mram_buffer",        // MRAM symbol name
                        mram_offset,          // Offset within MRAM
                        PAGE_SIZE,
                        DPU_XFER_WAITALL);
    
    if (ret) {
        mutex_unlock(&rank->alloc_lock);
        return ret;
    }
    
    rank->next_available_mram_offset += PAGE_SIZE;
    
    // Update swap entry to point to our device + offset
    // Kernel stores this in PTE for future lookups
    entry.val = (dpu_id << 48) | mram_offset;  // Custom encoding
    set_page_private(page, entry.val);
    
    mutex_unlock(&rank->alloc_lock);
    return 0;  // Success
}

/**
 * Called by kernel when reading page from swap.
 * Page fault on swapped-out page triggers this.
 */
static int upmem_swap_readpage(struct file *sfile, struct page *page)
{
    swp_entry_t entry = page_private(page);
    void *page_addr = page_address(page);
    
    // Decode DPU + offset from swap entry
    uint32_t dpu_id = entry.val >> 48;
    uint64_t mram_offset = entry.val & 0xffffffffffff;
    
    struct dpu_set_t dpu = {NULL};  // Your DPU handle
    
    // Perform DMA transfer: DPU MRAM → RAM
    int ret = dpu_prepare_xfer(&dpu, page_addr);
    if (ret) return ret;
    
    ret = dpu_push_xfer(&dpu, DPU_XFER_FROM_DPU,
                        "mram_buffer",
                        mram_offset,
                        PAGE_SIZE,
                        DPU_XFER_WAITALL);
    if (ret) return ret;
    
    SetPageUptodate(page);  // Mark as valid
    unlock_page(page);      // Resume waiting process
    return 0;
}

// ====== MODULE INIT/EXIT ======

static int __init upmem_swap_init(void)
{
    int ret;
    
    printk(KERN_INFO "UPMEM Swap Module Loading\n");
    
    // 1. Allocate DPU set
    ret = dpu_alloc(NR_DPUS, "backend=simulator", &global_dpu_set);
    if (ret) {
        printk(KERN_ERR "Failed to allocate DPUs: %d\n", ret);
        return ret;
    }
    nr_dpus = NR_DPUS;
    
    // 2. Initialize per-rank tracking
    rank_info = kzalloc(nr_dpus * sizeof(*rank_info), GFP_KERNEL);
    if (!rank_info) {
        dpu_free(&global_dpu_set);
        return -ENOMEM;
    }
    
    // 3. Register as swap device
    struct address_space_operations upmem_aops = {
        .writepage = upmem_swap_writepage,
        .readpage = upmem_swap_readpage,
    };
    
    // Register with swap subsystem
    // This makes kernel call our readpage/writepage callbacks
    swap_info_struct_t *sis = add_swap_info(upmem_aops);
    if (!sis) {
        kfree(rank_info);
        dpu_free(&global_dpu_set);
        return -EINVAL;
    }
    
    printk(KERN_INFO "UPMEM Swap Device Ready (%d DPUs)\n", nr_dpus);
    return 0;
}

static void __exit upmem_swap_exit(void)
{
    // Unregister device
    // Flush any pending swaps
    // Free DPUs
    
    kfree(rank_info);
    if (global_dpu_set) dpu_free(&global_dpu_set);
    
    printk(KERN_INFO "UPMEM Swap Module Unloaded\n");
}

module_init(upmem_swap_init);
module_exit(upmem_swap_exit);
```

### 2.3 Enabling UPMEM swap on real system

```bash
# 1. Compile module
cd drivers/upmemswap/
make -C /lib/modules/$(uname -r)/build M=$(pwd) modules
sudo insmod upmem_swap.ko

# 2. Verify
cat /proc/swaps
# Output:
# Filename                    Type        Size    Used    Priority
# /dev/sda3 (SSD)             partition   4194304 123456  -1
# /dev/upmem_swap (UPMEM)     device      262144  0       -2

# 3. Stress test to verify swapping works
stress-ng --vm 4 --vm-bytes 16G --timeout 60s

# 4. Monitor
while true; do
    echo "=== SWAP USAGE ==="
    free -h
    vmstat 1 1 | tail -1  # pages swapped per sec
    echo ""
    sleep 2
done

# 5. Benchmark: Compare SSD vs UPMEM latencies
# (use ktime measurement in module readpage callback)
```

---

## 3. Linking with existing benchmarks

The simulator latencies match real measurements:

```
simulator/src/upmem_swap.c (simulation):
  latency = 10.0 + (size_kb * 6.5)
  
  For 4 KB: 36 µs simulated ✓

benchmark_complete.c (real SDK):
  For 4 KB: 30.21 µs write (measured) ✓
  
Kernel module:
  For 4 KB: ~30-36 µs expected (same hardware path) ✓
```

---

## 4. Integration checkpoints

| Phase | What | Where | Status |
|-------|------|-------|--------|
| 1 | Latency validation | `benchmark_complete.c` | ✅ DONE (30-31 µs) |
| 2 | Simulator | `simulator/` | ✅ DONE (36 µs approx) |
| 3 | Kernel module | TBD - `drivers/upmemswap/` | ⏳ TODO |
| 4 | Real hardware | Hardware with UPMEM DIMMs | ⏳ FUTURE |
| 5 | Production deployment | Linux swap rotation | ⏳ FUTURE |

---

## 5. Known limitations of simulator

- ❌ No real memory pressure (kernel doesn't trigger swaps)
- ❌ Synthetic workloads (not real application patterns)
- ❌ Simplified LRU (just timestamps, no accounting)
- ❌ No NUMA effects, cache behavior, TLB flushes
- ❌ No competing processes, IO scheduler contention
- ❌ Latencies = estimated, not real kernel paths

**Valid for**:
- ✅ Proving latency advantage conceptually
- ✅ Validating LRU behavior
- ✅ Benchmarking DPU throughput
- ✅ Dry-run before hardware

**Not valid for**:
- ❌ Production performance prediction
- ❌ Real memory-pressure behavior
- ❌ Multi-workload interactions
- ❌ Thermal/power effects

---

## 6. Hardware requirements

To run kernel module on real UPMEM system:

```
CPU: AMD EPYC 9005 series (Genoa) or later
    - Built-in UPMEM interface
    - 128x cores per socket
    - Configurable memory controllers

Memory: UPMEM DIMMs (any configuration)
    - 8x DIMMs per socket = 64 DPUs typical
    - 64 MB MRAM per DPU = 4 GB total per socket
    - Capacity layer (optional extra storage)

OS: Linux kernel 6.2+
    - Memory management refactor
    - Swap subsystem stable API

SDK: UPMEM SDK v2025.1.0+
    - dpu_prepare_xfer, dpu_push_xfer functions
    - Rank-based DPU allocation
    - MRAM direct access capability
```

---

## 7. Testing progression

```
Simulator (now):
  make -C simulator/
  ./simulator/swap_sim --accesses 10000
  # Validates: LRU, transfers, latencies
  
Kernel module (next):
  make -C drivers/upmemswap/
  stress-ng --vm 4 --vm-bytes 16G
  # Validates: real memory pressure, kernel integration
  
Hardware tests (future):
  On local UPMEM node:
    - SSD swap baseline
    - UPMEM swap testing
    - Comparative latency/throughput
    - Real application workloads
```

---

**Summary**: 
- Simulator = concept validation ✓
- Kernel module = production path (sketch provided)
- Hardware = final validation step (future)
