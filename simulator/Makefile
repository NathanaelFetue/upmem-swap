# ===== Makefile for UPMEM Swap Simulator =====

CC = gcc
DPU_CC = dpu-upmem-dpurte-clang

CFLAGS = -Wall -Wextra -O2 -g -std=c99
LDFLAGS = -lm

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
          src/upmem_swap.c src/workload.c src/stats.c src/ssd_baseline.c
HEADERS = src/*.h
OBJECTS = $(SOURCES:.c=.o)

# Targets
TARGET = swap_sim
DPU_TARGET = dpu/dpu_program

# Main target
all: $(TARGET)

# Build simulator
$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJECTS) $(LDFLAGS)
	@echo "Build successful: $(TARGET)"

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# DPU program (optional)
dpu_program: $(DPU_TARGET)

ifeq ($(DPU_COMPILE),yes)
$(DPU_TARGET): dpu/dpu_program.c
	$(DPU_CC) -o $(DPU_TARGET) dpu/dpu_program.c
else
$(DPU_TARGET):
	@echo "Skipping DPU compilation (SDK not available)"
endif

# Run tests
test: $(TARGET)
	@echo "Running basic test..."
	./$(TARGET) --ram-mb 32 --dpus 16 --accesses 10000

test_with_swap: $(TARGET)
	@echo "Running test with ACTUAL swap (small RAM)..."
	./$(TARGET) --ram-mb 1 --dpus 8 --accesses 5000 --working-set 2000 --output results/with_swap.csv

test_scaling: $(TARGET)
	@echo "Running scaling tests..."
	./$(TARGET) --dpus 1 --accesses 5000 --output results/1dpu.csv
	./$(TARGET) --dpus 4 --accesses 5000 --output results/4dpu.csv
	./$(TARGET) --dpus 8 --accesses 5000 --output results/8dpu.csv
	./$(TARGET) --dpus 16 --accesses 5000 --output results/16dpu.csv

test_patterns: $(TARGET)
	@echo "Running pattern tests..."
	./$(TARGET) --workload random --output results/random.csv
	./$(TARGET) --workload sequential --output results/sequential.csv
	./$(TARGET) --workload mixed --output results/mixed.csv

# Cleanup
clean:
	rm -f $(OBJECTS) $(TARGET)
	rm -f dpu/dpu_program
	rm -f results/*.csv

# Debug build
debug: CFLAGS += -DDEBUG=1
debug: clean $(TARGET)

.PHONY: all clean test test_scaling test_patterns debug dpu_program
