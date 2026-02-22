#!/usr/bin/env python3
"""
Figures FINALES avec annotations pertinentes:
- Figure 2: Speedup values ON bars
- Figure 3: Names sans crochets
"""
import numpy as np
import matplotlib.pyplot as plt
import os

os.makedirs('simulator/results', exist_ok=True)

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
                         edgecolor='#339af0', facecolor='#d0ebff', linewidth=2)
ax.add_patch(app_box)
ax.text(5, 8.4, 'Application (Userspace)', fontsize=11, fontweight='bold', ha='center')

sdk_y = 5.2
sdk_box = FancyBboxPatch((0.5, sdk_y), 9, 2.2, boxstyle="round,pad=0.1",
                         edgecolor='#ff8787', facecolor='#ffe3e3', linewidth=2)
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
                         edgecolor='#fd7e14', facecolor='#fff3bf', linewidth=1.5)
    ax.add_patch(comp_box)
    ax.text(x, y, label, fontsize=8, ha='center', va='center', fontweight='bold')

arrow1 = FancyArrowPatch((5, 7.8), (5, 7.4),
                        arrowstyle='->', mutation_scale=25, linewidth=2.5, color='#339af0')
ax.add_patch(arrow1)

dpu_y = 2.5
ax.text(5, dpu_y + 1.8, '64 DPUs en Parallèle (Round-Robin Allocation)', 
        fontsize=10, fontweight='bold', ha='center')

for i in range(8):
    x_pos = 0.8 + i * 1.1
    dpu_small = Rectangle((x_pos, dpu_y), 0.9, 0.8,
                          edgecolor='#51cf66', facecolor='#e6fcf0', linewidth=1.5)
    ax.add_patch(dpu_small)
    ax.text(x_pos + 0.45, dpu_y + 0.4, f'DPU{i}', fontsize=7, ha='center', va='center', fontweight='bold')

ax.text(8.3, dpu_y + 0.4, '...', fontsize=10, fontweight='bold', ha='center', va='center')
ax.text(9.0, dpu_y + 0.4, 'DPU63', fontsize=7, ha='center', va='center', fontweight='bold')

arrow2 = FancyArrowPatch((5, sdk_y), (5, dpu_y + 0.8),
                        arrowstyle='->', mutation_scale=25, linewidth=2.5, color='#ff8787')
ax.add_patch(arrow2)

ax.text(5, 1.2, 'Mapping: page_id → (dpu_id, dpu_offset) | Allocation: Round-robin',
        fontsize=9, ha='center', fontweight='bold')

plt.tight_layout()
plt.savefig('simulator/results/Figure1_Architecture.png', dpi=150, bbox_inches='tight', facecolor='white')
print("✓ Figure 1: Architecture")
plt.close()

# ============= FIGURE 2: SPEEDUP avec VALUES =============
fig, ax = plt.subplots(figsize=(9, 5))

dpus = np.array([1, 8, 16, 32, 64])
write_speedup = np.array([0.97, 6.04, 10.44, 14.87, 15.62])
read_speedup = np.array([0.80, 6.05, 14.30, 14.67, 27.10])

x_pos = np.arange(len(dpus))
width = 0.35

bars_write = ax.bar(x_pos - width/2, write_speedup, width, 
                    label='WRITE', color='#4dabf7', edgecolor='black', linewidth=1)
bars_read = ax.bar(x_pos + width/2, read_speedup, width,
                   label='READ', color='#51cf66', edgecolor='black', linewidth=1)

# Add values ON bars
for i, (w, r) in enumerate(zip(write_speedup, read_speedup)):
    if w > 1.5:  # Only show if > 1.5 (skip the ~1 value)
        ax.text(i - width/2, w + 0.3, f'{w:.1f}x', ha='center', fontsize=9, fontweight='bold')
    if r > 1.5:
        ax.text(i + width/2, r + 0.3, f'{r:.1f}x', ha='center', fontsize=9, fontweight='bold')

ax.set_xlabel('Nombre de DPUs', fontsize=11, fontweight='bold')
ax.set_ylabel('Speedup (facteur)', fontsize=11, fontweight='bold')
ax.set_title('Speedup Mode Parallèle vs Séquentiel', fontsize=12, fontweight='bold')
ax.set_xticks(x_pos)
ax.set_xticklabels(dpus)
ax.legend(fontsize=10, loc='upper left')
ax.grid(axis='y', alpha=0.2, linestyle='--')
ax.set_ylim(0, 30)

plt.tight_layout()
plt.savefig('simulator/results/Figure2_Speedup.png', dpi=150, bbox_inches='tight', facecolor='white')
print("✓ Figure 2: Speedup (avec VALUES)")
plt.close()

# ============= FIGURE 3: BASELINES sans crochets =============
fig, ax = plt.subplots(figsize=(10, 5))

# Names WITHOUT line breaks
solutions = ['RDMA', 'Optane', 'NVMe fast', 'UPMEM 1 DPU', 'NVMe typical', 'SATA SSD', 'Linux swap']

latencies_mid = [10, 11.5, 35, 31, 80, 157.5, 250]
latencies_err_lower = [5, 10, 20, 0, 60, 140, 100]
latencies_err_upper = [5, 2.5, 15, 0, 20, 17.5, 150]

colors = ['#ff6b6b', '#ff8c42', '#ffa94d', '#51cf66', '#a0a0a0', '#808080', '#606060']

x_pos = np.arange(len(solutions))

ax.bar(x_pos, latencies_mid, 
       yerr=[latencies_err_lower, latencies_err_upper],
       capsize=6, error_kw={'linewidth': 1.5, 'ecolor': 'black'},
       color=colors, edgecolor='black', linewidth=1, alpha=0.85)

ax.set_ylabel('Latence (µs)', fontsize=11, fontweight='bold')
ax.set_title('Latence Swap 4KB: UPMEM vs Alternatives', fontsize=12, fontweight='bold')
ax.set_xticks(x_pos)
ax.set_xticklabels(solutions, fontsize=10, rotation=45, ha='right')
ax.grid(axis='y', alpha=0.2, linestyle='--')
ax.set_ylim(0, 400)

plt.tight_layout()
plt.savefig('simulator/results/Figure3_Baselines.png', dpi=150, bbox_inches='tight', facecolor='white')
print("✓ Figure 3: Baselines (sans crochets)")
plt.close()

print("\n✅ Figures finales prêtes pour article!")
