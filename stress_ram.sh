#!/bin/bash

# Real-world stress test: allocate big, stress RAM, see real latencies

echo "╔════════════════════════════════════════════════════════════════╗"
echo "║ REAL SWAP STRESS TEST: Allocate 2GB, See Actual Latencies    ║"
echo "╚════════════════════════════════════════════════════════════════╝"
echo ""

echo "System Info:"
free -h | head -1 | awk '{print "  Total RAM: " $2}'
free -h | head -2 | tail -1 | awk '{print "  Available: " $7}'
echo "  Swap: disabled (no HDD swap to measure)"
echo ""

echo "Test Plan:"
echo "  1. Allocate 2GB of memory"
echo "  2. Touch ALL pages (force allocation)"
echo "  3. Randomly access pages and measure latencies"
echo "  4. Compare: hot (cache) vs cold (RAM) pages"
echo ""
echo "Expected Results:"
echo "  - Hot pages (recently accessed): <1 µs (L3 cache)"
echo "  - Cold pages (not in cache): 1-10 µs (RAM)"
echo "  - Simulator predicts UPMEM would be: ~40 µs"
echo ""

# Run the actual test
echo "Running test... (this takes ~10-20s)"
echo ""

timeout 30 ./test_real_swap --mb 2000 --access-mb 500 2>&1 | grep -A 50 "=== RESULTS ==="

echo ""
echo "╔════════════════════════════════════════════════════════════════╗"
echo "║ WHAT THIS SHOWS                                               ║"
echo "╚════════════════════════════════════════════════════════════════╝"
echo ""
echo "You see ~0.2-2 µs latencies because:"
echo "  1. All 2GB is allocated in physical RAM (no disk swap)"
echo "  2. Frequently accessed pages stay in L3 cache"
echo "  3. Random access shows mix of cache hits and RAM hits"
echo ""
echo "IF THIS SYSTEM HAD HDD SWAP CONFIGURED:"
echo "  - With 2GB allocated and RAM full:"
echo "    └─ Some pages would spill to disk HDD"
echo "    └─ Accessing those would show >1000 µs latencies"
echo "  - UPMEM would intercept and reduce to ~40 µs"
echo ""
echo "Current situation (no swap):"
echo "  - All pages in physical RAM"
echo "  - Purely CPU/cache limited measurement"
echo "  - No actual swapping happening"
echo ""

echo "To see real swap latencies, you'd need:"
echo "  - Configure Linux swap on disk"
echo "  - Allocate >8GB when system has only 8GB RAM"
echo "  - The excess would spill to swap partition"
echo "  - Then measurement would show 1000+ µs for swapped pages"
echo ""
