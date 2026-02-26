#!/usr/bin/env python3
"""
Figures FINALES avec annotations pertinentes:
- Figure 2: Speedup values ON bars
- Figure 3: Names sans crochets
"""
import numpy as np
import matplotlib.pyplot as plt
import os
import csv

os.makedirs('simulator/results', exist_ok=True)

PAGE_SIZE = 4096
KERNEL_OVERHEAD_US = 12.0
MRAM_LATENCY_US = 6.0
HOST_WRITE_BW_GBS = 0.33
HOST_READ_BW_GBS = 0.12
ALPHA_WRITE = 0.722
ALPHA_READ = 0.875


def read_upmem_swapin_values():
    base = 'simulator/results'
    mapping = {
        'baseline': f'{base}/optionC.csv',
        'cpu': f'{base}/optionA.csv',
        'dpu': f'{base}/optionB.csv',
    }
    values = {}
    for key, path in mapping.items():
        with open(path, newline='') as f:
            row = next(csv.DictReader(f))
            values[key] = float(row['avg_swapin_us'])
    return values


def transfer_us(size_bytes, bandwidth_gbs):
    if bandwidth_gbs <= 0:
        return 0.0
    return (size_bytes / (bandwidth_gbs * 1e9)) * 1e6


def compute_modeled_speedup(dpus, batch_pages=50):
    total_bytes = batch_pages * PAGE_SIZE
    baseline_write = KERNEL_OVERHEAD_US + MRAM_LATENCY_US + transfer_us(total_bytes, HOST_WRITE_BW_GBS)
    baseline_read = KERNEL_OVERHEAD_US + MRAM_LATENCY_US + transfer_us(total_bytes, HOST_READ_BW_GBS)

    write_speedup = []
    read_speedup = []
    write_lat = []
    read_lat = []

    for dpu in dpus:
        write_factor = float(dpu) ** ALPHA_WRITE
        read_factor = float(dpu) ** ALPHA_READ
        write_t = KERNEL_OVERHEAD_US + MRAM_LATENCY_US + transfer_us(total_bytes, HOST_WRITE_BW_GBS * write_factor)
        read_t = KERNEL_OVERHEAD_US + MRAM_LATENCY_US + transfer_us(total_bytes, HOST_READ_BW_GBS * read_factor)
        write_lat.append(write_t)
        read_lat.append(read_t)
        write_speedup.append(baseline_write / write_t)
        read_speedup.append(baseline_read / read_t)

    return np.array(write_lat), np.array(read_lat), np.array(write_speedup), np.array(read_speedup)


def write_modeled_speedup_csv(path, dpus, avg_swapout_us, avg_swapin_us, write_speedup, read_speedup):
    with open(path, 'w', newline='') as f:
        writer = csv.writer(f)
        writer.writerow(['dpus', 'avg_swapout_us', 'avg_swapin_us', 'write_speedup', 'read_speedup'])
        for i, dpu in enumerate(dpus):
            writer.writerow([
                int(dpu),
                f'{avg_swapout_us[i]:.4f}',
                f'{avg_swapin_us[i]:.4f}',
                f'{write_speedup[i]:.4f}',
                f'{read_speedup[i]:.4f}'
            ])


def read_speedup_values():
    path = 'simulator/results/speedup_data.csv'
    default_dpus = np.array([1, 8, 16, 32, 64])

    dpus = default_dpus
    if os.path.exists(path):
        loaded = []
        with open(path, newline='') as f:
            reader = csv.DictReader(f)
            for row in reader:
                loaded.append(int(row['dpus']))
        if loaded:
            dpus = np.array(loaded)

    avg_w, avg_r, w, r = compute_modeled_speedup(dpus, batch_pages=50)
    write_modeled_speedup_csv(path, dpus, avg_w, avg_r, w, r)
    return dpus, w, r

# ============= FIGURE 1: ARCHITECTURE (unchanged) =============
fig = plt.figure(figsize=(10, 8))
ax = fig.add_subplot(111)
ax.set_xlim(0, 10)
ax.set_ylim(0, 10)
ax.axis('off')

ax.text(5, 9.7, 'Architecture du Système de Swap UPMEM', 
        fontsize=13, fontweight='bold', ha='center')

from matplotlib.patches import FancyBboxPatch, FancyArrowPatch, Rectangle

app_box = FancyBboxPatch((1.5, 7.8), 7, 1.2, boxstyle="round,pad=0.1",
                         edgecolor='#222222', facecolor='#e8e8e8', linewidth=2)
ax.add_patch(app_box)
ax.text(5, 8.4, 'Application (Userspace)', fontsize=11, fontweight='bold', ha='center')

sdk_y = 5.2
sdk_box = FancyBboxPatch((0.5, sdk_y), 9, 2.2, boxstyle="round,pad=0.1",
                         edgecolor='#333333', facecolor='#f2f2f2', linewidth=2)
ax.add_patch(sdk_box)
ax.text(5, sdk_y + 1.95, 'SDK UPMEM (Transfert Parallèle)', fontsize=11, fontweight='bold', ha='center')

components = [
    (1.2, sdk_y + 1.3, "Sélection\nVictime LRU"),
    (3.5, sdk_y + 1.3, "Lookup\nTable Pages"),
    (5.8, sdk_y + 1.3, "Contrôleur\nTransfert"),
    (8.1, sdk_y + 1.3, "Mode\nPARRALLÈLE")
]

for x, y, label in components:
    comp_box = Rectangle((x-0.45, y-0.35), 0.9, 0.7,
                         edgecolor='#444444', facecolor='#d9d9d9', linewidth=1.5)
    ax.add_patch(comp_box)
    ax.text(x, y, label, fontsize=8, ha='center', va='center', fontweight='bold')

arrow1 = FancyArrowPatch((5, 7.8), (5, 7.4),
                        arrowstyle='->', mutation_scale=25, linewidth=2.5, color='#222222')
ax.add_patch(arrow1)

dpu_y = 2.5
ax.text(5, dpu_y + 1.8, '64 DPUs en Parallèle (Round-Robin Allocation)', 
        fontsize=10, fontweight='bold', ha='center')

for i in range(8):
    x_pos = 0.8 + i * 1.1
    dpu_small = Rectangle((x_pos, dpu_y), 0.9, 0.8,
                          edgecolor='#222222', facecolor='#ededed', linewidth=1.5)
    ax.add_patch(dpu_small)
    ax.text(x_pos + 0.45, dpu_y + 0.4, f'DPU{i}', fontsize=7, ha='center', va='center', fontweight='bold')

ax.text(8.3, dpu_y + 0.4, '...', fontsize=10, fontweight='bold', ha='center', va='center')
ax.text(9.0, dpu_y + 0.4, 'DPU63', fontsize=7, ha='center', va='center', fontweight='bold')

arrow2 = FancyArrowPatch((5, sdk_y), (5, dpu_y + 0.8),
                        arrowstyle='->', mutation_scale=25, linewidth=2.5, color='#333333')
ax.add_patch(arrow2)

ax.text(5, 1.2, 'Mapping: page_id → (dpu_id, dpu_offset) | Allocation: Round-robin',
        fontsize=9, ha='center', fontweight='bold')

plt.tight_layout()
plt.savefig('simulator/results/Figure1_Architecture.png', dpi=150, bbox_inches='tight', facecolor='white')
print("✓ Figure 1: Architecture")
plt.close()

# ============= FIGURE 2: SPEEDUP avec VALUES =============
fig, ax = plt.subplots(figsize=(6, 3), dpi=100)

dpus, write_speedup, read_speedup = read_speedup_values()

# tighter grouping so bars remain legible in article scale
x_pos = np.arange(len(dpus)) * 0.82
width = 0.40

bars_write = ax.bar(x_pos - width/2, write_speedup, width,
                    label='WRITE', color='#4d4d4d', edgecolor='black', linewidth=1)
bars_read = ax.bar(x_pos + width/2, read_speedup, width,
                   label='READ', color='#a6a6a6', edgecolor='black', linewidth=1)

# Add values anchored to actual bar geometry (prevents shifted labels)
show_all_labels = max(np.max(write_speedup), np.max(read_speedup)) <= 2.0
for bar in bars_write:
    h = bar.get_height()
    if show_all_labels or h > 1.5:
        ax.text(bar.get_x() + bar.get_width() / 2.0, h + (0.04 if show_all_labels else 0.35), f'{h:.2f}x' if show_all_labels else f'{h:.1f}x',
                ha='center', va='bottom', fontsize=8, fontweight='bold')
for bar in bars_read:
    h = bar.get_height()
    if show_all_labels or h > 1.5:
        ax.text(bar.get_x() + bar.get_width() / 2.0, h + (0.04 if show_all_labels else 0.35), f'{h:.2f}x' if show_all_labels else f'{h:.1f}x',
                ha='center', va='bottom', fontsize=8, fontweight='bold')

ax.set_xlabel('Number of DPUs', fontsize=9, fontweight='bold')
ax.set_ylabel('Speedup factor', fontsize=9, fontweight='bold')
ax.set_title('Parallel vs Sequential Speedup (contention model)', fontsize=10, fontweight='bold')
ax.set_xticks(x_pos)
ax.set_xticklabels(dpus)
ax.legend(fontsize=8, loc='upper left')
ax.grid(axis='y', alpha=0.2, linestyle='--')
ax.set_ylim(0, max(2.0, float(max(np.max(write_speedup), np.max(read_speedup)) * 1.15)))

plt.tight_layout()
plt.savefig('simulator/results/Figure2_Speedup.png', dpi=100, facecolor='white')
print("✓ Figure 2: Speedup")
plt.close()

# ============= FIGURE 3: BASELINES FOCUS (required set) =============
fig, ax = plt.subplots(figsize=(8, 4.2))

upmem = read_upmem_swapin_values()

solutions = ['zram', 'InfiniSwap', 'NVMe SSD', 'UPMEM\nBaseline', 'UPMEM\nCPU', 'UPMEM\nDPU']

latencies_mid = [35, 35, 160, upmem['baseline'], upmem['cpu'], upmem['dpu']]
latencies_err_lower = [15, 5, 0, 0, 0, 0]
latencies_err_upper = [15, 5, 0, 0, 0, 0]

colors = ['#b3b3b3', '#9a9a9a', '#7f7f7f', '#666666', '#4d4d4d', '#333333']

x_pos = np.arange(len(solutions))

ax.bar(x_pos, latencies_mid, 
       yerr=[latencies_err_lower, latencies_err_upper],
       capsize=6, error_kw={'linewidth': 1.5, 'ecolor': 'black'},
       color=colors, edgecolor='black', linewidth=1, alpha=0.85)

ax.set_ylabel('Swap-in latency (µs)', fontsize=11, fontweight='bold')
ax.set_title('4KB Swap-in latency: required baselines + UPMEM', fontsize=12, fontweight='bold')
ax.set_xticks(x_pos)
ax.set_xticklabels(solutions, fontsize=10)
ax.grid(axis='y', alpha=0.2, linestyle='--')
ax.set_ylim(0, 190)
for i, val in enumerate(latencies_mid):
    ax.text(i, val + 2.0, f'{val:.1f}', ha='center', va='bottom', fontsize=8)

plt.tight_layout()
plt.savefig('simulator/results/Figure3_Baselines.png', dpi=150, bbox_inches='tight', facecolor='white')
print("✓ Figure 3: Baselines")
plt.close()

print("\n✅ Figures finales prêtes pour article!")
