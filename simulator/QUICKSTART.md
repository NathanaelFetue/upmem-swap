# UPMEM Swap Simulator - Getting Started

## ⚡ Quick Start (2 minutes)

```bash
cd simulator
make
./swap_sim
```

You'll see:
- Configuration details
- Live progress bar
- Results with swap latencies
- CSV export to `results/swap_sim.csv`

## 📊 See Real Swap in Action

```bash
./swap_sim --ram-mb 1 --dpus 8 --accesses 5000 --working-set 2000
```

Expected output:
```
Avg swap out latency: 35.99 µs
Avg swap in latency: 36.01 µs
Speedup vs SSD: 2.78×
```

## 🧪 Run All Tests

```bash
make test_patterns      # Random, sequential, mixed
make test_scaling       # 1, 4, 8, 16 DPUs
make test_with_swap     # Demonstrates actual swapping
python3 analyze_results.py  # Analyze all results
```

## 📁 What's Inside

- **src/main.c** - Entry point with CLI
- **src/memory_sim.c** - Simulates limited RAM
- **src/page_table.c** - Page table + LRU
- **src/upmem_swap.c** - Swap manager
- **src/workload.c** - Workload patterns
- **src/stats.c** - Results export

## 🎯 Key Results

| Test Case | Swap Latency | vs SSD |
|-----------|--------------|--------|
| Sequential, 16 DPUs | 36.09 µs | 2.8× faster |
| Small RAM (1 MB) | 35.99 µs | 2.8× faster |
| **Literature SSD** | **60-200 µs** | baseline |
| **Literature zram** | **20-50 µs** | comparable |

## 📖 Full Documentation

See [README.md](README.md) and [PROJECT_STATUS.md](PROJECT_STATUS.md)

## ✅ Status

- ✅ **Axis 1 (Fast Swap)**: COMPLETE
- ⏰ **Axis 2 (Compression)**: Future work
