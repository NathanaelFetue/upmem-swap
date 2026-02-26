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

dpus = np.array([1, 8, 16, 32, 64])
write_speedup = np.array([0.97, 6.04, 10.44, 14.87, 15.62])
read_speedup = np.array([0.80, 6.05, 14.30, 14.67, 27.10])

# tighter grouping so bars remain legible in article scale
x_pos = np.arange(len(dpus)) * 0.82
width = 0.40

bars_write = ax.bar(x_pos - width/2, write_speedup, width,
                    label='WRITE', color='#4d4d4d', edgecolor='black', linewidth=1)
bars_read = ax.bar(x_pos + width/2, read_speedup, width,
                   label='READ', color='#a6a6a6', edgecolor='black', linewidth=1)

# Add values anchored to actual bar geometry (prevents shifted labels)
for bar in bars_write:
    h = bar.get_height()
    if h > 1.5:
        ax.text(bar.get_x() + bar.get_width() / 2.0, h + 0.35, f'{h:.1f}x',
                ha='center', va='bottom', fontsize=8, fontweight='bold')
for bar in bars_read:
    h = bar.get_height()
    if h > 1.5:
        ax.text(bar.get_x() + bar.get_width() / 2.0, h + 0.35, f'{h:.1f}x',
                ha='center', va='bottom', fontsize=8, fontweight='bold')

ax.set_xlabel('Nombre de DPUs', fontsize=9, fontweight='bold')
ax.set_ylabel('Speedup (facteur)', fontsize=9, fontweight='bold')
ax.set_title('Speedup Parallèle vs Séquentiel', fontsize=10, fontweight='bold')
ax.set_xticks(x_pos)
ax.set_xticklabels(dpus)
ax.legend(fontsize=8, loc='upper left')
ax.grid(axis='y', alpha=0.2, linestyle='--')
ax.set_ylim(0, 31)

plt.tight_layout()
plt.savefig('simulator/results/Figure2_Speedup.png', dpi=100, facecolor='white')
print("✓ Figure 2: Speedup (avec VALUES)")
plt.close()

# ============= FIGURE 3: BASELINES FOCUS (required set) =============
fig, ax = plt.subplots(figsize=(8, 4.2))

solutions = ['zram', 'InfiniSwap', 'NVMe SSD', 'UPMEM\nBaseline', 'UPMEM\nCPU', 'UPMEM\nDPU']

latencies_mid = [35, 35, 160, 29.59, 14.71, 31.28]
latencies_err_lower = [15, 5, 0, 0, 0, 0]
latencies_err_upper = [15, 5, 0, 0, 0, 0]

colors = ['#b3b3b3', '#9a9a9a', '#7f7f7f', '#666666', '#4d4d4d', '#333333']

x_pos = np.arange(len(solutions))

ax.bar(x_pos, latencies_mid, 
       yerr=[latencies_err_lower, latencies_err_upper],
       capsize=6, error_kw={'linewidth': 1.5, 'ecolor': 'black'},
       color=colors, edgecolor='black', linewidth=1, alpha=0.85)

ax.set_ylabel('Latence (µs)', fontsize=11, fontweight='bold')
ax.set_title('Latence Swap 4KB: baselines requises + UPMEM', fontsize=12, fontweight='bold')
ax.set_xticks(x_pos)
ax.set_xticklabels(solutions, fontsize=10)
ax.grid(axis='y', alpha=0.2, linestyle='--')
ax.set_ylim(0, 190)

plt.tight_layout()
plt.savefig('simulator/results/Figure3_Baselines.png', dpi=150, bbox_inches='tight', facecolor='white')
print("✓ Figure 3: Baselines (sans crochets)")
plt.close()

print("\n✅ Figures finales prêtes pour article!")
