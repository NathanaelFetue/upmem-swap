#!/usr/bin/env python3
import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns
import numpy as np

# Set style
sns.set_style("whitegrid")
plt.rcParams['figure.figsize'] = (12, 8)

# Load data
df = pd.read_csv('../benchmark_results.csv')

# Create output directory at ROOT level
import os
os.makedirs('../plots', exist_ok=True)

# 1. Latency vs Size (by DPU count)
fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(15, 6))

for nr_dpus in df['nr_dpus'].unique():
    data = df[(df['nr_dpus'] == nr_dpus) & (df['mode'] == 'parallel') & (df['nr_tasklets'] == 1)]
    ax1.plot(data['size'], data['write_mean_us'], marker='o', label=f'{nr_dpus} DPUs')
    ax2.plot(data['size'], data['read_mean_us'], marker='o', label=f'{nr_dpus} DPUs')

ax1.set_xlabel('Transfer Size (bytes)')
ax1.set_ylabel('Latency (µs)')
ax1.set_title('WRITE Latency vs Size')
ax1.legend()
ax1.grid(True)

ax2.set_xlabel('Transfer Size (bytes)')
ax2.set_ylabel('Latency (µs)')
ax2.set_title('READ Latency vs Size')
ax2.legend()
ax2.grid(True)

plt.tight_layout()
plt.savefig('../plots/01_latency_vs_size.png', dpi=300)
plt.close()

# 2. Serial vs Parallel Comparison
fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(15, 6))

size = 4096  # Focus on 4KB
for mode in ['serial', 'parallel']:
    data = df[(df['size'] == size) & (df['mode'] == mode) & (df['nr_tasklets'] == 1)]
    ax1.plot(data['nr_dpus'], data['write_mean_us'], marker='o', label=mode.capitalize())
    ax2.plot(data['nr_dpus'], data['read_mean_us'], marker='o', label=mode.capitalize())

ax1.set_xlabel('Number of DPUs')
ax1.set_ylabel('Latency (µs)')
ax1.set_title(f'WRITE Latency: Serial vs Parallel (4KB)')
ax1.legend()
ax1.grid(True)

ax2.set_xlabel('Number of DPUs')
ax2.set_ylabel('Latency (µs)')
ax2.set_title(f'READ Latency: Serial vs Parallel (4KB)')
ax2.legend()
ax2.grid(True)

plt.tight_layout()
plt.savefig('../plots/02_serial_vs_parallel.png', dpi=300)
plt.close()

# 3. Throughput vs DPU count - ALL 5 SIZES (MODIFIED!)
fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(16, 6))

# Define colors and markers for each size
colors = {
    512: '#1f77b4',   # Blue
    1024: '#ff7f0e',  # Orange
    2048: '#2ca02c',  # Green
    4096: '#d62728',  # Red (emphasis on 4KB)
    8192: '#9467bd'   # Purple
}

markers = {
    512: 'o',
    1024: 's',
    2048: '^',
    4096: 'D',  # Diamond for 4KB
    8192: 'v'
}

sizes = sorted(df['size'].unique())

# Plot WRITE throughput
for size in sizes:
    data = df[(df['size'] == size) & (df['mode'] == 'parallel') & 
              (df['nr_tasklets'] == 1)].sort_values('nr_dpus')
    
    if len(data) > 0:
        size_kb = size // 1024 if size >= 1024 else size
        size_label = f'{size_kb}KB' if size >= 1024 else f'{size}B'
        
        ax1.plot(data['nr_dpus'], data['write_throughput_mbps'], 
                marker=markers.get(size, 'o'), 
                linewidth=2.5 if size == 4096 else 2,
                markersize=10 if size == 4096 else 8,
                label=size_label,
                color=colors.get(size, 'gray'),
                alpha=1.0 if size == 4096 else 0.85)

ax1.set_xlabel('Number of DPUs', fontsize=12, fontweight='bold')
ax1.set_ylabel('Throughput (MB/s)', fontsize=12, fontweight='bold')
ax1.set_title('WRITE Throughput - Parallel Mode (1 Tasklet)', fontsize=13, fontweight='bold')
ax1.legend(loc='lower right', fontsize=10, framealpha=0.95)
ax1.grid(True, alpha=0.3)
ax1.set_ylim(bottom=0)

# Plot READ throughput
for size in sizes:
    data = df[(df['size'] == size) & (df['mode'] == 'parallel') & 
              (df['nr_tasklets'] == 1)].sort_values('nr_dpus')
    
    if len(data) > 0:
        size_kb = size // 1024 if size >= 1024 else size
        size_label = f'{size_kb}KB' if size >= 1024 else f'{size}B'
        
        ax2.plot(data['nr_dpus'], data['read_throughput_mbps'], 
                marker=markers.get(size, 'o'), 
                linewidth=2.5 if size == 4096 else 2,
                markersize=10 if size == 4096 else 8,
                label=size_label,
                color=colors.get(size, 'gray'),
                alpha=1.0 if size == 4096 else 0.85)

ax2.set_xlabel('Number of DPUs', fontsize=12, fontweight='bold')
ax2.set_ylabel('Throughput (MB/s)', fontsize=12, fontweight='bold')
ax2.set_title('READ Throughput - Parallel Mode (1 Tasklet)', fontsize=13, fontweight='bold')
ax2.legend(loc='lower right', fontsize=10, framealpha=0.95)
ax2.grid(True, alpha=0.3)
ax2.set_ylim(bottom=0)

plt.suptitle('Aggregate Throughput: All 5 Transfer Sizes (Parallel, 1 Tasklet)', 
             fontsize=14, fontweight='bold', y=1.00)
plt.tight_layout()
plt.savefig('../plots/03_throughput_scaling.png', dpi=300, bbox_inches='tight')
plt.close()

# 4. Heatmap: DPUs × Size (Parallel Write)
pivot_data = df[(df['mode'] == 'parallel') & (df['nr_tasklets'] == 1)].pivot_table(
    values='write_mean_us', index='nr_dpus', columns='size', aggfunc='mean'
)

fig, ax = plt.subplots(figsize=(10, 6))
sns.heatmap(pivot_data, annot=True, fmt='.1f', cmap='YlOrRd', ax=ax)
ax.set_title('WRITE Latency Heatmap (µs): DPUs × Size (Parallel)')
ax.set_xlabel('Transfer Size (bytes)')
ax.set_ylabel('Number of DPUs')
plt.tight_layout()
plt.savefig('../plots/04_heatmap_write.png', dpi=300)
plt.close()

# 5. Tasklets Impact
fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(15, 6))

nr_dpus = 16
for nr_tasklets in df['nr_tasklets'].unique():
    data = df[(df['nr_dpus'] == nr_dpus) & (df['nr_tasklets'] == nr_tasklets) & 
              (df['mode'] == 'parallel')]
    ax1.plot(data['size'], data['write_mean_us'], marker='o', label=f'{nr_tasklets} tasklets')
    ax2.plot(data['size'], data['read_mean_us'], marker='o', label=f'{nr_tasklets} tasklets')

ax1.set_xlabel('Transfer Size (bytes)')
ax1.set_ylabel('Latency (µs)')
ax1.set_title(f'WRITE: Tasklets Impact ({nr_dpus} DPUs, Parallel)')
ax1.legend()
ax1.grid(True)

ax2.set_xlabel('Transfer Size (bytes)')
ax2.set_ylabel('Latency (µs)')
ax2.set_title(f'READ: Tasklets Impact ({nr_dpus} DPUs, Parallel)')
ax2.legend()
ax2.grid(True)

plt.tight_layout()
plt.savefig('../plots/05_tasklets_impact.png', dpi=300)
plt.close()

# 6. Speedup: Parallel vs Serial
fig, ax = plt.subplots(figsize=(12, 6))

for size in [4096, 8192]:
    serial_data = df[(df['size'] == size) & (df['mode'] == 'serial') & (df['nr_tasklets'] == 1)]
    parallel_data = df[(df['size'] == size) & (df['mode'] == 'parallel') & (df['nr_tasklets'] == 1)]
    
    speedup = serial_data['write_mean_us'].values / parallel_data['write_mean_us'].values
    ax.plot(serial_data['nr_dpus'].values, speedup, marker='o', label=f'{size}B')

ax.axhline(y=1, color='r', linestyle='--', label='No speedup')
ax.set_xlabel('Number of DPUs')
ax.set_ylabel('Speedup (Serial / Parallel)')
ax.set_title('Parallel Speedup vs Serial (WRITE)')
ax.legend()
ax.grid(True)
plt.tight_layout()
plt.savefig('../plots/06_speedup.png', dpi=300)
plt.close()

print("✓ All plots generated in ../plots/ (root level)")
print("\nGenerated plots:")
print("  01_latency_vs_size.png - Latency scaling with transfer size")
print("  02_serial_vs_parallel.png - Serial vs Parallel comparison")
print("  03_throughput_scaling.png - Aggregate throughput (ALL 5 SIZES!)")
print("  04_heatmap_write.png - Latency heatmap")
print("  05_tasklets_impact.png - Impact of tasklets")
print("  06_speedup.png - Parallel speedup")