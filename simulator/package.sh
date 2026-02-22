#!/usr/bin/env bash
set -euo pipefail

# Simple packaging script: gathers headers and sources required to integrate
# the upmem_swap module into another project and creates a tarball.

PKG_DIR="dist/upmem_swap_module"
mkdir -p "$PKG_DIR"

# Files to include
cp src/upmem_swap.h "$PKG_DIR/"
cp src/upmem_swap.c "$PKG_DIR/"
cp src/page_table.h "$PKG_DIR/"
cp src/page_table.c "$PKG_DIR/"
cp src/memory_sim.h "$PKG_DIR/"
cp src/memory_sim.c "$PKG_DIR/"
cp src/config.h "$PKG_DIR/"
cp example_integration.c "$PKG_DIR/"

# Minimal README
cat > "$PKG_DIR/README.md" <<'EOF'
# UPMEM Swap Module (packaged)

Include these files into your project:

- `upmem_swap.h` / `upmem_swap.c` : swap manager implementation
- `page_table.h` / `page_table.c` : simple page table used by the module
- `memory_sim.h` / `memory_sim.c` : RAM simulator helpers (optional)
- `config.h` : configuration constants (PAGE_SIZE, DPU_MRAM_SIZE)
- `example_integration.c` : small example showing API usage

Build: include the `.c` files in your build or compile into a static library.

EOF

tar -C dist -czf dist/upmem_swap_module.tar.gz upmem_swap_module
echo "Created dist/upmem_swap_module.tar.gz"
