#!/usr/bin/env python3
"""
Visualize batch swap operations performance
"""

import matplotlib.pyplot as plt
import pandas as pd
import numpy as np

# Read batch benchmark results
df = pd.read_csv('results/benchmark_batch.csv')

# Create figure with subplots
fig, axes = plt.subplots(1, 3, figsize=(16, 5))
fig.suptitle('UPMEM Swap: Batch Operations Performance Analysis', fontsize=14, fontweight='bold')

# Plot 1: Latency per Page vs Batch Size
ax1 = axes[0]
ax1.plot(df['batch_size'], df['avg_per_page_us'], marker='o', linewidth=2, markersize=8, color='#2E86AB')
ax1.fill_between(df['batch_size'], df['avg_per_page_us'], alpha=0.3, color='#2E86AB')
ax1.set_xlabel('Batch Size (pages)', fontsize=11, fontweight='bold')
ax1.set_ylabel('Avg Latency per Page (µs)', fontsize=11, fontweight='bold')
ax1.set_title('Per-Page Latency vs Batch Size', fontsize=12, fontweight='bold')
ax1.grid(True, alpha=0.3, linestyle='--')
ax1.set_ylim(0, max(df['avg_per_page_us']) * 1.2)
for i, row in df.iterrows():
    ax1.annotate(f"{row['avg_per_page_us']:.2f}µs", 
                xy=(row['batch_size'], row['avg_per_page_us']),
                xytext=(0, 5), textcoords='offset points', ha='center', fontsize=9)

# Plot 2: Speedup vs Batch Size
ax2 = axes[1]
colors = ['#A23B72' if x > 1.5 else '#F18F01' for x in df['speedup']]
bars = ax2.bar(df['batch_size'].astype(str), df['speedup'], color=colors, edgecolor='black', linewidth=1.5, alpha=0.85)
ax2.axhline(y=1.0, color='red', linestyle='--', linewidth=2, label='Baseline (no speedup)')
ax2.set_xlabel('Batch Size (pages)', fontsize=11, fontweight='bold')
ax2.set_ylabel('Speedup Factor (×)', fontsize=11, fontweight='bold')
ax2.set_title('Throughput Speedup vs Batch Size', fontsize=12, fontweight='bold')
ax2.set_ylim(0, max(df['speedup']) * 1.15)
ax2.grid(True, alpha=0.3, axis='y', linestyle='--')
for i, (bar, val) in enumerate(zip(bars, df['speedup'])):
    height = bar.get_height()
    ax2.text(bar.get_x() + bar.get_width()/2., height,
            f'{val:.2f}×',
            ha='center', va='bottom', fontsize=10, fontweight='bold')
ax2.legend(fontsize=10)

# Plot 3: Throughput (pages/ms) vs Batch Size
ax3 = axes[2]
ax3.plot(df['batch_size'], df['throughput_pages_per_ms'], marker='s', linewidth=2, markersize=8, color='#06A77D')
ax3.fill_between(df['batch_size'], df['throughput_pages_per_ms'], alpha=0.3, color='#06A77D')
ax3.set_xlabel('Batch Size (pages)', fontsize=11, fontweight='bold')
ax3.set_ylabel('Throughput (pages/ms)', fontsize=11, fontweight='bold')
ax3.set_title('Transfer Throughput vs Batch Size', fontsize=12, fontweight='bold')
ax3.grid(True, alpha=0.3, linestyle='--')
ax3.set_ylim(0, max(df['throughput_pages_per_ms']) * 1.2)
for i, row in df.iterrows():
    ax3.annotate(f"{row['throughput_pages_per_ms']:.0f}", 
                xy=(row['batch_size'], row['throughput_pages_per_ms']),
                xytext=(0, 5), textcoords='offset points', ha='center', fontsize=9)

plt.tight_layout()
plt.savefig('results/batch_operations_analysis.png', dpi=150, bbox_inches='tight')
print("✓ Saved: results/batch_operations_analysis.png")

# Create detailed performance table as image
fig2, ax = plt.subplots(figsize=(10, 4))
ax.axis('tight')
ax.axis('off')

# Prepare table data
table_data = []
table_data.append(['Batch Size', 'Total Time\n(µs)', 'Per-Page\nLatency (µs)', 
                   'Throughput\n(pages/ms)', 'Speedup\nvs Serial', 'Efficiency'])

for _, row in df.iterrows():
    efficiency = "High" if row['speedup'] > 1.5 else "Medium" if row['speedup'] > 1.2 else "Baseline"
    table_data.append([
        f"{int(row['batch_size'])} pages",
        f"{row['total_time_us']:.1f}",
        f"{row['avg_per_page_us']:.2f}",
        f"{row['throughput_pages_per_ms']:.0f}",
        f"{row['speedup']:.2f}×",
        efficiency
    ])

table = ax.table(cellText=table_data, cellLoc='center', loc='center',
                colWidths=[0.15, 0.15, 0.15, 0.15, 0.15, 0.15])
table.auto_set_font_size(False)
table.set_fontsize(10)
table.scale(1, 2.2)

# Style header row
for i in range(6):
    table[(0, i)].set_facecolor('#2E86AB')
    table[(0, i)].set_text_props(weight='bold', color='white')

# Alternate row colors
for i in range(1, len(table_data)):
    color = '#F0F0F0' if i % 2 == 0 else '#FFFFFF'
    for j in range(6):
        table[(i, j)].set_facecolor(color)
        if 'High' in str(table_data[i][5]):
            table[(i, 5)].set_facecolor('#D4EDDA')
        elif 'Medium' in str(table_data[i][5]):
            table[(i, 5)].set_facecolor('#FFF3CD')

plt.title('Batch Operations Performance Summary\n(100 total pages, 8 DPUs)', 
         fontsize=12, fontweight='bold', pad=20)
plt.savefig('results/batch_performance_table.png', dpi=150, bbox_inches='tight')
print("✓ Saved: results/batch_performance_table.png")

print("\n" + "="*60)
print("BATCH OPERATIONS PERFORMANCE SUMMARY")
print("="*60)
print(df.to_string(index=False))
print("="*60)
print(f"\nBest Performance: {df.loc[df['speedup'].idxmax()]['batch_size']:.0f} pages")
print(f"  - Speedup: {df['speedup'].max():.2f}×")
print(f"  - Throughput: {df['throughput_pages_per_ms'].max():.0f} pages/ms")
print(f"  - Per-page latency: {df['avg_per_page_us'].min():.2f} µs")
print("="*60)
