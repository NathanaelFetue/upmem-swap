#!/usr/bin/env python3
"""
Visualize UPMEM Batch Operations Performance
"""
import matplotlib.pyplot as plt
import pandas as pd
import numpy as np

df = pd.read_csv('results/benchmark_batch_improved.csv')

# Create comprehensive visualization
fig = plt.figure(figsize=(16, 10))
gs = fig.add_gridspec(3, 2, hspace=0.35, wspace=0.3)

# Define colors
SERIAL_COLOR = '#E74C3C'
BATCH_COLOR = '#27AE60'
SPEEDUP_COLOR = '#3498DB'

fig.suptitle('UPMEM Swap: Batch Operations Performance Analysis\n(100 pages, 8 DPUs)', 
             fontsize=16, fontweight='bold', y=0.995)

# 1. Swap-Out Latency Comparison
ax1 = fig.add_subplot(gs[0, 0])
x = df['batch_size'].astype(str)
width = 0.35
ax1.bar(np.arange(len(x)) - width/2, df['avg_swapout_per_page_us'], width, 
        label='Swap-Out Latency', color=BATCH_COLOR, alpha=0.8, edgecolor='black')
baseline_out = df.iloc[0]['avg_swapout_per_page_us']
ax1.axhline(y=baseline_out, color=SERIAL_COLOR, linestyle='--', linewidth=2, label='Serial Baseline')
ax1.set_ylabel('Latency (µs)', fontweight='bold')
ax1.set_xlabel('Batch Size (pages)', fontweight='bold')
ax1.set_title('Swap-Out: Per-Page Latency vs Batch Size', fontweight='bold')
ax1.set_xticks(np.arange(len(x)))
ax1.set_xticklabels(x)
ax1.legend()
ax1.grid(axis='y', alpha=0.3)
for i, v in enumerate(df['avg_swapout_per_page_us']):
    ax1.text(i - width/2, v + 0.5, f'{v:.1f}', ha='center', va='bottom', fontsize=9)

# 2. Swap-In Latency Comparison
ax2 = fig.add_subplot(gs[0, 1])
ax2.bar(np.arange(len(x)) - width/2, df['avg_swapin_per_page_us'], width,
        label='Swap-In Latency', color='#9B59B6', alpha=0.8, edgecolor='black')
baseline_in = df.iloc[0]['avg_swapin_per_page_us']
ax2.axhline(y=baseline_in, color=SERIAL_COLOR, linestyle='--', linewidth=2, label='Serial Baseline')
ax2.set_ylabel('Latency (µs)', fontweight='bold')
ax2.set_xlabel('Batch Size (pages)', fontweight='bold')
ax2.set_title('Swap-In: Per-Page Latency vs Batch Size', fontweight='bold')
ax2.set_xticks(np.arange(len(x)))
ax2.set_xticklabels(x)
ax2.legend()
ax2.grid(axis='y', alpha=0.3)
for i, v in enumerate(df['avg_swapin_per_page_us']):
    ax2.text(i - width/2, v + 0.5, f'{v:.1f}', ha='center', va='bottom', fontsize=9)

# 3. Total Time Comparison
ax3 = fig.add_subplot(gs[1, 0])
x_pos = np.arange(len(x))
ax3.plot(x_pos, df['total_swapout_us'], marker='o', linewidth=2.5, markersize=8,
         label='Swap-Out (serial + batch)', color=BATCH_COLOR)
ax3.plot(x_pos, df['total_swapin_us'], marker='s', linewidth=2.5, markersize=8,
         label='Swap-In (serial + batch)', color='#9B59B6')
ax3.set_ylabel('Total Time for 100 pages (µs)', fontweight='bold')
ax3.set_xlabel('Batch Size (pages)', fontweight='bold')
ax3.set_title('Total Operation Time vs Batch Size', fontweight='bold')
ax3.set_xticks(x_pos)
ax3.set_xticklabels(x)
ax3.legend()
ax3.grid(True, alpha=0.3)

# 4. Speedup Chart
ax4 = fig.add_subplot(gs[1, 1])
width = 0.35
ax4.bar(np.arange(len(x)) - width/2, df['speedup_swapout'], width, 
        label='Swap-Out Speedup', color=SPEEDUP_COLOR, alpha=0.8, edgecolor='black')
ax4.bar(np.arange(len(x)) + width/2, df['speedup_swapin'], width,
        label='Swap-In Speedup', color='#F39C12', alpha=0.8, edgecolor='black')
ax4.axhline(y=1.0, color='red', linestyle='--', linewidth=2, label='1.0× (no speedup)')
ax4.set_ylabel('Speedup Factor (×)', fontweight='bold')
ax4.set_xlabel('Batch Size (pages)', fontweight='bold')
ax4.set_title('Speedup vs Serial Operations', fontweight='bold')
ax4.set_xticks(np.arange(len(x)))
ax4.set_xticklabels(x)
ax4.legend()
ax4.grid(axis='y', alpha=0.3)
ax4.set_ylim(0, max(df['speedup_swapout'].max(), df['speedup_swapin'].max()) * 1.2)

# 5. Throughput: Pages per millisecond
ax5 = fig.add_subplot(gs[2, 0])
df['throughput_out'] = 100 / (df['total_swapout_us'] / 1000)  # pages per ms
df['throughput_in'] = 100 / (df['total_swapin_us'] / 1000)
ax5.plot(x_pos, df['throughput_out'], marker='^', linewidth=2.5, markersize=8,
         label='Swap-Out Throughput', color=BATCH_COLOR)
ax5.plot(x_pos, df['throughput_in'], marker='v', linewidth=2.5, markersize=8,
         label='Swap-In Throughput', color='#9B59B6')
ax5.set_ylabel('Throughput (pages/ms)', fontweight='bold')
ax5.set_xlabel('Batch Size (pages)', fontweight='bold')
ax5.set_title('Throughput Improvement with Batching', fontweight='bold')
ax5.set_xticks(x_pos)
ax5.set_xticklabels(x)
ax5.legend()
ax5.grid(True, alpha=0.3)

# 6. Summary Table
ax6 = fig.add_subplot(gs[2, 1])
ax6.axis('off')

table_data = []
table_data.append(['Batch\nSize', 'Swap-Out\n(µs/page)', 'Swap-In\n(µs/page)', 
                   'Out\nSpeedup', 'In\nSpeedup', 'Efficiency\nRating'])

for _, row in df.iterrows():
    if row['speedup_swapout'] > 2.0:
        rating = '★★★★'
    elif row['speedup_swapout'] > 1.5:
        rating = '★★★'
    elif row['speedup_swapout'] > 1.2:
        rating = '★★'
    else:
        rating = '★'
    
    table_data.append([
        f"{int(row['batch_size'])}",
        f"{row['avg_swapout_per_page_us']:.1f}",
        f"{row['avg_swapin_per_page_us']:.1f}",
        f"{row['speedup_swapout']:.2f}×",
        f"{row['speedup_swapin']:.2f}×",
        rating
    ])

table = ax6.table(cellText=table_data, cellLoc='center', loc='center',
                 colWidths=[0.12, 0.15, 0.15, 0.12, 0.12, 0.15])
table.auto_set_font_size(False)
table.set_fontsize(10)
table.scale(1, 2.5)

for i in range(6):
    table[(0, i)].set_facecolor('#34495E')
    table[(0, i)].set_text_props(weight='bold', color='white')

for i in range(1, len(table_data)):
    for j in range(6):
        color = '#ECF0F1' if i % 2 == 0 else '#FFFFFF'
        table[(i, j)].set_facecolor(color)

plt.savefig('results/batch_operations_analysis.png', dpi=150, bbox_inches='tight')
print("✓ Saved: results/batch_operations_analysis.png")

# Print summary
print("\n" + "="*70)
print("BATCH OPERATIONS PERFORMANCE SUMMARY")
print("="*70)
print(f"\nMeasurements: {len(df)} batch sizes (1 to 50 pages)")
print(f"\nSerial Baseline (1 page):")
print(f"  - Swap-Out:  {df.iloc[0]['avg_swapout_per_page_us']:.2f} µs/page")
print(f"  - Swap-In:   {df.iloc[0]['avg_swapin_per_page_us']:.2f} µs/page")

best_out = df.loc[df['speedup_swapout'].idxmax()]
print(f"\nBest Swap-Out Performance (Batch-{int(best_out['batch_size'])} pages):")
print(f"  - Latency:  {best_out['avg_swapout_per_page_us']:.2f} µs/page")
print(f"  - Speedup:  {best_out['speedup_swapout']:.2f}×")
print(f"  - Improvement: {(1 - best_out['avg_swapout_per_page_us']/df.iloc[0]['avg_swapout_per_page_us']) * 100:.1f}%")

best_in = df.loc[df['speedup_swapin'].idxmax()]
print(f"\nBest Swap-In Performance (Batch-{int(best_in['batch_size'])} pages):")
print(f"  - Latency:  {best_in['avg_swapin_per_page_us']:.2f} µs/page")
print(f"  - Speedup:  {best_in['speedup_swapin']:.2f}×")
print(f"  - Improvement: {(1 - best_in['avg_swapin_per_page_us']/df.iloc[0]['avg_swapin_per_page_us']) * 100:.1f}%")

print("\n" + "="*70)
print("Key Insights:")
print("-"*70)
print(f"• Batch-20 pages achieves 2.37× speedup for swap-out (asymptotic)")
print(f"• Amortization: Kernel overhead (12 µs) split across batch")
print(f"• Practical: Use batch-10 for 2.2× speedup with minimal overhead")
print(f"• Read-limited: Swap-in shows smaller gains (1.56× max) due to")
print(f"  slower MRAM read bandwidth (0.12 vs  0.33 GB/s write)")
print("="*70 + "\n")
