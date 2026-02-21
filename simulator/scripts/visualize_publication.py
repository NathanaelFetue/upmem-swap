#!/usr/bin/env python3
"""
Comprehensive UPMEM Batch Performance Analysis
2 visualizations per image (3 images total)
"""
import matplotlib.pyplot as plt
import pandas as pd
import numpy as np

df = pd.read_csv('results/benchmark_batch_improved.csv')

# Convert throughput to pages/second
df['throughput_out_pages_per_sec'] = (100 / (df['total_swapout_us'] / 1e6))
df['throughput_in_pages_per_sec'] = (100 / (df['total_swapin_us'] / 1e6))

print("\n" + "="*70)
print("THROUGHPUT IN PAGES/SECOND")
print("="*70)
print(f"{'Batch Size':<12} {'Swap-Out (pages/s)':<22} {'Swap-In (pages/s)':<22}")
print("-"*70)
for _, row in df.iterrows():
    print(f"{int(row['batch_size']):<12} {row['throughput_out_pages_per_sec']:>20.0f} {row['throughput_in_pages_per_sec']:>20.0f}")
print("="*70 + "\n")

# Define colors
SERIAL_COLOR = '#E74C3C'
BATCH_COLOR = '#27AE60'
SWAP_IN_COLOR = '#9B59B6'
SPEEDUP_COLOR = '#3498DB'

# ============================================================
# IMAGE 1: Latency Comparison (2 subplots)
# ============================================================
fig1, axes1 = plt.subplots(1, 2, figsize=(14, 5))
fig1.suptitle('UPMEM Swap: Per-Page Latency Analysis\n(100 pages, 8 DPUs)', 
              fontsize=13, fontweight='bold')

# Subplot 1A: Swap-Out Latency
ax1a = axes1[0]
x_pos = np.arange(len(df))
bars_out = ax1a.bar(x_pos, df['avg_swapout_per_page_us'], 
                     color=BATCH_COLOR, alpha=0.85, edgecolor='black', linewidth=1.5)
baseline_out = df.iloc[0]['avg_swapout_per_page_us']
ax1a.axhline(y=baseline_out, color=SERIAL_COLOR, linestyle='--', linewidth=2, 
             label=f'Serial Baseline ({baseline_out:.1f} µs)')
ax1a.set_ylabel('Latency (µs)', fontweight='bold', fontsize=11)
ax1a.set_xlabel('Batch Size (pages)', fontweight='bold', fontsize=11)
ax1a.set_title('Swap-Out: Per-Page Latency Reduction', fontweight='bold')
ax1a.set_xticks(x_pos)
ax1a.set_xticklabels(df['batch_size'].astype(int))
ax1a.legend(fontsize=10)
ax1a.grid(axis='y', alpha=0.3, linestyle='--')
ax1a.set_ylim(0, baseline_out * 1.2)

# Add value labels
for i, (bar, val) in enumerate(zip(bars_out, df['avg_swapout_per_page_us'])):
    height = bar.get_height()
    ax1a.text(bar.get_x() + bar.get_width()/2., height + 0.5,
             f'{val:.1f}µs', ha='center', va='bottom', fontsize=9, fontweight='bold')

# Subplot 1B: Swap-In Latency
ax1b = axes1[1]
bars_in = ax1b.bar(x_pos, df['avg_swapin_per_page_us'],
                    color=SWAP_IN_COLOR, alpha=0.85, edgecolor='black', linewidth=1.5)
baseline_in = df.iloc[0]['avg_swapin_per_page_us']
ax1b.axhline(y=baseline_in, color=SERIAL_COLOR, linestyle='--', linewidth=2,
             label=f'Serial Baseline ({baseline_in:.1f} µs)')
ax1b.set_ylabel('Latency (µs)', fontweight='bold', fontsize=11)
ax1b.set_xlabel('Batch Size (pages)', fontweight='bold', fontsize=11)
ax1b.set_title('Swap-In: Per-Page Latency Reduction', fontweight='bold')
ax1b.set_xticks(x_pos)
ax1b.set_xticklabels(df['batch_size'].astype(int))
ax1b.legend(fontsize=10)
ax1b.grid(axis='y', alpha=0.3, linestyle='--')
ax1b.set_ylim(0, baseline_in * 1.2)

# Add value labels
for i, (bar, val) in enumerate(zip(bars_in, df['avg_swapin_per_page_us'])):
    height = bar.get_height()
    ax1b.text(bar.get_x() + bar.get_width()/2., height + 0.5,
             f'{val:.1f}µs', ha='center', va='bottom', fontsize=9, fontweight='bold')

plt.tight_layout()
plt.savefig('results/01_latency_analysis.png', dpi=150, bbox_inches='tight')
print("✓ Saved: results/01_latency_analysis.png")

# ============================================================
# IMAGE 2: Speedup & Throughput (2 subplots)
# ============================================================
fig2, axes2 = plt.subplots(1, 2, figsize=(14, 5))
fig2.suptitle('UPMEM Swap: Speedup and Throughput Performance\n(100 pages, 8 DPUs)',
              fontsize=13, fontweight='bold')

# Subplot 2A: Speedup Comparison
ax2a = axes2[0]
width = 0.35
x_pos_speedup = np.arange(len(df))
bars1 = ax2a.bar(x_pos_speedup - width/2, df['speedup_swapout'], width,
                 label='Swap-Out Speedup', color=SPEEDUP_COLOR, alpha=0.8, edgecolor='black', linewidth=1)
bars2 = ax2a.bar(x_pos_speedup + width/2, df['speedup_swapin'], width,
                 label='Swap-In Speedup', color='#F39C12', alpha=0.8, edgecolor='black', linewidth=1)
ax2a.axhline(y=1.0, color=SERIAL_COLOR, linestyle='--', linewidth=2, label='1.0× (no speedup)')
ax2a.set_ylabel('Speedup Factor (×)', fontweight='bold', fontsize=11)
ax2a.set_xlabel('Batch Size (pages)', fontweight='bold', fontsize=11)
ax2a.set_title('Speedup vs Serial Operations', fontweight='bold')
ax2a.set_xticks(x_pos_speedup)
ax2a.set_xticklabels(df['batch_size'].astype(int))
ax2a.legend(fontsize=10, loc='upper right')
ax2a.grid(axis='y', alpha=0.3, linestyle='--')
ax2a.set_ylim(0, max(df['speedup_swapout'].max(), df['speedup_swapin'].max()) * 1.15)

# Add value labels on bars
for bars in [bars1, bars2]:
    for bar in bars:
        height = bar.get_height()
        ax2a.text(bar.get_x() + bar.get_width()/2., height + 0.05,
                 f'{height:.2f}×', ha='center', va='bottom', fontsize=8)

# Subplot 2B: Throughput (pages/second)
ax2b = axes2[1]
x_pos_tp = np.arange(len(df))
line1 = ax2b.plot(x_pos_tp, df['throughput_out_pages_per_sec'], 
                  marker='o', linewidth=2.5, markersize=8, color=BATCH_COLOR,
                  label='Swap-Out Throughput')
line2 = ax2b.plot(x_pos_tp, df['throughput_in_pages_per_sec'],
                  marker='s', linewidth=2.5, markersize=8, color=SWAP_IN_COLOR,
                  label='Swap-In Throughput')
ax2b.set_ylabel('Throughput (pages/second)', fontweight='bold', fontsize=11)
ax2b.set_xlabel('Batch Size (pages)', fontweight='bold', fontsize=11)
ax2b.set_title('Throughput: Pages Processed per Second', fontweight='bold')
ax2b.set_xticks(x_pos_tp)
ax2b.set_xticklabels(df['batch_size'].astype(int))
ax2b.legend(fontsize=10, loc='upper right')
ax2b.grid(True, alpha=0.3, linestyle='--')

# Format y-axis with thousands separator
ax2b.yaxis.set_major_formatter(plt.FuncFormatter(lambda x, p: f'{int(x):,}'))

# Add value labels
for i, (x, y_out, y_in) in enumerate(zip(x_pos_tp, df['throughput_out_pages_per_sec'], 
                                          df['throughput_in_pages_per_sec'])):
    ax2b.annotate(f'{int(y_out):,}', xy=(x, y_out), xytext=(0, 5),
                 textcoords='offset points', ha='center', fontsize=8)
    ax2b.annotate(f'{int(y_in):,}', xy=(x, y_in), xytext=(0, -15),
                 textcoords='offset points', ha='center', fontsize=8, color=SWAP_IN_COLOR)

plt.tight_layout()
plt.savefig('results/02_speedup_throughput.png', dpi=150, bbox_inches='tight')
print("✓ Saved: results/02_speedup_throughput.png")

# ============================================================
# IMAGE 3: Performance Table + Recommendations (2 subplots)
# ============================================================
fig3 = plt.figure(figsize=(14, 6))
gs = fig3.add_gridspec(1, 2, width_ratios=[1, 1.2])

fig3.suptitle('UPMEM Swap: Performance Summary & Scaling Analysis\n(100 pages, 8 DPUs)',
              fontsize=13, fontweight='bold')

# Subplot 3A: Detailed Performance Table
ax3a = fig3.add_subplot(gs[0, 0])
ax3a.axis('off')

table_data = []
table_data.append(['Batch\nSize', 'Out\n(µs/pg)', 'In\n(µs/pg)', 
                   'Speed\nOut', 'Speed\nIn', 'Out\n(pp/s)', 'Rating'])

best_out_idx = df['speedup_swapout'].idxmax()
best_in_idx = df['speedup_swapin'].idxmax()

for idx, (_, row) in enumerate(df.iterrows()):
    out_rating = '★★★★★' if row['speedup_swapout'] > 2.3 else '★★★★' if row['speedup_swapout'] > 2.0 else '★★★' if row['speedup_swapout'] > 1.5 else '★★' if row['speedup_swapout'] > 1.2 else '★'
    
    batch_size = int(row['batch_size'])
    mark = ' ⭐ BEST' if idx == best_out_idx else ''
    
    table_data.append([
        f"{batch_size}{mark}",
        f"{row['avg_swapout_per_page_us']:.1f}",
        f"{row['avg_swapin_per_page_us']:.1f}",
        f"{row['speedup_swapout']:.2f}×",
        f"{row['speedup_swapin']:.2f}×",
        f"{int(row['throughput_out_pages_per_sec']):,}",
        out_rating
    ])

table = ax3a.table(cellText=table_data, cellLoc='center', loc='center',
                  colWidths=[0.12, 0.12, 0.12, 0.12, 0.12, 0.16, 0.14])
table.auto_set_font_size(False)
table.set_fontsize(9)
table.scale(1, 2.4)

# Style header
for i in range(7):
    table[(0, i)].set_facecolor('#34495E')
    table[(0, i)].set_text_props(weight='bold', color='white', fontsize=9)

# Color rows
for i in range(1, len(table_data)):
    for j in range(7):
        if '⭐' in table_data[i][0]:
            table[(i, j)].set_facecolor('#D5F4E6')
        elif i % 2 == 0:
            table[(i, j)].set_facecolor('#ECF0F1')
        else:
            table[(i, j)].set_facecolor('#FFFFFF')

ax3a.text(0.5, -0.15, 'pp/s = pages per second', 
         ha='center', fontsize=9, style='italic', transform=ax3a.transAxes)

# Subplot 3B: Scaling Analysis
ax3b = fig3.add_subplot(gs[0, 1])

recommendations = [
    "📊 KEY FINDINGS:",
    "",
    "✓ Batch-10 is OPTIMAL ratio:",
    "  • 2.22× swap-out speedup",
    "  • 1.50× swap-in speedup",
    "  • Practical & minimal overhead",
    "",
    "✓ Throughput (pages/second):",
    f"  • Serial:    {int(df.iloc[0]['throughput_out_pages_per_sec']):,} pp/s",
    f"  • Batch-10:  {int(df.loc[df['batch_size']==10, 'throughput_out_pages_per_sec'].values[0]):,} pp/s",
    f"  • Batch-50:  {int(df.iloc[-1]['throughput_out_pages_per_sec']):,} pp/s",
    "",
    "✓ Read Bandwidth Limited:",
    "  • Swap-in maxes at 1.56× (50 pages)",
    "  • Limited by 0.12 GB/s read BW",
    "  • Write BW is 0.33 GB/s (higher)",
    "",
    "✓ Asymptotic Limits:",
    "  • Swap-out: 12.12 µs/page (batch-50)",
    "  • Swap-in:  31.99 µs/page (batch-50)",
]

ax3b.axis('off')
y_pos = 0.95
for line in recommendations:
    if line.startswith("✓") or line.startswith("📊"):
        ax3b.text(0.05, y_pos, line, fontsize=10, fontweight='bold',
                 transform=ax3b.transAxes, color='#27AE60')
    elif line == "":
        y_pos -= 0.02
        continue
    else:
        ax3b.text(0.05, y_pos, line, fontsize=9, family='monospace',
                 transform=ax3b.transAxes)
    y_pos -= 0.045

plt.tight_layout()
plt.savefig('results/03_performance_summary.png', dpi=150, bbox_inches='tight')
print("✓ Saved: results/03_performance_summary.png")

print("\n" + "="*70)
print("PERFORMANCE SUMMARY")
print("="*70)
print(f"\nBest Batch Size: 10 pages")
print(f"  Swap-Out: {df.loc[df['batch_size']==10, 'speedup_swapout'].values[0]:.2f}× speedup → {int(df.loc[df['batch_size']==10, 'throughput_out_pages_per_sec'].values[0]):,} pages/second")
print(f"  Swap-In:  {df.loc[df['batch_size']==10, 'speedup_swapin'].values[0]:.2f}× speedup → {int(df.loc[df['batch_size']==10, 'throughput_in_pages_per_sec'].values[0]):,} pages/second")
print(f"\nAsymptotic Limit (Batch-50):")
print(f"  Swap-Out: {df.loc[df['batch_size']==50, 'speedup_swapout'].values[0]:.2f}× speedup")
print(f"  Swap-In:  {df.loc[df['batch_size']==50, 'speedup_swapin'].values[0]:.2f}× speedup (limited by read BW)")
print("="*70 + "\n")
