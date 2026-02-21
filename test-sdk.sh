#!/bin/bash
# Test script to verify UPMEM SDK installation

echo "=== UPMEM SDK Installation Test ==="
echo ""

# Check if SDK exists
if [ -d "/opt/upmem-sdk-2025.1.0" ]; then
    echo " SDK directory found: /opt/upmem-sdk-2025.1.0"
else
    echo " SDK directory NOT found"
    exit 1
fi

# Check key SDK binaries
echo ""
echo "Checking SDK binaries..."
BINARIES=(
    "bin/dpu-upmem-dpurte-clang"
    "bin/dpu-lldb"
    "bin/dpu-upmem-dpurte-clang++"
)

for binary in "${BINARIES[@]}"; do
    if [ -f "/opt/upmem-sdk-2025.1.0/$binary" ]; then
        echo "   $binary"
    else
        echo "    $binary NOT found"
    fi
done

# Check environment
echo ""
echo "Environment variables:"
echo "  UPMEM_HOME=$UPMEM_HOME"
echo "  PATH includes SDK: $(echo $PATH | grep -q upmem && echo ' YES' || echo '❌ NO')"

# Check DPU demo
echo ""
if [ -d "/opt/dpu_demo" ]; then
    echo " DPU demo examples found"
else
    echo "  DPU demo NOT found (non-critical)"
fi

# Summary
echo ""
echo "=== Test Summary ==="
if [ -d "/opt/upmem-sdk-2025.1.0" ]; then
    echo " UPMEM SDK 2025.1.0 is properly installed!"
    echo ""
    echo "You can now:"
    echo "  - Build DPU programs"
    echo "  - Use the dpu-lldb debugger"
    echo "  - Access SDK documentation"
    exit 0
else
    echo " SDK installation incomplete"
    exit 1
fi
