#!/usr/bin/env python3
"""
Compare UPMEM swap latencies (serial & batch) with other methods
Saves results/04_compare_methods.png
"""
import matplotlib.pyplot as plt

# Data: latency in microseconds (µs) for comparable operation (swap-like)
methods = [
    'eBPF (notif)',
    'UPMEM (batch-10)',
    'UPMEM (serial)',
    'NVMe SSD',
    'Infiniswap',
    'SSD SATA',
    'userfaultfd',
    'HDD 7200'
]
latencies_us = [2.4, 13.31, 29.59, 30.0, 40.0, 85.0, 107.0, 10610.0]

colors = [
    '#2E86AB',  # eBPF
    '#27AE60',  # UPMEM batch
    '#1ABC9C',  # UPMEM serial
    '#9B59B6',  # NVMe
    '#F39C12',  # Infiniswap
    '#E74C3C',  # SATA
    '#95A5A6',  # userfaultfd
    '#34495E'   # HDD
]

fig, ax = plt.subplots(figsize=(10,6))

# Use log scale for y to show HDD but keep linear look via minor ticks
x = range(len(methods))
bars = ax.bar(x, latencies_us, color=colors, edgecolor='black', linewidth=0.8)

ax.set_yscale('log')
ax.set_xticks(x)
ax.set_xticklabels(methods, rotation=25, ha='right')
ax.set_ylabel('Latency (µs) — échelle logarithmique', fontsize=12, fontweight='bold')
ax.set_title('Comparaison: UPMEM Swap vs autres méthodes (latence par opération)', fontsize=14, fontweight='bold')
ax.grid(True, which='both', axis='y', linestyle='--', alpha=0.4)

# Annotate bars with values and short notes
for rect, val, method in zip(bars, latencies_us, methods):
    height = val
    if val < 100:
        ax.text(rect.get_x() + rect.get_width()/2., height*1.15, f'{val:.1f}µs', ha='center', va='bottom', fontsize=9)
    else:
        ax.text(rect.get_x() + rect.get_width()/2., height*1.05, f'{int(val):,}µs', ha='center', va='bottom', fontsize=9)

# Explanatory note
note = (
    "Notes: eBPF = notification-only (low overhead); UPMEM batch = per-page effective latency "
    "for batch-10; userfaultfd = userfaultfd-based swap to storage. Log scale used to include HDD."
)
fig.text(0.02, 0.02, note, fontsize=9)

plt.tight_layout()
plt.savefig('results/04_compare_methods.png', dpi=150, bbox_inches='tight')
print('✓ Saved: results/04_compare_methods.png')
