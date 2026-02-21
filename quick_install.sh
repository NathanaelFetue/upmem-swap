#!/usr/bin/env bash
set -euo pipefail

# quick_install.sh - One-line installation helper for UPMEM Swap module

REPO="${1:-https://github.com/Pegasus04-Nathanael/upmem-swap}"
DEST="${2:-$HOME/.local}"
CLONE_DIR="/tmp/upmem_swap_install_$$"

echo "=================================================="
echo "UPMEM Swap Module - Quick Installer"
echo "=================================================="
echo "Repository:  $REPO"
echo "Install to:  $DEST"
echo ""

# Cleanup on exit
cleanup() {
    rm -rf "$CLONE_DIR"
}
trap cleanup EXIT

# Clone
echo "[1/4] Cloning repository..."
git clone --depth 1 "$REPO" "$CLONE_DIR"
cd "$CLONE_DIR"

# Build
echo "[2/4] Building with CMake..."
mkdir -p build
cd build
cmake .. \
    -DCMAKE_INSTALL_PREFIX="$DEST" \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_EXAMPLES=ON
make -j$(nproc)

# Install
echo "[3/4] Installing..."
make install

# Summary
echo "[4/4] Installation complete!"
echo ""
echo "=================================================="
echo "Installation Summary"
echo "=================================================="
echo "Library installed to:   $DEST/lib"
echo "Headers installed to:   $DEST/include/upmem_swap"
echo "Examples in:            $DEST/bin"
echo ""
echo "To use in your CMake project:"
echo "  find_package(UPMEMSwap REQUIRED)"
echo "  target_link_libraries(your_app UPMEMSwap::upmem_swap)"
echo ""
echo "Or manually:"
echo "  gcc myapp.c -I$DEST/include -L$DEST/lib -lupmem_swap -lm"
echo ""
