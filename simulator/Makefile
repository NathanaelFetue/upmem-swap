# ===== Makefile for UPMEM Swap Simulator (EXTENDED) =====

CC = gcc
DPU_CC = dpu-upmem-dpurte-clang

CFLAGS = -Wall -Wextra -O2 -g -std=c99
LDFLAGS = -lm -llz4

# Try to detect UPMEM SDK
UPMEM_SDK_AVAILABLE = $(shell which dpu-pkg-config > /dev/null 2>&1 && echo yes || echo no)

ifeq ($(UPMEM_SDK_AVAILABLE),yes)
    CFLAGS += $(shell dpu-pkg-config --cflags dpu)
    LDFLAGS += $(shell dpu-pkg-config --libs dpu)
    DPU_COMPILE = yes
else
    $(warning UPMEM SDK not found - building simulator without actual DPU support)
    DPU_COMPILE = no
endif

# Source files
SOURCES = src/main.c src/memory_sim.c src/page_table.c \
          src/upmem_swap.c src/workload.c src/stats.c \
          src/ssd_baseline.c src/zram_backend.c src/zswap_backend.c
HEADERS = src/*.h
OBJECTS = $(SOURCES:.c=.o)

# Targets
TARGET = swap_sim
BENCHMARK_BATCH = benchmark_batch
BENCHMARK_BATCH_IMPROVED = benchmark_batch_improved
BENCHMARK_COMPARISON = benchmark_comparison

# Main target - build all
all: $(TARGET) $(BENCHMARK_BATCH) $(BENCHMARK_BATCH_IMPROVED) $(BENCHMARK_COMPARISON)

# Build simulator
$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJECTS) $(LDFLAGS)
	@echo "✅ Build successful: $(TARGET)"

# Build batch benchmark
$(BENCHMARK_BATCH): src/benchmark_batch.c src/memory_sim.c src/page_table.c \
                    src/upmem_swap.c src/ssd_baseline.c
	$(CC) $(CFLAGS) -o $(BENCHMARK_BATCH) $^ $(LDFLAGS)
	@echo "✅ Build successful: $(BENCHMARK_BATCH)"

# Build improved batch benchmark
$(BENCHMARK_BATCH_IMPROVED): src/benchmark_batch_improved.c src/memory_sim.c \
                             src/page_table.c src/upmem_swap.c src/ssd_baseline.c
	$(CC) $(CFLAGS) -o $(BENCHMARK_BATCH_IMPROVED) $^ $(LDFLAGS)
	@echo "✅ Build successful: $(BENCHMARK_BATCH_IMPROVED)"

# Build comparison benchmark (NEW - all backends)
$(BENCHMARK_COMPARISON): src/benchmark_comparison.c src/memory_sim.c src/page_table.c \
                        src/upmem_swap.c src/ssd_baseline.c \
                        src/zram_backend.c src/zswap_backend.c
	$(CC) $(CFLAGS) -o $(BENCHMARK_COMPARISON) $^ $(LDFLAGS)
	@echo "✅ Build successful: $(BENCHMARK_COMPARISON)"

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# DPU program (optional)
dpu_program: dpu/dpu_program
ifeq ($(DPU_COMPILE),yes)
dpu/dpu_program: dpu/dpu_program.c
	$(DPU_CC) -o $@ $<
else
dpu/dpu_program:
	@echo "Skipping DPU compilation (SDK not available)"
endif

# Run tests
test: $(TARGET)
	@echo "Running basic test..."
	./$(TARGET) --ram-mb 32 --dpus 16 --accesses 10000

test_scaling: $(TARGET)
	@echo "Running scaling tests..."
	@mkdir -p results
	./$(TARGET) --dpus 1 --accesses 5000 --output results/1dpu.csv
	./$(TARGET) --dpus 4 --accesses 5000 --output results/4dpu.csv
	./$(TARGET) --dpus 8 --accesses 5000 --output results/8dpu.csv
	./$(TARGET) --dpus 16 --accesses 5000 --output results/16dpu.csv

# Run batch benchmarks
bench_batch: $(BENCHMARK_BATCH)
	@echo "Running batch operations benchmark..."
	@mkdir -p results
	./$(BENCHMARK_BATCH) 8 100
	@echo "✅ Batch results in: results/benchmark_batch.csv"

bench_batch_improved: $(BENCHMARK_BATCH_IMPROVED)
	@echo "Running improved batch operations benchmark..."
	@mkdir -p results
	./$(BENCHMARK_BATCH_IMPROVED) 8 100
	@echo "✅ Improved batch results in: results/benchmark_batch_improved.csv"

# Run comparison benchmark (NEW - all backends)
bench_comparison: $(BENCHMARK_COMPARISON)
	@echo "Running ALL backends comparison..."
	@mkdir -p results
	./$(BENCHMARK_COMPARISON) 8 1000
	@echo "✅ Comparison results in: results/backend_comparison.csv"

# Generate images (requires Python + matplotlib)
images: bench_comparison
	@echo "Generating comparison images..."
	@cd .. && python3 scripts/generate_final_figures.py
	@python3 plot_compression.py
	@echo "✅ Images generated in results/"

# Full article workflow
article: bench_comparison images
	@echo ""
	@echo "╔════════════════════════════════════════════════════╗"
	@echo "║     ARTICLE WORKFLOW COMPLETE                      ║"
	@echo "╚════════════════════════════════════════════════════╝"
	@echo ""
	@echo "📊 Results:"
	@echo "  - results/backend_comparison.csv"
	@echo "  - results/Figure2_Speedup.png"
	@echo "  - results/Figure3_Baselines.png"
	@echo "  - results/compression_tradeoffs.png"
	@echo ""
	@echo "📝 Ready for LaTeX inclusion!"

# Cleanup
clean:
	rm -f $(OBJECTS) $(TARGET) $(BENCHMARK_BATCH) $(BENCHMARK_BATCH_IMPROVED) $(BENCHMARK_COMPARISON)
	rm -f dpu/dpu_program
	rm -f src/*.o

clean_results:
	rm -f results/*.csv results/*.png

# Debug build
debug: CFLAGS += -DDEBUG=1
debug: clean all

.PHONY: all clean clean_results test test_scaling debug dpu_program \
        bench_batch bench_batch_improved bench_comparison images article
