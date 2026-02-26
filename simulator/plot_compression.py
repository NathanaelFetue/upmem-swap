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
cpu = []
ratio = []
for label in labels:
    r = rows[label]
    swapout.append(float(r["avg_swapout_us"]))
    cpu.append(float(r.get("avg_cpu_overhead_us", 0.0)))
    ratio.append(float(r.get("compression_ratio", 1.0)))

plt.rcParams.update({"font.size": 10})
fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(6, 3), dpi=100)

x = np.arange(len(labels))
w = 0.35

ax1.bar(x - w / 2, swapout, w, label="Swap-out (µs)", color="#444444")
ax1.bar(x + w / 2, cpu, w, label="CPU overhead (µs)", color="#aaaaaa")
ax1.set_xticks(x)
ax1.set_xticklabels(labels, fontsize=9)
ax1.set_ylabel("Microseconds", fontsize=9)
ax1.legend(fontsize=7, loc="upper right", frameon=True)
ax1.grid(axis="y", alpha=0.3)

ax2.bar(labels, ratio, color="#777777")
ax2.set_ylabel("Compression ratio", fontsize=9)
ax2.set_ylim(0, max(8, max(ratio) * 1.1))
ax2.grid(axis="y", alpha=0.3)

fig.tight_layout(pad=0.8)
OUT.parent.mkdir(parents=True, exist_ok=True)
fig.savefig(OUT, dpi=100)
print(f"Figure saved: {OUT} (600x300px)")

# Second compact figure: latency vs CPU overhead positioning
fig2, ax = plt.subplots(figsize=(6, 3), dpi=100)
latency = []
for label in labels:
    r = rows[label]
    latency.append(float(r["avg_swapout_us"]))

point_colors = ["#333333", "#777777", "#b0b0b0"]
for i, label in enumerate(labels):
    ax.scatter(cpu[i], latency[i], s=70, c=point_colors[i], edgecolors="black", label=label.replace("\n", " "))

ax.set_xlabel("CPU overhead (µs/page)", fontsize=9)
ax.set_ylabel("Swap-out latency (µs/page)", fontsize=9)
ax.set_title("Latency vs CPU overhead (A/B/C)", fontsize=10)
ax.legend(fontsize=7, loc="upper right", frameon=True)
ax.grid(alpha=0.3)
ax.set_xlim(left=-0.2)
fig2.tight_layout(pad=0.8)
fig2.savefig(OUT2, dpi=100)
print(f"Figure saved: {OUT2} (600x300px)")
