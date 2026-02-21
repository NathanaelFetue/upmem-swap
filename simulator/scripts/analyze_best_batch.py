#!/usr/bin/env python3
import pandas as pd

# Read latest results
df = pd.read_csv('results/benchmark_batch_improved.csv')

# Compute combined per-page latency (out + in)
df['combined_us_per_page'] = df['avg_swapout_per_page_us'] + df['avg_swapin_per_page_us']

# Find best batch minimizing combined latency
best_idx = df['combined_us_per_page'].idxmin()
best_row = df.loc[best_idx]

print('Analysis of results/benchmark_batch_improved.csv')
print('Total pages:', int(df.iloc[0]['total_swapout_us'] / df.iloc[0]['avg_swapout_per_page_us'] if 'total_swapout_us' in df.columns else 'N/A'))
print('\nBatch sizes tested:', df['batch_size'].tolist())
print('\nPer-batch detailed:')
print(df[['batch_size','avg_swapout_per_page_us','avg_swapin_per_page_us','combined_us_per_page','speedup_swapout','speedup_swapin']].to_string(index=False))

print('\nBest batch by combined per-page latency:')
print(f"Batch size = {int(best_row['batch_size'])}")
print(f"  Swap-out per page: {best_row['avg_swapout_per_page_us']:.2f} µs")
print(f"  Swap-in  per page: {best_row['avg_swapin_per_page_us']:.2f} µs")
print(f"  Combined per page: {best_row['combined_us_per_page']:.2f} µs")
print(f"  Speedup (out): {best_row['speedup_swapout']:.2f}×")
print(f"  Speedup (in):  {best_row['speedup_swapin']:.2f}×")
