# PixelCore-SNES

A Super Nintendo Entertainment System emulator for Linux, built in C++20 with Qt 6.

## Current status

The project is at its initial foundation stage. The current code includes:

- A Qt 6 desktop application shell.
- A 24-bit memory-bus foundation with 128 KiB WRAM and initial WRAM mirror handling.
- Initial LoROM reads for the common upper-half bank windows.
- A first 65C816 CPU foundation: reset-vector loading, NOP, immediate LDA/LDX/LDY, and STP.
- CTest-based memory and CPU test executables.

This is **not yet a playable SNES emulator**. Full cartridge/header detection, HiROM/ExHiROM mapping, the complete CPU instruction set, PPU, APU, DMA/HDMA, controller input, and system timing still need to be implemented.

## Build

Dependencies:

- CMake 3.20+
- A C++20 compiler
- Qt 6.2+ Widgets

Build and run tests:

```bash
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Run the desktop shell:

```bash
./build/pixelcore-snes
```

## Planned implementation order

1. Cartridge loading, header detection, and LoROM/HiROM mapping.
2. 65C816 CPU: addressing modes, arithmetic, branches, stack, interrupts, and complete opcode coverage.
3. SNES memory map and hardware registers.
4. PPU: backgrounds, tile decoding, sprites/objects, windows, color math, and video timing.
5. DMA and HDMA.
6. SPC700/APU and audio output.
7. Controllers and input.
8. Master-clock scheduling, integration tests, and game compatibility.

Accuracy will be developed incrementally with focused regression tests for every subsystem.
