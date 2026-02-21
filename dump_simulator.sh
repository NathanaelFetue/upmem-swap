#!/usr/bin/env bash
# dump_simulator.sh - Extract all files from simulator/ into a single txt document

set -euo pipefail
OUTPUT_FILE="simulator_dump.txt"

echo "Generating complete simulator source dump..."

{
    echo "=================================================="
    echo "UPMEM Swap Simulator - Complete Source Dump"
    echo "=================================================="
    echo "Generated: $(date)"
    echo "=================================================="
    echo ""

    # Find all source files (exclude binaries, images, archives, dist/)
    find simulator -type f \
        \( -name "*.c" -o -name "*.h" -o -name "*.md" \
           -o -name "*.py" -o -name "Makefile" \) \
        ! -path "*/\.git/*" \
        ! -path "*/dist/*" \
        ! -name "simulator_dump.txt" | sort | while read -r filepath; do
        
        echo ""
        echo "==============================================="
        echo "FILE: $filepath"
        echo "==============================================="
        echo ""
        cat "$filepath"
        echo ""
    done

    echo ""
    echo "==============================================="
    echo "END OF DUMP"
    echo "==============================================="
} > "$OUTPUT_FILE"

# Count files
LINES=$(wc -l < "$OUTPUT_FILE")
SIZE=$(du -h "$OUTPUT_FILE" | cut -f1)

echo "✓ Dump complete: $OUTPUT_FILE"
echo "  Size: $SIZE"
echo "  Lines: $LINES"
