#!/usr/bin/env python3
import pandas as pd
import matplotlib.pyplot as plt

# Read results
df = pd.read_csv('results/benchmark_batch_improved.csv')
# compute combined latency per page
df['combined_us_per_page'] = df['avg_swapout_per_page_us'] + df['avg_swapin_per_page_us']

best = df.loc[df['combined_us_per_page'].idxmin()]

plt.figure(figsize=(8,5))
plt.plot(df['batch_size'], df['avg_swapout_per_page_us'], marker='o', label='Swap-Out (µs/page)')
plt.plot(df['batch_size'], df['avg_swapin_per_page_us'], marker='s', label='Swap-In (µs/page)')
plt.plot(df['batch_size'], df['combined_us_per_page'], marker='^', label='Combined (µs/page)', linewidth=2)
plt.scatter([best['batch_size']], [best['combined_us_per_page']], color='red', zorder=5)
plt.text(best['batch_size'], best['combined_us_per_page']*1.01, f"Best: {int(best['batch_size'])}", ha='center', color='red')
plt.xscale('linear')
plt.xlabel('Batch Size (pages)')
plt.ylabel('Latency (µs per page)')
plt.title('UPMEM: Swap-Out, Swap-In and Combined Latency per Page')
plt.grid(True, linestyle='--', alpha=0.4)
plt.legend()
plt.tight_layout()
plt.savefig('results/05_combined_latency.png', dpi=150)
print('✓ Saved: results/05_combined_latency.png')
