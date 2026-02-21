#!/bin/bash

# Validation benchmark: test with constrained RAM to force swaps
echo "=== UPMEM Swap Simulator - Validation Benchmark ==="
echo "Testing with memory pressure to measure realistic swap latencies"
echo ""

# Test 1: Small RAM forces heavy swapping
echo "Test 1: Memory pressure scenario (2MB RAM, 1000 page working set)"
echo "Expected: 2500+ page faults, realistic swap latencies"
./swap_sim --accesses 5000 --ram-mb 2 --dpus 4 --working-set 1000 --output results/pressure_2mb.csv

# Test 2: Medium pressure
echo ""
echo "Test 2: Medium memory pressure (8MB RAM, 2000 page working set)"
./swap_sim --accesses 10000 --ram-mb 8 --dpus 4 --working-set 2000 --output results/pressure_8mb.csv

# Test 3: Moderate pressure  
echo ""
echo "Test 3: Moderate pressure (16MB RAM, 4000 page working set)"
./swap_sim --accesses 10000 --ram-mb 16 --dpus 4 --working-set 4000 --output results/pressure_16mb.csv

# Test 4: Varied DPU configurations
echo ""
echo "Test 4: Scaling with DPU count (4MB RAM, 2000 pages)"
for dpus in 1 2 4 8; do
    echo "  Testing with $dpus DPUs..."
    ./swap_sim --accesses 5000 --ram-mb 4 --dpus $dpus --working-set 2000 \
        --output results/scaling_${dpus}dpu.csv
done

echo ""
echo "=== Validation Complete ==="
echo "Results in: results/*.csv"
ls -lh results/*.csv | awk '{print $9, "(" $5 ")"}'
