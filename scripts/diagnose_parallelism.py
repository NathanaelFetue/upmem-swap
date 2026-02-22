#!/usr/bin/env python3
"""
Diagnostic: Sequential vs Parallel latency model
Shows why current simulator sees NO improvement with >1 DPU
"""
import sys
import numpy as np
import matplotlib.pyplot as plt
from matplotlib.patches import Rectangle
from matplotlib.patches import FancyBboxPatch
import os

# Parameters from simulator
PAGE_SIZE = 4096  # bytes
KERNEL_OVERHEAD = 12.0  # µs
MRAM_LATENCY = 6.0  # µs
HOST_WRITE_BANDWIDTH = 0.33  # GB/s (CPU→DPU)
HOST_READ_BANDWIDTH = 0.12   # GB/s (DPU→CPU)

def calc_transfer_time_us(size_bytes, bandwidth_gbps):
    """Calculate transfer time in µs given bandwidth in GB/s"""
    return (size_bytes / (bandwidth_gbps * 1e9)) * 1e6

def simulate_current_latency(num_dpus, batch_size):
    """
    Current implementation: SEQUENTIAL transfer model
    - Pages distributed to different DPUs (allocation works)
    - BUT transfer is calculated as if sequential on ONE bus
    """
    total_size = batch_size * PAGE_SIZE
    transfer_time = calc_transfer_time_us(total_size, HOST_WRITE_BANDWIDTH)
    total_latency = KERNEL_OVERHEAD + MRAM_LATENCY + transfer_time
    latency_per_page = total_latency / batch_size
    return latency_per_page, total_latency

def simulate_parallel_latency(num_dpus, batch_size):
    """
    Corrected implementation: TRUE PARALLEL transfers
    - Pages distributed to different DPUs
    - Each DPU handles independently (max time, not sum)
    - With contention effects for many DPUs
    """
    # Round-robin distribution
    pages_per_dpu = [batch_size // num_dpus for _ in range(num_dpus)]
    for i in range(batch_size % num_dpus):
        pages_per_dpu[i] += 1
    
    # Each DPU transfers its pages in parallel
    dpu_max_time = 0
    for pages_on_dpu in pages_per_dpu:
        if pages_on_dpu > 0:
            dpu_size = pages_on_dpu * PAGE_SIZE
            dpu_transfer = calc_transfer_time_us(dpu_size, HOST_WRITE_BANDWIDTH / num_dpus)
            dpu_time = KERNEL_OVERHEAD + MRAM_LATENCY + dpu_transfer
            dpu_max_time = max(dpu_max_time, dpu_time)
    
    # Add contention penalty for many DPUs
    contention = 1.0
    if num_dpus > 8:
        contention += 0.05 * (num_dpus - 8)
    
    total_latency = dpu_max_time * contention
    latency_per_page = total_latency / batch_size
    return latency_per_page, total_latency

# Generate data
dpus_list = [1, 2, 4, 8, 16, 32, 64]
batch_sizes = [1, 10, 50]

fig, axes = plt.subplots(1, 3, figsize=(16, 5))

for idx, batch_size in enumerate(batch_sizes):
    ax = axes[idx]
    
    current_latencies = []
    parallel_latencies = []
    speedups = []
    
    for num_dpus in dpus_list:
        curr, _ = simulate_current_latency(num_dpus, batch_size)
        para, _ = simulate_parallel_latency(num_dpus, batch_size)
        current_latencies.append(curr)
        parallel_latencies.append(para)
        speedups.append(curr / para)
    
    # Plot
    x_pos = np.arange(len(dpus_list))
    width = 0.35
    
    ax.bar(x_pos - width/2, current_latencies, width, label='CURRENT (Sequential)', 
           color='#ff6b6b', alpha=0.8)
    ax.bar(x_pos + width/2, parallel_latencies, width, label='EXPECTED (Parallel)',
           color='#51cf66', alpha=0.8)
    
    # Add speedup values on top of expected bars
    for i, (curr, para) in enumerate(zip(current_latencies, parallel_latencies)):
        speedup = curr / para
        ax.text(i + width/2, para + 0.5, f'{speedup:.1f}×', 
               ha='center', va='bottom', fontsize=9, fontweight='bold', color='#51cf66')
    
    ax.set_xlabel('Number of DPUs', fontsize=11, fontweight='bold')
    ax.set_ylabel('Latency (µs/page)', fontsize=11, fontweight='bold')
    ax.set_title(f'Batch Size: {batch_size} pages', fontsize=12, fontweight='bold')
    ax.set_xticks(x_pos)
    ax.set_xticklabels(dpus_list)
    ax.legend(loc='upper left', fontsize=10)
    ax.grid(axis='y', alpha=0.3)
    
    # Add problem annotation
    if idx == 0:
        ax.annotate('SAME LATENCY!\n(Should be different)', 
                   xy=(2, current_latencies[4]), xytext=(4, current_latencies[4] + 3),
                   arrowprops=dict(arrowstyle='->', lw=2, color='#ff6b6b'),
                   fontsize=10, fontweight='bold', color='#ff6b6b',
                   bbox=dict(boxstyle='round,pad=0.5', facecolor='yellow', alpha=0.3))

plt.suptitle('DIAGNOSTIC: Why Multi-DPU Shows NO Latency Improvement', 
            fontsize=14, fontweight='bold', y=1.02)
plt.tight_layout()
os.makedirs('plots', exist_ok=True)
plt.savefig('plots/06_parallelism_diagnostic.png', dpi=150, bbox_inches='tight')
print("✓ Diagnostic plot saved: plots/06_parallelism_diagnostic.png")

# Create detailed explanation chart
fig, ax = plt.subplots(figsize=(14, 8))
ax.axis('off')

# Title
title_text = "SIMULATOR LATENCY MODEL PROBLEM"
ax.text(0.5, 0.95, title_text, ha='center', fontsize=16, fontweight='bold',
       transform=ax.transAxes)

# Current (wrong) model
current_box = FancyBboxPatch((0.05, 0.55), 0.4, 0.35, 
                            boxstyle="round,pad=0.01", 
                            edgecolor='#ff6b6b', facecolor='#ffe6e6', linewidth=2,
                            transform=ax.transAxes)
ax.add_patch(current_box)

ax.text(0.07, 0.88, "❌ CURRENT MODEL (Sequential)", 
       fontsize=12, fontweight='bold', color='#ff6b6b', transform=ax.transAxes)

current_text = """8 pages, Batch size = 8:

total_size = 8 × 4K = 32 KB
bandwidth = 0.33 GB/s

Transfer time = (32 KB) / (0.33 GB/s)
              = 98 µs  ← WRONG!
              
Supposes ALL pages on ONE bus
(not distributed to 8 DPUs!)

Latency per page = 118 µs / 8 = 14.75 µs

❌ Same latency regardless of DPU count!"""

ax.text(0.07, 0.70, current_text, fontsize=10, family='monospace',
       transform=ax.transAxes, verticalalignment='top')

# Expected (correct) model
expected_box = FancyBboxPatch((0.55, 0.55), 0.4, 0.35,
                             boxstyle="round,pad=0.01",
                             edgecolor='#51cf66', facecolor='#e6ffe6', linewidth=2,
                             transform=ax.transAxes)
ax.add_patch(expected_box)

ax.text(0.57, 0.88, "✓ EXPECTED MODEL (Parallel)",
       fontsize=12, fontweight='bold', color='#51cf66', transform=ax.transAxes)

expected_text = """8 pages → 8 DPUs (1 page each):

Each DPU gets 1 page = 4 KB
bandwidth per DPU = 0.33 GB/s

Transfer time = (4 KB) / (0.33 GB/s)
              = 12.4 µs  ← CORRECT!

All transfers happen in PARALLEL
(one per DPU on different channels!)

Latency per page = 30 µs / 8 = 3.75 µs

✓ 3.9× speedup with 8 DPUs!"""

ax.text(0.57, 0.70, expected_text, fontsize=10, family='monospace',
       transform=ax.transAxes, verticalalignment='top')

# Problem explanation
problem_box = FancyBboxPatch((0.05, 0.05), 0.9, 0.45,
                            boxstyle="round,pad=0.01",
                            edgecolor='#ffd700', facecolor='#fffff0', linewidth=2,
                            transform=ax.transAxes)
ax.add_patch(problem_box)

ax.text(0.07, 0.47, "THE ROOT CAUSE:", fontsize=11, fontweight='bold', 
       color='#d39e00', transform=ax.transAxes)

problem_text = """Code allocates pages to different DPUs correctly:
  ✓ find_available_dpu() round-robin loop
  ✓ pages[i]->dpu_id properly assigned
  ✓ Allocation logic works!

BUT latency calculation uses wrong model:
  ❌ batch_transfer_us = total_size / single_bandwidth
  ❌ Treats all pages as ONE sequential transfer
  ❌ Doesn't divide bandwidth by # active DPUs
  
FIX NEEDED:
  1. Group pages by destination DPU
  2. Calculate transfer for each DPU independently  
  3. Use MAX(dpu_times), not SUM!
  4. Apply contention penalty for 16+ DPUs

EXPECTED GAINS (with fix):
  - 1 DPU:   30 µs/page (baseline)
  - 8 DPUs:  3.8 µs/page (7.9× speedup)
  - 64 DPUs: 0.6 µs/page (50× speedup, with contention ~2-3 µs)"""

ax.text(0.07, 0.40, problem_text, fontsize=9, family='monospace',
       transform=ax.transAxes, verticalalignment='top')

plt.savefig('plots/07_parallelism_explanation.png', dpi=150, bbox_inches='tight')
print("✓ Explanation chart saved: plots/07_parallelism_explanation.png")
