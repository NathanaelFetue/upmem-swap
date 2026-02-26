#!/usr/bin/env python3
import csv
import matplotlib.pyplot as plt
import numpy as np
from pathlib import Path

ROOT = Path(__file__).resolve().parent
RESULTS = ROOT / "results"
INPUTS = {
    "A\nCPU": RESULTS / "optionA.csv",
    "B\nDPU": RESULTS / "optionB.csv",
    "Base": RESULTS / "optionC.csv",
}
OUT = RESULTS / "compression_tradeoffs.png"
OUT2 = RESULTS / "compression_latency_cpu_tradeoff.png"

rows = {}
for label, path in INPUTS.items():
    if not path.exists():
        raise SystemExit(f"Missing input CSV: {path}")
    with path.open() as f:
        reader = csv.DictReader(f)
        row = next(reader, None)
        if row is None:
            raise SystemExit(f"Empty CSV: {path}")
        rows[label] = row

labels = list(rows.keys())
swapout = []
swapin = []
cpu_compress = []
cpu_decompress = []
for label in labels:
    r = rows[label]
    swapout.append(float(r["avg_swapout_us"]))
    swapin.append(float(r["avg_swapin_us"]))
    cpu_compress.append(float(r.get("avg_cpu_compress_us", 0.0)))
    cpu_decompress.append(float(r.get("avg_cpu_decompress_us", 0.0)))

plt.rcParams.update({"font.size": 10})
fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(6, 3), dpi=100)

x = np.arange(len(labels))
w = 0.35

ax1.bar(x - w / 2, swapout, w, label="Swap-out latency (µs)", color="#444444")
ax1.bar(x + w / 2, cpu_compress, w, label="CPU compress (µs)", color="#aaaaaa")
ax1.set_xticks(x)
ax1.set_xticklabels(labels, fontsize=9)
ax1.set_ylabel("Microseconds", fontsize=9)
ax1.set_title("Swap-out", fontsize=9)
ax1.legend(fontsize=7, loc="upper right", frameon=True)
ax1.grid(axis="y", alpha=0.3)

ax2.bar(x - w / 2, swapin, w, label="Swap-in latency (µs)", color="#666666")
ax2.bar(x + w / 2, cpu_decompress, w, label="CPU decompress (µs)", color="#c0c0c0")
ax2.set_xticks(x)
ax2.set_xticklabels(labels, fontsize=9)
ax2.set_ylabel("Microseconds", fontsize=9)
ax2.set_title("Swap-in (critical)", fontsize=9)
ax2.legend(fontsize=7, loc="upper right", frameon=True)
ax2.grid(axis="y", alpha=0.3)

fig.tight_layout(pad=0.8)
OUT.parent.mkdir(parents=True, exist_ok=True)
fig.savefig(OUT, dpi=100)
print(f"Figure saved: {OUT} (600x300px)")

# Second compact figure: measured CPU decompression overhead vs swap-in latency (from CSV)
fig2, ax = plt.subplots(figsize=(6, 3), dpi=100)
labels_scatter = ["Option A (CPU)", "Option B (DPU)", "Baseline"]
csv_keys = ["A\nCPU", "B\nDPU", "Base"]
cpu_decomp_measured = [float(rows[k].get("avg_cpu_decompress_us", 0.0)) for k in csv_keys]
swapin_measured = [float(rows[k]["avg_swapin_us"]) for k in csv_keys]
point_colors = ["#333333", "#777777", "#b0b0b0"]

for i, label in enumerate(labels_scatter):
    ax.scatter(cpu_decomp_measured[i], swapin_measured[i], s=72,
               c=point_colors[i], edgecolors="black")
    ax.annotate(label,
                (cpu_decomp_measured[i], swapin_measured[i]),
                xytext=(4, 4),
                textcoords="offset points",
                fontsize=7)

ax.set_xlabel("CPU decompression overhead (µs)", fontsize=9)
ax.set_ylabel("Swap-in latency (µs)", fontsize=9)
ax.set_title("Swap-in latency vs CPU decompression", fontsize=9)
ax.grid(alpha=0.3)
ax.set_xlim(left=0.0)
ax.set_ylim(14, 53)
fig2.tight_layout(pad=0.8)
fig2.savefig(OUT2, dpi=100)
print(f"Figure saved: {OUT2} (600x300px)")
