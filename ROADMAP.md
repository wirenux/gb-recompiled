# GameBoy Recompiler Roadmap

> Last updated: January 3, 2026

## Overview

This document tracks the implementation progress of the GameBoy static recompiler based on the architecture defined in [ARCH.md](ARCH.md).

---

## Phase 1: Foundation ✅ COMPLETE

**Goal**: Minimal working recompiler for simple 32KB ROMs (no banking)

| Task | Status | Notes |
|------|--------|-------|
| Project setup (CMake, directory structure) | ✅ | CMake with C++20, modular structure |
| ROM loader with header parsing | ✅ | Title, MBC type, ROM size, checksums |
| MBC type detection | ✅ | Detects ROM ONLY, MBC1-5 |
| SM83 instruction decoder (~500 opcodes) | ✅ | Full opcode + CB-prefix support |
| Basic IR builder | ✅ | Converts decoded instructions to IR |
| CPU context structure | ✅ | GBContext with registers, flags, memory |
| Memory bus implementation (no banking) | ✅ | gb_read8/gb_write8 in runtime |
| C code emitter | ✅ | Generates compilable C from IR |
| Simple test ROM working | ✅ | Test ROM executes correctly |

**Milestone Achieved**: Successfully recompiles and runs a 32KB no-MBC ROM with:
- Register operations (LD, INC, DEC)
- ALU operations (ADD, SUB, AND, OR, XOR)
- Control flow (JP, JR, conditional jumps, loops)
- Memory access (LD (nn),A)
- HALT instruction

---

## Phase 2: Control Flow & Analysis ✅ COMPLETE

**Goal**: Proper function detection and control flow

| Task | Status | Notes |
|------|--------|-------|
| Control flow analyzer | ✅ | Builds CFG from decoded instructions |
| Jump/call target identification | ✅ | Tracks JP, JR, CALL targets |
| Basic block construction | ✅ | Blocks split at jumps/labels |
| Function boundary detection | ✅ | Functions from call_targets |
| Handle conditional jumps (JP cc, JR cc) | ✅ | Correct target calculation |
| RST vector handling | ✅ | RST 00-38 as functions |
| Reachability analysis | ✅ | DFS from entry point |
| Interrupt vector stubs | ✅ | VBlank, LCD, Timer, Serial, Joypad |

**Milestone Achieved**: Handles ROMs with multiple functions, loops, and conditional branches.

---

## Phase 3: Bank Switching 🔲 NOT STARTED

**Goal**: Support for MBC1/MBC3/MBC5 games

| Task | Status | Notes |
|------|--------|-------|
| Bank tracker implementation | 🔲 | |
| MBC1 support | 🔲 | Most common MBC |
| MBC3 support | 🔲 | Pokémon games, includes RTC |
| MBC5 support | 🔲 | Later games |
| Cross-bank call detection | 🔲 | |
| Per-bank function generation | 🔲 | bank01_func_XXXX naming |
| Runtime bank dispatch | 🔲 | For unknown bank states |
| RAM banking support | 🔲 | External cartridge RAM |

**Target**: Recompile banked ROMs (e.g., Tetris DX, Pokémon)

---

## Phase 4: PPU (Graphics) 🔲 NOT STARTED

**Goal**: Visual output

| Task | Status | Notes |
|------|--------|-------|
| Background rendering | 🔲 | Tile-based background |
| Window rendering | 🔲 | Overlay window layer |
| Sprite rendering (8x8, 8x16) | 🔲 | OAM-based sprites |
| Scanline timing | 🔲 | Mode 0/1/2/3 transitions |
| VBlank interrupt | 🔲 | Frame sync |
| LCD STAT interrupt | 🔲 | Scanline triggers |
| VRAM access timing | 🔲 | Basic timing restrictions |
| Palette handling | 🔲 | BGP, OBP0, OBP1 |
| SDL2 rendering backend | 🔲 | Already linked in project |

**Target**: Games display correctly

---

## Phase 5: Interrupts & Timing 🔲 NOT STARTED

**Goal**: Accurate timing and interrupt handling

| Task | Status | Notes |
|------|--------|-------|
| Full interrupt controller | 🟡 | Stubs exist, need implementation |
| Timer (DIV, TIMA, TMA, TAC) | 🔲 | |
| Timer interrupt | 🔲 | |
| Joypad input | 🔲 | |
| Joypad interrupt | 🔲 | |
| DMA transfers | 🔲 | OAM DMA |
| Cycle-accurate yielding | 🔲 | Cooperative multitasking |

**Target**: Timing-sensitive games work

---

## Phase 6: Audio 🔲 NOT STARTED

**Goal**: Sound output

| Task | Status | Notes |
|------|--------|-------|
| Channel 1 (Pulse + sweep) | 🔲 | |
| Channel 2 (Pulse) | 🔲 | |
| Channel 3 (Wave) | 🔲 | |
| Channel 4 (Noise) | 🔲 | |
| Audio mixing | 🔲 | |
| SDL2 audio backend | 🔲 | |

**Target**: Games have sound

---

## Phase 7: Polish & Optimization 🔲 NOT STARTED

**Goal**: Production quality

| Task | Status | Notes |
|------|--------|-------|
| IR optimization passes | 🔲 | Const prop, dead code elim |
| Test ROM compatibility | 🔲 | Blargg's, Mooneye tests |
| Commercial game testing | 🔲 | |
| Debug overlay (ImGui) | 🔲 | |
| Performance profiling | 🔲 | |
| Save state support | 🔲 | |
| Save file support | 🔲 | Battery-backed RAM |
| Documentation | 🟡 | ARCH.md exists |

**Target**: Release-ready recompiler

---

## Future Enhancements (Post-MVP)

| Feature | Status | Priority |
|---------|--------|----------|
| LLVM backend | 🔲 | Medium |
| Game Boy Color support | 🔲 | High |
| Super Game Boy support | 🔲 | Low |
| Link cable emulation | 🔲 | Low |
| Debugger integration | 🔲 | Medium |
| Web/WASM target | 🔲 | Medium |

---

## Current Capabilities

### What Works Now ✅
```
ROM Loading → Decoding → Analysis → IR → C Generation → Compilation → Execution
```

- **Input**: 32KB GameBoy ROM (no MBC)
- **Output**: Portable C code + runtime library
- **Tested**: Custom test ROM with loops, jumps, ALU ops, memory stores

### Test Command
```bash
./build/bin/gbrecomp test.gb -o test_output
cd test_output && gcc *.c -I../runtime/include ../runtime/src/gbrt.c -o test && ./test
```

### Sample Output
```
Recompiled code executed successfully!
Registers: A=42 B=00 C=13
```

---

## Legend

| Symbol | Meaning |
|--------|---------|
| ✅ | Complete |
| 🟡 | Partial / In Progress |
| 🔲 | Not Started |

---

## Quick Stats

| Metric | Value |
|--------|-------|
| Phases Complete | 2 of 7 |
| Core Recompiler | Working |
| Graphics | Not implemented |
| Sound | Not implemented |
| Bank Switching | Not implemented |
| Estimated Completion | ~20 weeks remaining |
