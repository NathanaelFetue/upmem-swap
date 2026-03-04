# UPMEM Swap Makefile
# Builds both HOST (C) and DPU (DPU-C) applications

# When UPMEM SDK becomes available, this Makefile will:
# 1. Compile HOST application with dpu-upmem-dpurte-clang
# 2. Compile DPU kernels with dpu compiler
# 3. Link everything together

.PHONY: all clean test help

# Directories
BUILD_DIR := build
SRC_HOST_DIR := prototype/host
SRC_DPU_DIR := prototype/dpu
DOCS_DIR := docs

# Output files
HOST_BIN := $(BUILD_DIR)/host
DPU_BIN := $(BUILD_DIR)/dpu

# DPU toolchain
DPU_CC ?= $(UPMEM_HOME)/bin/dpu-clang
DPU_CFLAGS := -I$(UPMEM_HOME)/include -I$(UPMEM_HOME)/include/dpu -O2 -D__DPU__

# Check UPMEM SDK availability
UPMEM_SDK_PATH ?= $(shell which dpu-upmem-dpurte-clang 2>/dev/null)
HAVE_SDK := $(if $(UPMEM_SDK_PATH),1,0)

# Derive UPMEM_HOME from the SDK binary path (if available)
UPMEM_HOME := $(patsubst %/bin/,%,$(dir $(UPMEM_SDK_PATH)))

.PHONY: check-sdk
check-sdk:
	@echo "=== UPMEM SDK Status ==="
ifeq ($(HAVE_SDK),1)
	@echo "✓ UPMEM SDK Found: $(UPMEM_SDK_PATH)"
	@echo "Ready to build DPU applications"
else
	@echo "✗ UPMEM SDK Not Available"
	@echo "Building HOST application only (no DPU acceleration)"
endif

# Default target
all: check-sdk
	@mkdir -p $(BUILD_DIR)
ifeq ($(HAVE_SDK),1)
	@echo "Building with UPMEM SDK..."
	gcc -I$(SRC_HOST_DIR) -I$(UPMEM_HOME)/include -I$(UPMEM_HOME)/include/dpu -DHAVE_DPU_H -O2 -o $(HOST_BIN) $(SRC_HOST_DIR)/main.c \
		-L$(UPMEM_HOME)/lib -ldpu -Wl,-rpath,$(UPMEM_HOME)/lib
	@echo "Building DPU kernel..."
	mkdir -p $(BUILD_DIR)
	$(DPU_CC) $(DPU_CFLAGS) -o $(DPU_BIN) $(SRC_DPU_DIR)/main.c || true
else
	@echo "Building without UPMEM SDK (development mode)..."
	gcc -I$(SRC_HOST_DIR) -o $(HOST_BIN) $(SRC_HOST_DIR)/main.c
endif
	@echo "Build complete: $(HOST_BIN)"

# Run application
run: all
	@echo "=== Running UPMEM Swap ==="
	$(HOST_BIN)

# Clean build artifacts
clean:
	rm -rf $(BUILD_DIR)
	find . -name "*.o" -delete
	find . -name "*.d" -delete
	@echo "Clean complete"

# Help target
help:
	@echo "UPMEM Swap Build System"
	@echo ""
	@echo "Targets:"
	@echo "  make              - Build HOST application"
	@echo "  make run          - Build and run"
	@echo "  make check-sdk    - Check UPMEM SDK availability"
	@echo "  make clean        - Remove build artifacts"
	@echo "  make help         - Show this help"
	@echo ""
	@echo "SDK Status: $(HAVE_SDK) (1=Available, 0=Not Available)"
	@echo ""
	@echo "Documentation:"
	@echo "  - Project Plan: $(DOCS_DIR)/PROJECT_PLAN.md"
	@echo "  - UPMEM Docs:  https://sdk.upmem.com/stable/"
	@echo "  - GitHub:      https://github.com/upmem/dpu_demo"


# Test 4KB pages
test_4kb: prototype/host/test_4kb_pages.c $(BUILD_DIR)/dpu
	@echo "Building 4KB page test..."
	gcc -I$(UPMEM_HOME)/include -I$(UPMEM_HOME)/include/dpu \
	    -o $(BUILD_DIR)/test_4kb \
	    prototype/host/test_4kb_pages.c \
	    -L$(UPMEM_HOME)/lib -ldpu -lm \
	    -Wl,-rpath,$(UPMEM_HOME)/lib
	@echo "✓ test_4kb built successfully"

run_4kb: test_4kb
	@echo "=== Running 4KB Page Test ==="
	$(BUILD_DIR)/test_4kb
