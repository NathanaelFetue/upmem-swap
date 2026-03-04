# UPMEM Swap Repository

This repository currently contains two tracks:

1. `simulator/` (MAIN for swap study, benchmarks, figures, paper workflow)
2. root-level `prototype/` (older hardware/prototype code path)

If your goal is to understand the swap work (latency, swap-in path, baselines, article figures), start in `simulator/`.

## Where To Start

- Primary entrypoint: `simulator/README.md`
- Main simulator sources: `simulator/src/`
- Benchmark results: `simulator/results/`
- Figure generation scripts: `scripts/`
- Paper draft: `simulator/article.tex`

## What Is The Main Project Today?

For the current swap task, the main project is the simulator stack in `simulator/`:

- page table + LRU victim selection
- userspace swap manager
- UPMEM latency model (including swap-in path)
- backend comparison (UPMEM, SSD, zram, zswap)
- export to CSV and generation of article figures

The root-level `prototype/host` and `prototype/dpu` directories are still useful as prototype/hardware-oriented artifacts, but they are not the primary path used for the swap evaluation and article results.

## Quick Start (Swap Workflow)

```bash
cd simulator
make clean
make
make bench_comparison
make images
```

Outputs are generated in `simulator/results/`.

## Repository Map

```text
.
├── simulator/           # Main swap simulator project (recommended entrypoint)
│   ├── README.md
│   ├── src/
│   ├── results/
│   └── Makefile
├── scripts/             # Figure/data generation scripts used by simulator results
├── prototype/           # Legacy/prototype host+dpu path
├── README_BUILD.md
├── README_FIGURES.md
└── ARTICLE_GUIDE.md
```

## Notes

- If someone lands at repository root, they should read this file first, then go to `simulator/README.md`.
- Root legacy hardware code has already been renamed to `prototype/` to avoid confusion with `simulator/src/`.

## License

MIT
