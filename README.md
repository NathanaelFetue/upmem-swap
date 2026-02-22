# upmem-swap

UPMEM swap memory optimization project.

### Project Structure
```
src/
├── host/        # HOST application (CPU side)
├── dpu/         # DPU kernels (coprocessor side)
docs/
├── PROJECT_PLAN.md
```

# UPMEM Swap Prototype

Exploring Processing-In-Memory technology as a fast memory tier between RAM and SSD.

## Objective

Investigate whether UPMEM PIM can serve as an intermediate memory tier with lower latency than traditional SSD swap. Potential applications include:
- Container memory management under resource constraints
- Database buffer pool extension
- Memory-intensive workloads on memory-constrained systems

**Current Status:** Phase 1 - HOST↔DPU transfer mechanism validated

## Results

**Prototype measurements (simulator):**
- Round-trip latency: 155 µs (256 bytes)
- Transfer overhead: significant bottleneck identified

**Hardware expectations (from literature):**
- HOST↔DPU transfers: 5-90 ms
- SSD swap baseline: ~100 µs

**Key finding:** Transfer overhead dominates performance. Pure swap operations not viable without optimization strategies (batching, compression).

## Technical Details

### Architecture
```
Host Application (C)
    ↓ SDK API
DPU Kernel (C)
    ↓ MRAM operations
UPMEM Device (simulator/hardware)
```

### Implementation
- **Host:** `dpu_alloc`, `dpu_load`, `dpu_prepare_xfer`, `dpu_push_xfer`, `dpu_launch`
- **DPU:** `mram_read`, `mram_write` (max 2048 bytes per transfer)
- **Validation:** Byte inversion test (0xA5 → 0x5A)

## Build
```bash
make              # Build host application
make run          # Build and run
make check-sdk    # Verify SDK installation
```

## SDK Status

**RESOLVED!** The UPMEM SDK is now available from the community archive:
- Repository: https://github.com/kagandikmen/upmem-sdk
- Version: 2025.1.0 (Linux x86_64, Ubuntu 20.04)
- Status: Integrated in devcontainer

The dev container will automatically download and install the SDK.

## Getting Started

### Build the Dev Container
```bash
devcontainer rebuild
```


## Documentation

- [Project Plan](docs/PROJECT_PLAN.md)
- [SDK Setup](./devcontainer/SDK_SETUP.md)

## SDK Access Note

The official SDK infrastructure became unavailable in 2025. This project uses a community-maintained archive:
- Repository: https://github.com/kagandikmen/upmem-sdk
- Version: 2025.1.0


## License

MIT License

## Related Issues
- Original SDK access issue: https://github.com/upmem/dpu_demo/issues/17
- Community archive: https://github.com/kagandikmen/upmem-sdk
