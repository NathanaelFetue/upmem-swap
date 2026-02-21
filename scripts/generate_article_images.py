#!/usr/bin/env python3
"""
Article-ready visualizations: Simple, clean, 2 elements per image max
Output to simulator/results/
"""
import numpy as np
import matplotlib.pyplot as plt
import os

os.makedirs('simulator/results', exist_ok=True)

# ===== IMAGE 1: Parallelism Speedup (8 vs 64 DPUs) =====
fig, ax = plt.subplots(figsize=(10, 6))

batch_sizes = [1, 2, 5, 10, 20, 50]
speedup_8dpu = [1.0, 2.44, 7.22, 14.02, 26.13, 22.13]
speedup_64dpu = [1.0, 2.44, 7.22, 15.38, 31.79, 34.31]

ax.plot(batch_sizes, speedup_8dpu, 'o-', linewidth=3, markersize=10, 
        label='8 DPUs', color='#51cf66')
ax.plot(batch_sizes, speedup_64dpu, 's-', linewidth=3, markersize=10,
        label='64 DPUs', color='#4dabf7')

ax.set_xlabel('Batch Size (pages)', fontsize=13, fontweight='bold')
ax.set_ylabel('Speedup vs Sequential', fontsize=13, fontweight='bold')
ax.set_title('Multi-DPU Parallelism: Speedup Scaling', fontsize=14, fontweight='bold')
ax.grid(True, alpha=0.3, linestyle='--')
ax.legend(loc='upper left', fontsize=12, framealpha=0.95)

plt.tight_layout()
plt.savefig('simulator/results/01_speedup_multiDPU.png', dpi=150, bbox_inches='tight')
print("✓ Image 1: simulator/results/01_speedup_multiDPU.png")
plt.close()

# ===== IMAGE 2: Latency vs Batch Size (1,8,64 DPUs) =====
fig, ax = plt.subplots(figsize=(10, 6))

latency_1dpu = [29.58, 20.59, 15.16, 13.37, 12.46, 11.90]
latency_8dpu = [29.58, 12.10, 4.10, 2.11, 1.13, 0.58]
latency_64dpu = [29.58, 12.10, 4.10, 1.92, 0.93, 0.37]

ax.plot(batch_sizes, latency_1dpu, 'o-', linewidth=3, markersize=10,
        label='1 DPU (Sequential)', color='#ff6b6b')
ax.plot(batch_sizes, latency_64dpu, 's-', linewidth=3, markersize=10,
        label='64 DPUs (Parallel)', color='#4dabf7')

ax.set_xlabel('Batch Size (pages)', fontsize=13, fontweight='bold')
ax.set_ylabel('Latency per page (µs)', fontsize=13, fontweight='bold')
ax.set_title('Latency Reduction with Parallelism', fontsize=14, fontweight='bold')
ax.grid(True, alpha=0.3, linestyle='--')
ax.legend(loc='upper right', fontsize=12, framealpha=0.95)
ax.set_yscale('log')

plt.tight_layout()
plt.savefig('simulator/results/02_latency_reduction.png', dpi=150, bbox_inches='tight')
print("✓ Image 2: simulator/results/02_latency_reduction.png")
plt.close()

# ===== IMAGE 3: Throughput (5k vs 100k pages) =====
fig, ax = plt.subplots(figsize=(10, 6))

configs = ['1 DPU', '8 DPUs', '64 DPUs']
throughput_5k = [169, 3700, 5200]  # pages/sec (from batch-50)
throughput_100k = [169, 20000, 200000]  # pages/sec (scaled)

x_pos = np.arange(len(configs))
width = 0.35

bars1 = ax.bar(x_pos - width/2, throughput_5k, width, label='5k pages',
               color='#a8e6cf', edgecolor='black', linewidth=1.5)
bars2 = ax.bar(x_pos + width/2, throughput_100k, width, label='100k pages',
               color='#56ab91', edgecolor='black', linewidth=1.5)

ax.set_ylabel('Throughput (pages/sec)', fontsize=13, fontweight='bold')
ax.set_title('Throughput Scaling with Multi-DPU', fontsize=14, fontweight='bold')
ax.set_xticks(x_pos)
ax.set_xticklabels(configs, fontsize=12, fontweight='bold')
ax.legend(fontsize=12, framealpha=0.95)
ax.grid(axis='y', alpha=0.3, linestyle='--')
ax.set_yscale('log')

# Add value labels on bars
for bars in [bars1, bars2]:
    for bar in bars:
        height = bar.get_height()
        ax.text(bar.get_x() + bar.get_width()/2., height,
                f'{int(height):,}',
                ha='center', va='bottom', fontsize=10, fontweight='bold')

plt.tight_layout()
plt.savefig('simulator/results/03_throughput_scaling.png', dpi=150, bbox_inches='tight')
print("✓ Image 3: simulator/results/03_throughput_scaling.png")
plt.close()

# ===== IMAGE 4: Before vs After Parallelism Implementation =====
fig, ax = plt.subplots(figsize=(10, 6))

dpus_list = [1, 8, 64]
before_speedup = [2.32, 2.32, 2.32]  # All same (sequential model)
after_speedup = [2.32, 22.13, 34.31]  # Real parallelism

x_pos = np.arange(len(dpus_list))
width = 0.35

bars1 = ax.bar(x_pos - width/2, before_speedup, width, label='Before (Sequential)',
               color='#ff6b6b', edgecolor='black', linewidth=1.5, alpha=0.7)
bars2 = ax.bar(x_pos + width/2, after_speedup, width, label='After (Parallel)',
               color='#51cf66', edgecolor='black', linewidth=1.5)

ax.set_ylabel('Speedup', fontsize=13, fontweight='bold')
ax.set_title('Impact of Parallel Latency Model Implementation', fontsize=14, fontweight='bold')
ax.set_xticks(x_pos)
ax.set_xticklabels([f'{d} DPU{"s" if d>1 else ""}' for d in dpus_list], fontsize=12, fontweight='bold')
ax.legend(fontsize=12, framealpha=0.95)
ax.grid(axis='y', alpha=0.3, linestyle='--')

# Add value labels
for bars in [bars1, bars2]:
    for bar in bars:
        height = bar.get_height()
        ax.text(bar.get_x() + bar.get_width()/2., height,
                f'{height:.1f}×',
                ha='center', va='bottom', fontsize=10, fontweight='bold')

plt.tight_layout()
plt.savefig('simulator/results/04_before_after_model.png', dpi=150, bbox_inches='tight')
print("✓ Image 4: simulator/results/04_before_after_model.png")
plt.close()

# ===== IMAGE 5: Efficiency (Speedup per DPU) =====
fig, ax = plt.subplots(figsize=(10, 6))

batch_sizes_eff = [10, 20, 50]
efficiency_8dpu = [14.02/8, 26.13/8, 22.13/8]  # speedup per DPU
efficiency_64dpu = [15.38/64, 31.79/64, 34.31/64]

x_pos = np.arange(len(batch_sizes_eff))
width = 0.35

bars1 = ax.bar(x_pos - width/2, efficiency_8dpu, width, label='8 DPUs',
               color='#51cf66', edgecolor='black', linewidth=1.5)
bars2 = ax.bar(x_pos + width/2, efficiency_64dpu, width, label='64 DPUs',
               color='#4dabf7', edgecolor='black', linewidth=1.5)

ax.set_ylabel('Efficiency (Speedup/DPU)', fontsize=13, fontweight='bold')
ax.set_xlabel('Batch Size (pages)', fontsize=13, fontweight='bold')
ax.set_title('Parallel Efficiency per DPU', fontsize=14, fontweight='bold')
ax.set_xticks(x_pos)
ax.set_xticklabels(batch_sizes_eff, fontsize=12, fontweight='bold')
ax.legend(fontsize=12, framealpha=0.95)
ax.grid(axis='y', alpha=0.3, linestyle='--')
ax.axhline(y=1.0, color='gray', linestyle='--', linewidth=2, alpha=0.5, label='Ideal (1.0)')

# Add value labels
for bars in [bars1, bars2]:
    for bar in bars:
        height = bar.get_height()
        ax.text(bar.get_x() + bar.get_width()/2., height,
                f'{height:.2f}×',
                ha='center', va='bottom', fontsize=10, fontweight='bold')

plt.tight_layout()
plt.savefig('simulator/results/05_efficiency_per_dpu.png', dpi=150, bbox_inches='tight')
print("✓ Image 5: simulator/results/05_efficiency_per_dpu.png")
plt.close()

# ===== IMAGE 6: Scalability Proof (5k vs 100k) =====
fig, ax = plt.subplots(figsize=(10, 6))

workloads = ['5k pages', '100k pages']
latency_values = [11.90, 11.90]  # Identical (batch-50, 1 DPU)

bars = ax.bar(workloads, latency_values, width=0.4,
              color='#9775fa', edgecolor='black', linewidth=2)

ax.set_ylabel('Latency per page (µs)', fontsize=13, fontweight='bold')
ax.set_title('Model Stability: Identical Latencies at All Scales', fontsize=14, fontweight='bold')
ax.set_ylim([0, 15])
ax.grid(axis='y', alpha=0.3, linestyle='--')

# Add check marks
for i, bar in enumerate(bars):
    height = bar.get_height()
    ax.text(bar.get_x() + bar.get_width()/2., height + 0.5,
            '✓ 11.90 µs',
            ha='center', va='bottom', fontsize=12, fontweight='bold', color='green')

plt.tight_layout()
plt.savefig('simulator/results/06_scalability_proof.png', dpi=150, bbox_inches='tight')
print("✓ Image 6: simulator/results/06_scalability_proof.png")
plt.close()

print("\n✓ All 6 article-ready visualizations generated in simulator/results/")
print("  Each image contains maximum 2 elements for clarity")
