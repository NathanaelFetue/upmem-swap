#!/bin/bash

# Comparison Script: Simulator vs Real Swap
# Shows the difference between simulated and actual swap latencies

echo "╔════════════════════════════════════════════════════════════════╗"
echo "║   UPMEM Swap: Simulator vs Real Measurement                   ║"
echo "╚════════════════════════════════════════════════════════════════╝"
echo ""

SIMULATOR="./simulator/swap_sim"
REAL_TEST="./test_real_swap"

if [ ! -f "$SIMULATOR" ]; then
    echo "❌ Simulator not found. Building..."
    cd simulator && make && cd ..
fi

if [ ! -f "$REAL_TEST" ]; then
    echo "❌ Real test not found. Compiling..."
    gcc -O2 -o test_real_swap test_real_swap.c -lm
fi

echo ""
echo "╔════════════════════════════════════════════════════════════════╗"
echo "║ PART 1: SIMULATOR (Virtual Swap Latencies)                    ║"
echo "╚════════════════════════════════════════════════════════════════╝"
echo ""
echo "Configuration: 2MB RAM, 4 DPUs, 1000 page working set"
echo "Expected: ~40 µs average (with kernel overhead)"
echo ""

$SIMULATOR --accesses 5000 --ram-mb 2 --dpus 4 --working-set 1000 --output results/compare_sim.csv 2>&1 | grep -A 30 "UPMEM Swap Simulator - Results"

echo ""
echo "╔════════════════════════════════════════════════════════════════╗"
echo "║ PART 2: REAL TEST (Actual Memory Allocation & Access)         ║"
echo "╚════════════════════════════════════════════════════════════════╝"
echo ""
echo "Configuration: 100MB allocation, access 20MB region randomly"
echo "Expected: ~0.1-5 µs (if in RAM), >1000 µs (if swapped)"
echo ""

$REAL_TEST --mb 100 --access-mb 20 2>&1 | grep -A 20 "=== RESULTS ==="

echo ""
echo "╔════════════════════════════════════════════════════════════════╗"
echo "║ COMPARISON INTERPRETATION                                      ║"
echo "╚════════════════════════════════════════════════════════════════╝"
echo ""
echo "Simulator predicts: ~40 µs"
echo "  └─ Includes kernel overhead (12 µs) + MRAM (6 µs) + transfer (12-34 µs)"
echo ""
echo "Real test shows:"
echo "  └─ If <1 µs: Pages in L3 cache (ultra-fast)"
echo "  └─ If ~1-10 µs: In main RAM (normal)"
echo "  └─ If ~40-100 µs: Could be UPMEM if implemented"
echo "  └─ If >1000 µs: Kernel swap to disk (slow)"
echo ""
echo "Key insight:"
echo "  When system has real swap enabled AND memory pressure,"
echo "  you'll see latencies >1000 µs (HDD swap)."
echo ""
echo "  With UPMEM, those would drop to ~40 µs."
echo ""

echo "Done. Check results/compare_sim.csv for CSV output."
