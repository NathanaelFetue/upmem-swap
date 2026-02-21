# UPMEM Swap Module (packaged)

Include these files into your project:

- `upmem_swap.h` / `upmem_swap.c` : swap manager implementation
- `page_table.h` / `page_table.c` : simple page table used by the module
- `memory_sim.h` / `memory_sim.c` : RAM simulator helpers (optional)
- `config.h` : configuration constants (PAGE_SIZE, DPU_MRAM_SIZE)
- `example_integration.c` : small example showing API usage

Build: include the `.c` files in your build or compile into a static library.

