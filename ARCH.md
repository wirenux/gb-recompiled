# GameBoy Recompiler Reference Guide

## Based on Analysis of CHIP-8 Recompiled Project

This document provides a comprehensive analysis of the CHIP-8 static recompiler architecture to serve as a blueprint for building a GameBoy (DMG/CGB) static recompiler.

---

## Table of Contents

1. [What is Static Recompilation?](#what-is-static-recompilation)
2. [CHIP-8 Project Architecture Overview](#chip-8-project-architecture-overview)
3. [Key Components Deep Dive](#key-components-deep-dive)
4. [Recompilation Pipeline](#recompilation-pipeline)
5. [Code Generation Strategy](#code-generation-strategy)
6. [Runtime Design](#runtime-design)
7. [GameBoy vs CHIP-8 Comparison](#gameboy-vs-chip-8-comparison)
8. [GameBoy Recompiler Architecture Proposal](#gameboy-recompiler-architecture-proposal)
9. [Implementation Roadmap](#implementation-roadmap)

---

## What is Static Recompilation?

Static recompilation (AOT - Ahead-of-Time compilation) transforms binary code from one instruction set to another **before** runtime.

### Advantages Over Emulation

| Approach | Execution | Optimization | Overhead |
|----------|-----------|--------------|----------|
| **Interpretation** | Decode & execute at runtime | None | High |
| **JIT (Dynamic)** | Compile hot paths at runtime | Runtime-limited | Medium |
| **Static Recompilation** | Pre-compile to native code | Full compiler optimizations | Zero |

### How It Works

```
┌─────────────────┐     ┌─────────────────┐     ┌─────────────────┐
│   Source ROM    │────▶│   Recompiler    │────▶│   C Source      │
│   (binary)      │     │   (translator)  │     │   Code          │
└─────────────────┘     └─────────────────┘     └─────────────────┘
                                                        │
                                                        ▼
┌─────────────────┐     ┌─────────────────┐     ┌─────────────────┐
│   Native        │◀────│   C Compiler    │◀────│   Runtime       │
│   Executable    │     │   (gcc/clang)   │     │   Library       │
└─────────────────┘     └─────────────────┘     └─────────────────┘
```

---

## CHIP-8 Project Architecture Overview

### Directory Structure

```
chip8-recompiled/
├── recompiler/                 # The recompiler tool (chip8recomp)
│   ├── include/recompiler/
│   │   ├── decoder.h           # Instruction decoder interface
│   │   ├── analyzer.h          # Control flow analyzer
│   │   ├── generator.h         # C code generator
│   │   ├── rom.h               # ROM loader
│   │   ├── config.h            # Configuration parser
│   │   └── batch.h             # Batch compilation
│   └── src/
│       ├── main.cpp            # CLI entry point
│       ├── decoder.cpp         # Opcode decoding
│       ├── analyzer.cpp        # CFG analysis
│       ├── generator.cpp       # Code emission
│       ├── rom.cpp             # ROM loading
│       └── batch.cpp           # Multi-ROM compilation
│
├── runtime/                    # Runtime library (libchip8rt)
│   ├── include/chip8rt/
│   │   ├── context.h           # CPU state structure
│   │   ├── instructions.h      # Instruction helper macros
│   │   ├── runtime.h           # Main runtime API
│   │   ├── platform.h          # Platform abstraction
│   │   ├── settings.h          # Configuration
│   │   └── menu.h              # In-game menu
│   └── src/
│       ├── context.c           # State management
│       ├── instructions.c      # Instruction implementations
│       ├── runtime.c           # Main loop
│       ├── platform_sdl.c      # SDL2 backend
│       ├── font.c              # Built-in font data
│       └── settings.c          # Config persistence
│
├── external/                   # Third-party libraries
│   └── imgui/                  # Dear ImGui for debug UI
│
├── docs/                       # Documentation
│   ├── ARCHITECTURE.md
│   └── ROADMAP.md
│
└── build/                      # Generated output
    └── <rom>_output/           # Per-ROM output directories
        ├── <rom>.c             # Generated C code
        ├── <rom>.h             # Generated header
        ├── rom_data.c          # Embedded ROM data
        ├── main.c              # Entry point
        └── CMakeLists.txt      # Build configuration
```

### Two Main Components

| Component | Purpose | Language |
|-----------|---------|----------|
| **chip8recomp** | Recompiler tool - converts ROM to C | C++20 |
| **libchip8rt** | Runtime library - platform abstraction | C11 |

---

## Key Components Deep Dive

### 1. Instruction Decoder (`decoder.h/cpp`)

The decoder transforms raw bytes into structured instruction representations.

#### CHIP-8 Instruction Structure

```cpp
struct Instruction {
    uint16_t address;           // Address in ROM
    uint16_t opcode;            // Raw 2-byte opcode
    InstructionType type;       // Decoded type enum
    
    // Operands
    uint8_t x;                  // Register X (nibble 2)
    uint8_t y;                  // Register Y (nibble 3)
    uint8_t n;                  // 4-bit immediate
    uint8_t nn;                 // 8-bit immediate
    uint16_t nnn;               // 12-bit address
    
    // Control flow flags
    bool is_jump;               // Unconditional jump
    bool is_branch;             // Conditional branch
    bool is_call;               // Subroutine call
    bool is_return;             // Subroutine return
    bool is_terminator;         // Ends a basic block
};
```

#### Instruction Categories

```cpp
enum class InstructionType {
    // System
    CLS, RET, SYS,
    
    // Control flow
    JP, CALL, JP_V0,
    
    // Conditional skips
    SE_VX_NN, SNE_VX_NN, SE_VX_VY, SNE_VX_VY, SKP, SKNP,
    
    // Loads
    LD_VX_NN, LD_VX_VY, LD_I_NNN, LD_VX_DT, LD_VX_K,
    LD_DT_VX, LD_ST_VX, LD_F_VX, LD_B_VX, LD_I_VX, LD_VX_I,
    
    // Arithmetic
    ADD_VX_NN, ADD_VX_VY, SUB_VX_VY, SUBN_VX_VY, ADD_I_VX,
    
    // Bitwise
    OR_VX_VY, AND_VX_VY, XOR_VX_VY, SHR_VX, SHL_VX,
    
    // Other
    RND, DRW,
    
    UNKNOWN
};
```

#### Decoding Process

```cpp
Instruction decode_opcode(uint16_t opcode, uint16_t address) {
    Instruction instr{};
    instr.address = address;
    instr.opcode = opcode;
    
    // Extract common fields
    instr.x = (opcode & 0x0F00) >> 8;
    instr.y = (opcode & 0x00F0) >> 4;
    instr.n = opcode & 0x000F;
    instr.nn = opcode & 0x00FF;
    instr.nnn = opcode & 0x0FFF;
    
    // Decode by top nibble
    uint8_t op = (opcode & 0xF000) >> 12;
    
    switch (op) {
        case 0x0:
            if (opcode == 0x00E0) instr.type = InstructionType::CLS;
            else if (opcode == 0x00EE) {
                instr.type = InstructionType::RET;
                instr.is_return = true;
                instr.is_terminator = true;
            }
            break;
        case 0x1:  // JP addr
            instr.type = InstructionType::JP;
            instr.is_jump = true;
            instr.is_terminator = true;
            break;
        // ... etc
    }
    return instr;
}
```

### 2. Control Flow Analyzer (`analyzer.h/cpp`)

The analyzer builds a control flow graph (CFG) to identify:
- **Basic blocks**: Sequences without branches
- **Functions**: CALL targets
- **Labels**: Jump targets
- **Reachability**: Which code is actually executed

#### Analysis Result Structure

```cpp
struct AnalysisResult {
    std::vector<Instruction> instructions;
    std::map<uint16_t, BasicBlock> blocks;
    std::map<uint16_t, Function> functions;
    std::set<uint16_t> label_addresses;
    std::set<uint16_t> call_targets;
    std::set<uint16_t> computed_jump_bases;
    uint16_t entry_point = 0x200;
};

struct BasicBlock {
    uint16_t start_address;
    uint16_t end_address;
    std::vector<uint16_t> instruction_indices;
    std::vector<uint16_t> successors;
    std::vector<uint16_t> predecessors;
    bool is_function_entry;
    bool is_reachable;
};

struct Function {
    std::string name;
    uint16_t entry_address;
    std::vector<uint16_t> block_addresses;
};
```

#### Analysis Algorithm

```
1. PASS 1: Identify targets
   - Find all JP targets → label_addresses
   - Find all CALL targets → call_targets, functions
   - Find all skip instruction targets → labels

2. PASS 2: Build basic blocks
   - Start new block at each label/call target
   - End block at terminators (JP, RET, conditional)

3. PASS 3: Build predecessor lists
   - For each block's successors, add to predecessor list

4. PASS 4: Reachability analysis (BFS from entry)
   - Mark blocks reachable from entry point
   - Mark all call targets as reachable

5. PASS 5: Assign blocks to functions
   - Each call target starts a function
   - BFS to find all blocks in function
```

### 3. Code Generator (`generator.h/cpp`)

The generator emits C code for each instruction.

#### Generator Options

```cpp
struct GeneratorOptions {
    std::string output_prefix = "rom";
    std::filesystem::path output_dir = ".";
    
    bool emit_comments = true;           // Disassembly comments
    bool single_function_mode = false;   // All code in one function
    bool use_prefixed_symbols = false;   // For batch/multi-ROM mode
    
    // Quirk modes for CHIP-8 variants
    bool quirk_shift_uses_vy = false;
    bool quirk_load_store_inc_i = true;
    bool quirk_vf_reset = true;
    
    bool embed_rom_data = true;
    bool debug_mode = false;
};
```

#### Translation Rules

Each instruction maps to C code:

| Instruction | Generated C Code |
|-------------|------------------|
| `CLS` | `chip8_clear_screen(ctx);` |
| `RET` | `return;` |
| `JP addr` | `goto label_0xNNN;` |
| `CALL addr` | `func_0xNNN(ctx);` |
| `SE Vx, NN` | `if (ctx->V[x] == NN) goto skip_label;` |
| `LD Vx, NN` | `ctx->V[x] = NN;` |
| `ADD Vx, Vy` | `CHIP8_ADD_VX_VY(ctx, x, y);` |
| `DRW Vx, Vy, N` | `chip8_draw_sprite(ctx, x, y, N);` |

#### Generated Code Example

```c
// Input: 6A02 6B0C A2EA DAB6
// Output:

void func_0x200(Chip8Context* ctx) {
    /* 0x200: 6A02 - LD VA, 0x02 */
    ctx->V[0xA] = 0x02;
    
    /* 0x202: 6B0C - LD VB, 0x0C */
    ctx->V[0xB] = 0x0C;
    
    /* 0x204: A2EA - LD I, 0x2EA */
    ctx->I = 0x2EA;
    
    /* 0x206: DAB6 - DRW VA, VB, 6 */
    chip8_draw_sprite(ctx, 0xA, 0xB, 6);
}
```

#### Two Compilation Modes

**Normal Mode**: Separate functions per CALL target
```c
void func_0x200(Chip8Context* ctx) {
    // ...
    func_0x300(ctx);  // CALL 0x300
    // ...
}

void func_0x300(Chip8Context* ctx) {
    // ...
    return;  // RET
}
```

**Single-Function Mode**: All code in one function with computed dispatch
```c
void rom_main(Chip8Context* ctx) {
    // CALL becomes: push return address, goto target
    ctx->stack[ctx->SP++] = 0x204;
    goto label_0x300;
    
label_0x204:
    // ... continue after call ...
    
label_0x300:
    // Subroutine code
    
    // RET becomes: switch on return address
    {
        uint16_t ret_addr = ctx->stack[--ctx->SP];
        switch (ret_addr) {
            case 0x204: goto label_0x204;
            // ... other return points
        }
    }
}
```

### 4. Runtime Library (`libchip8rt`)

The runtime provides the execution environment for generated code.

#### CPU Context Structure

```c
typedef struct Chip8Context {
    // Registers
    uint8_t  V[16];              // V0-VF general purpose
    uint16_t I;                  // Index register
    uint16_t PC;                 // Program counter
    uint8_t  SP;                 // Stack pointer
    
    // Timers (60Hz)
    uint8_t delay_timer;
    uint8_t sound_timer;
    
    // Memory
    uint8_t memory[4096];        // 4KB RAM
    uint16_t stack[16];          // Call stack
    
    // Display
    uint8_t display[64 * 32];    // Framebuffer
    bool display_dirty;
    
    // Input
    bool keys[16];               // Hex keypad state
    
    // Runtime control
    bool running;
    bool waiting_for_key;
    int cycles_remaining;        // For yielding
    uint16_t resume_pc;          // Resume after yield
    bool should_yield;
    
    // Platform data
    void* platform_data;
} Chip8Context;
```

#### Instruction Helper Macros

```c
// Arithmetic with flag handling
#define CHIP8_ADD_VX_VY(ctx, x, y) do { \
    uint16_t sum = (uint16_t)(ctx)->V[(x)] + (uint16_t)(ctx)->V[(y)]; \
    (ctx)->V[(x)] = (uint8_t)(sum & 0xFF); \
    (ctx)->V[0xF] = (sum > 255) ? 1 : 0; \
} while(0)

#define CHIP8_SUB_VX_VY(ctx, x, y) do { \
    uint8_t vx = (ctx)->V[(x)]; \
    uint8_t vy = (ctx)->V[(y)]; \
    (ctx)->V[(x)] = vx - vy; \
    (ctx)->V[0xF] = (vx >= vy) ? 1 : 0; \
} while(0)
```

#### Runtime Functions

```c
void chip8_clear_screen(Chip8Context* ctx);
void chip8_draw_sprite(Chip8Context* ctx, uint8_t vx, uint8_t vy, uint8_t height);
bool chip8_key_pressed(Chip8Context* ctx, uint8_t key);
void chip8_wait_key(Chip8Context* ctx, uint8_t reg);
void chip8_store_bcd(Chip8Context* ctx, uint8_t x);
void chip8_store_registers(Chip8Context* ctx, uint8_t x, bool inc_i);
void chip8_load_registers(Chip8Context* ctx, uint8_t x, bool inc_i);
uint8_t chip8_random_byte(void);
```

#### Platform Abstraction Layer

```c
typedef struct Chip8Platform {
    const char* name;
    
    // Lifecycle
    bool (*init)(Chip8Context* ctx, const char* title, int scale);
    void (*shutdown)(Chip8Context* ctx);
    
    // Video
    void (*render)(Chip8Context* ctx);
    
    // Audio
    void (*beep_start)(Chip8Context* ctx);
    void (*beep_stop)(Chip8Context* ctx);
    
    // Input
    void (*poll_events)(Chip8Context* ctx);
    bool (*should_quit)(Chip8Context* ctx);
    
    // Timing
    uint64_t (*get_time_us)(void);
    void (*sleep_us)(uint64_t microseconds);
} Chip8Platform;
```

---

## Recompilation Pipeline

### CHIP-8 Pipeline (Simple)

```
┌─────────────┐
│  Load ROM   │  → Read binary file, validate size
└──────┬──────┘
       ▼
┌─────────────┐
│   Decode    │  → Convert bytes to Instruction structs
└──────┬──────┘
       ▼
┌─────────────┐
│  Analyze    │  → Build CFG, find functions/labels
└──────┬──────┘
       ▼
┌─────────────┐
│  Generate   │  → Emit C code for each instruction
└──────┬──────┘
       ▼
┌─────────────┐
│   Write     │  → Output .c, .h, CMakeLists.txt
└─────────────┘
```

### GameBoy Pipeline (Enhanced with IR Layer)

For GameBoy, we add an **Intermediate Representation (IR)** layer between analysis and code generation. This enables optimization passes and future backend support (e.g., LLVM).

```
┌─────────────┐     ┌─────────────┐     ┌─────────────┐     ┌─────────────┐     ┌─────────────┐     ┌─────────────┐
│  Load ROM   │────▶│   Decode    │────▶│  Analyze    │────▶│  IR Build   │────▶│  Optimize   │────▶│   Emit      │
│  + Banks    │     │  (SM83)     │     │  CFG+Banks  │     │  (Lower)    │     │  (Optional) │     │  (Backend)  │
└─────────────┘     └─────────────┘     └─────────────┘     └─────────────┘     └─────────────┘     └─────────────┘
      │                   │                   │                   │                   │                   │
      ▼                   ▼                   ▼                   ▼                   ▼                   ▼
 Parse header        ~500 opcodes       Bank-aware          Semantic IR       Const prop,         C code (MVP)
 Detect MBC          CB-prefix          Cross-bank          Flag effects      Dead code           LLVM (Future)
 Map banks           Variable len       call graph          Memory ops        Bank inline
```

### Main Entry Point

```cpp
int main(int argc, char* argv[]) {
    // 1. Parse arguments
    // 2. Load ROM
    auto rom = load_rom(rom_path);
    validate_rom(*rom, error_msg);
    
    // 3. Decode
    auto instructions = decode_rom(rom->bytes(), rom->size());
    
    // 4. Analyze
    auto analysis = analyze(instructions);
    
    // 5. Generate
    auto output = generate(analysis, rom->bytes(), rom->size(), options);
    
    // 6. Write
    write_output(output, output_dir);
}
```

---

## Code Generation Strategy

### Handling Control Flow Challenges

#### 1. Backward Jumps (Loops)

Problem: Infinite loops would hang the program.

Solution: Cooperative yielding

```c
// 0x21E: 121A - JP 0x21A (backward jump)
if (--ctx->cycles_remaining <= 0) {
    ctx->resume_pc = 0x21A;
    ctx->should_yield = true;
    return;
}
goto label_0x21A;
```

#### 2. Computed Jumps (JP V0, addr)

Problem: Target depends on runtime value.

Solution: Switch statement dispatch

```c
// JP V0, 0x300
{
    uint16_t target = 0x300 + ctx->V[0];
    switch (target) {
        case 0x300: goto label_0x300;
        case 0x302: goto label_0x302;
        case 0x304: goto label_0x304;
        default: chip8_panic("Invalid jump target", target);
    }
}
```

#### 3. Data Mixed with Code

Problem: Some ROMs have sprite data in code sections.

Solution: Reachability-based generation (only emit reachable code)

```cpp
// Only generate code for addresses reachable from entry point
std::set<uint16_t> reachable;
std::queue<uint16_t> worklist;
worklist.push(entry_point);

while (!worklist.empty()) {
    uint16_t addr = worklist.front();
    worklist.pop();
    
    if (reachable.count(addr)) continue;
    reachable.insert(addr);
    
    // Add successors based on instruction type
    // JP → target, CALL → target + return, etc.
}
```

---

## GameBoy vs CHIP-8 Comparison

### Architecture Differences

| Aspect | CHIP-8 | GameBoy (DMG) |
|--------|--------|---------------|
| **CPU** | Virtual (interpreted) | Sharp LR35902 (Z80-like) |
| **Word Size** | 16-bit opcodes | 8-bit with 16-bit ops |
| **Instruction Count** | 35 | ~500 (with CB prefix) |
| **Instruction Size** | Fixed 2 bytes | Variable 1-3 bytes |
| **Memory** | 4KB | 64KB addressable |
| **RAM** | 4KB total | 8KB work RAM + 8KB VRAM |
| **ROM Size** | 3.5KB max | 32KB - 8MB (bank switched) |
| **Display** | 64×32 monochrome | 160×144, 4 colors/palette |
| **Sprites** | 8×N, XOR-drawn | 8×8 or 8×16, OAM-based |
| **Registers** | V0-VF, I, PC, SP | A, B, C, D, E, H, L, F, SP, PC |
| **Timers** | Delay, Sound (60Hz) | DIV, TIMA, TMA, TAC |
| **Audio** | Single beep | 4 channels (2 pulse, wave, noise) |
| **Input** | 16-key hex pad | D-pad + A, B, Start, Select |
| **Interrupts** | None | VBlank, LCD STAT, Timer, Serial, Joypad |
| **Banking** | None | ROM/RAM bank switching |

### Key Challenges for GameBoy

1. **Variable-Length Instructions**: 1-3 bytes, CB-prefixed extended opcodes
2. **Memory-Mapped I/O**: Hardware registers at 0xFF00-0xFF7F
3. **Bank Switching**: Must track ROM/RAM bank state
4. **Interrupts**: Need to handle interrupt vectors
5. **PPU Timing**: Scanline-accurate rendering required for some games
6. **DMA**: OAM DMA transfers
7. **Self-Modifying Code**: More common than CHIP-8

---

## GameBoy Recompiler Architecture Proposal

### Proposed Directory Structure

```
gameboy-recompiled/
├── recompiler/                    # gbrecomp tool
│   ├── include/recompiler/
│   │   ├── decoder.h              # SM83 instruction decoder
│   │   ├── analyzer.h             # Control flow analyzer
│   │   ├── bank_tracker.h         # Bank switching analysis
│   │   ├── ir/
│   │   │   ├── ir.h               # IR opcodes and instruction types
│   │   │   ├── ir_builder.h       # Build IR from decoded instructions
│   │   │   └── ir_optimizer.h     # Optimization passes (optional)
│   │   ├── codegen/
│   │   │   ├── emitter.h          # Abstract emitter interface
│   │   │   ├── c_emitter.h        # C code backend (MVP)
│   │   │   └── llvm_emitter.h     # LLVM backend (Future)
│   │   ├── generator.h            # High-level generation orchestration
│   │   ├── rom.h                  # ROM loader (with MBC detection)
│   │   └── symbol_table.h         # Symbol/label management
│   └── src/
│       ├── main.cpp
│       ├── decoder.cpp            # ~500 opcodes
│       ├── analyzer.cpp
│       ├── bank_tracker.cpp       # ROM bank tracking
│       ├── ir/
│       │   ├── ir_builder.cpp
│       │   └── ir_optimizer.cpp
│       ├── codegen/
│       │   └── c_emitter.cpp      # C code generation
│       ├── generator.cpp
│       ├── rom.cpp                # Header parsing, MBC type
│       └── symbol_table.cpp
│
├── runtime/                       # libgbrt
│   ├── include/gbrt/
│   │   ├── cpu.h                  # CPU state
│   │   ├── memory.h               # Memory bus
│   │   ├── ppu.h                  # Picture Processing Unit
│   │   ├── apu.h                  # Audio Processing Unit
│   │   ├── timer.h                # Timer subsystem
│   │   ├── joypad.h               # Input handling
│   │   ├── interrupts.h           # Interrupt controller
│   │   ├── mbc.h                  # Memory Bank Controller interface
│   │   ├── instructions.h         # Instruction macros
│   │   ├── runtime.h              # Main API
│   │   └── platform.h             # Platform abstraction
│   └── src/
│       ├── cpu.c
│       ├── memory.c
│       ├── ppu.c
│       ├── apu.c
│       ├── timer.c
│       ├── joypad.c
│       ├── interrupts.c
│       ├── mbc/
│       │   ├── mbc.c              # MBC dispatch
│       │   ├── mbc_none.c         # No MBC (32KB ROMs)
│       │   ├── mbc1.c
│       │   ├── mbc2.c
│       │   ├── mbc3.c             # With RTC
│       │   └── mbc5.c
│       ├── runtime.c
│       └── platform_sdl.c
│
├── templates/                     # Code generation templates (optional)
│   ├── function.c.tmpl
│   ├── instruction.c.tmpl
│   └── main.c.tmpl
│
├── tests/                         # Test suite
│   ├── decoder_tests.cpp
│   ├── ir_tests.cpp
│   └── roms/                      # Test ROMs
│
└── docs/
    ├── ARCHITECTURE.md
    ├── SM83_INSTRUCTION_SET.md
    └── IR_REFERENCE.md
```

### GameBoy CPU Context

```c
typedef struct GBContext {
    // Registers (directly accessible for performance)
    union {
        struct { uint8_t F, A; };  // AF
        uint16_t AF;
    };
    union {
        struct { uint8_t C, B; };  // BC
        uint16_t BC;
    };
    union {
        struct { uint8_t E, D; };  // DE
        uint16_t DE;
    };
    union {
        struct { uint8_t L, H; };  // HL
        uint16_t HL;
    };
    uint16_t SP;
    uint16_t PC;
    
    // Flags (extracted from F for fast access)
    bool flag_z;  // Zero
    bool flag_n;  // Subtract
    bool flag_h;  // Half-carry
    bool flag_c;  // Carry
    
    // Interrupt state
    bool IME;                    // Interrupt Master Enable
    bool ime_scheduled;          // EI enables after next instruction
    uint8_t IE;                  // Interrupt Enable register
    uint8_t IF;                  // Interrupt Flag register
    bool halted;
    bool stopped;
    
    // Memory
    uint8_t* rom;                // Pointer to ROM data
    uint8_t wram[0x2000];        // 8KB Work RAM
    uint8_t hram[0x7F];          // High RAM (127 bytes)
    uint8_t vram[0x2000];        // 8KB Video RAM
    uint8_t oam[0xA0];           // Object Attribute Memory
    uint8_t* eram;               // External RAM (cartridge)
    
    // Memory banking
    uint8_t rom_bank;            // Current ROM bank (1-255)
    uint8_t ram_bank;            // Current RAM bank
    bool ram_enabled;
    uint8_t mbc_type;
    
    // PPU state
    uint8_t lcdc, stat, scy, scx, ly, lyc, dma, bgp, obp0, obp1, wy, wx;
    uint8_t framebuffer[160 * 144];
    bool frame_ready;
    uint32_t ppu_dots;
    
    // Timer state
    uint8_t div, tima, tma, tac;
    uint16_t div_counter;
    uint16_t tima_counter;
    
    // APU state
    // ... (4 channels)
    
    // Joypad
    uint8_t joyp;
    uint8_t buttons;             // Current button state
    
    // Timing
    uint64_t cycles;             // Total cycles executed
    int cycles_remaining;        // For yielding
    
    // Platform
    void* platform_data;
} GBContext;
```

### GameBoy Instruction Decoder

```cpp
enum class GBInstructionType {
    // 8-bit loads
    LD_R_R, LD_R_N, LD_R_HL, LD_HL_R, LD_HL_N,
    LD_A_BC, LD_A_DE, LD_BC_A, LD_DE_A,
    LD_A_NN, LD_NN_A, LDH_A_N, LDH_N_A, LDH_A_C, LDH_C_A,
    LD_A_HLI, LD_A_HLD, LD_HLI_A, LD_HLD_A,
    
    // 16-bit loads
    LD_RR_NN, LD_SP_HL, LD_NN_SP, PUSH, POP,
    
    // 8-bit arithmetic
    ADD_R, ADD_N, ADD_HL, ADC_R, ADC_N, ADC_HL,
    SUB_R, SUB_N, SUB_HL, SBC_R, SBC_N, SBC_HL,
    AND_R, AND_N, AND_HL, OR_R, OR_N, OR_HL,
    XOR_R, XOR_N, XOR_HL, CP_R, CP_N, CP_HL,
    INC_R, DEC_R, INC_HL, DEC_HL,
    
    // 16-bit arithmetic
    ADD_HL_RR, ADD_SP_N, INC_RR, DEC_RR,
    
    // Bit operations (CB-prefixed)
    RLC, RRC, RL, RR, SLA, SRA, SWAP, SRL,
    BIT, RES, SET,
    
    // Control flow
    JP, JP_CC, JR, JR_CC, CALL, CALL_CC, RET, RET_CC, RETI, RST,
    
    // Misc
    NOP, HALT, STOP, DI, EI, CCF, SCF, DAA, CPL,
    
    // Rotates (non-CB)
    RLCA, RRCA, RLA, RRA,
    
    INVALID
};

struct GBInstruction {
    uint16_t address;
    uint8_t opcode;
    uint8_t cb_opcode;           // For CB-prefixed
    bool is_cb_prefixed;
    GBInstructionType type;
    
    uint8_t length;              // 1, 2, or 3 bytes
    uint8_t cycles;              // Base cycle count
    uint8_t cycles_branch;       // Cycles if branch taken
    
    // Operands
    uint8_t reg1, reg2;          // Register indices
    uint8_t imm8;                // 8-bit immediate
    uint16_t imm16;              // 16-bit immediate
    int8_t offset;               // Signed offset for JR
    uint8_t bit;                 // Bit index for BIT/SET/RES
    uint8_t condition;           // Condition code for jumps
    
    // Flags
    bool is_jump;
    bool is_call;
    bool is_return;
    bool is_conditional;
    bool reads_memory;
    bool writes_memory;
    bool is_terminator;
};
```

### Code Generation for GameBoy

#### Simple Instructions

```c
// LD A, B
ctx->A = ctx->B;

// LD A, (HL)
ctx->A = gb_read_byte(ctx, ctx->HL);

// ADD A, B
{
    uint16_t result = ctx->A + ctx->B;
    ctx->flag_h = ((ctx->A & 0xF) + (ctx->B & 0xF)) > 0xF;
    ctx->flag_c = result > 0xFF;
    ctx->A = (uint8_t)result;
    ctx->flag_z = ctx->A == 0;
    ctx->flag_n = false;
}

// JP NZ, 0x1234
if (!ctx->flag_z) {
    ctx->cycles += 4;  // Extra cycles for taken branch
    goto label_0x1234;
}
```

#### Memory Access

```c
// All memory access goes through bus functions
uint8_t gb_read_byte(GBContext* ctx, uint16_t addr);
void gb_write_byte(GBContext* ctx, uint16_t addr, uint8_t value);

// These handle:
// - ROM banking (0x0000-0x7FFF)
// - VRAM (0x8000-0x9FFF)
// - External RAM (0xA000-0xBFFF)
// - Work RAM (0xC000-0xDFFF)
// - OAM (0xFE00-0xFE9F)
// - I/O registers (0xFF00-0xFF7F)
// - High RAM (0xFF80-0xFFFE)
```

#### Bank Switching Challenge

```c
// When LD (0x2000), A is executed, ROM bank changes
// This is a major challenge for static recompilation

// Option 1: Function table per bank
void (*bank_functions[256][MAX_FUNCS])(GBContext* ctx);

// Option 2: Unified address space with bank prefix
void func_bank01_0x4000(GBContext* ctx);
void func_bank02_0x4000(GBContext* ctx);

// Option 3: Runtime dispatch
void execute_at(GBContext* ctx, uint8_t bank, uint16_t addr) {
    Chip8FuncPtr func = lookup_function(bank, addr);
    if (func) func(ctx);
    else gb_panic("Unknown code location");
}
```

#### Interrupt Handling

```c
void gb_check_interrupts(GBContext* ctx) {
    if (!ctx->IME) return;
    
    uint8_t pending = ctx->IE & ctx->IF;
    if (!pending) return;
    
    ctx->IME = false;
    ctx->halted = false;
    
    // Priority: VBlank > LCD > Timer > Serial > Joypad
    uint16_t vector;
    if (pending & 0x01) { vector = 0x40; ctx->IF &= ~0x01; }      // VBlank
    else if (pending & 0x02) { vector = 0x48; ctx->IF &= ~0x02; } // LCD STAT
    else if (pending & 0x04) { vector = 0x50; ctx->IF &= ~0x04; } // Timer
    else if (pending & 0x08) { vector = 0x58; ctx->IF &= ~0x08; } // Serial
    else if (pending & 0x10) { vector = 0x60; ctx->IF &= ~0x10; } // Joypad
    
    // Push PC and jump to vector
    ctx->SP -= 2;
    gb_write_word(ctx, ctx->SP, ctx->PC);
    // Jump to interrupt handler
    goto_vector(ctx, vector);
}
```

---

## Intermediate Representation (IR) Design

The IR layer decouples instruction semantics from code generation, enabling future optimizations and alternative backends.

### IR Opcodes

```cpp
enum class IROpcode {
    // === Data Movement ===
    MOV_REG_REG,        // dst = src
    MOV_REG_IMM8,       // dst = imm8
    MOV_REG_IMM16,      // dst16 = imm16
    LOAD8,              // dst = mem[addr]
    LOAD16,             // dst16 = mem16[addr]
    STORE8,             // mem[addr] = src
    STORE16,            // mem16[addr] = src16
    PUSH16,             // SP -= 2; mem16[SP] = src16
    POP16,              // dst16 = mem16[SP]; SP += 2
    
    // === ALU Operations ===
    ADD8,               // A = A + src, set flags
    ADC8,               // A = A + src + C, set flags
    SUB8,               // A = A - src, set flags
    SBC8,               // A = A - src - C, set flags
    AND8,               // A = A & src, set flags
    OR8,                // A = A | src, set flags
    XOR8,               // A = A ^ src, set flags
    CP8,                // compare A - src, set flags only
    INC8,               // dst++, set flags (no carry)
    DEC8,               // dst--, set flags (no carry)
    ADD16,              // HL = HL + rr, set H/C flags
    ADD_SP_IMM8,        // SP = SP + signed_imm8
    INC16,              // rr++, no flags
    DEC16,              // rr--, no flags
    
    // === Bit Operations ===
    RLC,                // Rotate left through carry
    RRC,                // Rotate right through carry
    RL,                 // Rotate left
    RR,                 // Rotate right
    SLA,                // Shift left arithmetic
    SRA,                // Shift right arithmetic (preserve sign)
    SRL,                // Shift right logical
    SWAP,               // Swap nibbles
    BIT,                // Test bit, set Z flag
    SET,                // Set bit
    RES,                // Reset bit
    
    // === Control Flow ===
    JUMP,               // Unconditional jump
    JUMP_CC,            // Conditional jump
    JR,                 // Relative jump
    JR_CC,              // Conditional relative jump
    CALL,               // Push PC, jump
    CALL_CC,            // Conditional call
    RET,                // Pop PC
    RET_CC,             // Conditional return
    RETI,               // Return and enable interrupts
    RST,                // Call to fixed vector
    
    // === Special ===
    NOP,
    HALT,
    STOP,
    DI,                 // Disable interrupts
    EI,                 // Enable interrupts
    DAA,                // Decimal adjust A
    CPL,                // Complement A
    CCF,                // Complement carry flag
    SCF,                // Set carry flag
    
    // === Memory-Mapped I/O ===
    IO_READ,            // Read from 0xFF00 + offset
    IO_WRITE,           // Write to 0xFF00 + offset
    
    // === Bank Switching (pseudo-ops) ===
    BANK_SWITCH,        // Hint: bank may change
    CROSS_BANK_CALL,    // Call to different bank
    
    // === Meta ===
    LABEL,              // Label definition
    COMMENT,            // Debugging info
};
```

### IR Instruction Structure

```cpp
// Operand types
enum class IROperandType {
    NONE,
    REG8,           // A, B, C, D, E, H, L
    REG16,          // AF, BC, DE, HL, SP
    IMM8,           // 8-bit immediate
    IMM16,          // 16-bit immediate
    OFFSET,         // Signed 8-bit offset (for JR)
    ADDR,           // 16-bit address
    COND,           // Condition code (Z, NZ, C, NC)
    BIT_IDX,        // Bit index 0-7
    BANK,           // ROM bank number
    MEM_REG16,      // Memory at [reg16] e.g., [HL]
    MEM_IMM16,      // Memory at [imm16]
    IO_OFFSET,      // 0xFF00 + offset
    IO_C,           // 0xFF00 + C register
};

struct IROperand {
    IROperandType type;
    union {
        uint8_t reg8;       // Register index (0=A, 1=B, etc.)
        uint8_t reg16;      // Register pair index
        uint8_t imm8;
        uint16_t imm16;
        int8_t offset;
        uint8_t bit_idx;
        uint8_t bank;
        uint8_t condition;
    };
};

struct IRInstruction {
    IROpcode opcode;
    IROperand dst;
    IROperand src;
    IROperand extra;        // For 3-operand ops (e.g., BIT n, r)
    
    // Source location
    uint8_t source_bank;
    uint16_t source_address;
    
    // Cycle cost
    uint8_t cycles;
    uint8_t cycles_branch_taken;  // For conditional ops
    
    // Flag effects (what this instruction modifies)
    struct {
        bool affects_z : 1;
        bool affects_n : 1;
        bool affects_h : 1;
        bool affects_c : 1;
        bool z_value : 1;   // For fixed flag values (e.g., AND always sets N=0)
        bool n_value : 1;
        bool h_value : 1;
        bool c_value : 1;
    } flags;
};
```

### IR Program Structure

```cpp
struct IRBasicBlock {
    uint32_t id;
    std::string label;
    std::vector<IRInstruction> instructions;
    std::vector<uint32_t> successors;
    std::vector<uint32_t> predecessors;
    
    // Bank info
    uint8_t bank;               // ROM bank this block belongs to
    bool is_bank_entry;         // Entry point after bank switch
};

struct IRFunction {
    std::string name;
    uint8_t bank;
    uint16_t entry_address;
    std::vector<uint32_t> block_ids;
    bool is_interrupt_handler;
};

struct IRProgram {
    std::string rom_name;
    std::map<uint32_t, IRBasicBlock> blocks;
    std::map<std::string, IRFunction> functions;
    
    // Bank info
    uint8_t mbc_type;
    uint16_t rom_banks;
    
    // Entry points
    uint16_t entry_point;           // Usually 0x100
    std::vector<uint16_t> vectors;  // Interrupt vectors
};
```

---

## Code Generation Backend Abstraction

The backend abstraction allows swapping between C output (MVP) and LLVM (future).

### Emitter Interface

```cpp
// Abstract base class for code emission
class CodeEmitter {
public:
    virtual ~CodeEmitter() = default;
    
    // Program structure
    virtual void begin_program(const std::string& name) = 0;
    virtual void end_program() = 0;
    virtual void begin_function(const std::string& name, uint8_t bank, uint16_t addr) = 0;
    virtual void end_function() = 0;
    virtual void emit_label(const std::string& label) = 0;
    
    // Data movement
    virtual void emit_mov_reg_reg(uint8_t dst, uint8_t src) = 0;
    virtual void emit_mov_reg_imm8(uint8_t dst, uint8_t imm) = 0;
    virtual void emit_load8(uint8_t dst, uint16_t addr) = 0;
    virtual void emit_load8_reg(uint8_t dst, uint8_t addr_reg) = 0;
    virtual void emit_store8(uint16_t addr, uint8_t src) = 0;
    virtual void emit_store8_reg(uint8_t addr_reg, uint8_t src) = 0;
    
    // ALU
    virtual void emit_add8(uint8_t src) = 0;  // A += src
    virtual void emit_sub8(uint8_t src) = 0;  // A -= src
    virtual void emit_and8(uint8_t src) = 0;
    virtual void emit_or8(uint8_t src) = 0;
    virtual void emit_xor8(uint8_t src) = 0;
    virtual void emit_cp8(uint8_t src) = 0;
    virtual void emit_inc8(uint8_t reg) = 0;
    virtual void emit_dec8(uint8_t reg) = 0;
    
    // Control flow
    virtual void emit_jump(const std::string& label) = 0;
    virtual void emit_jump_cc(uint8_t cc, const std::string& label) = 0;
    virtual void emit_call(const std::string& func_name) = 0;
    virtual void emit_call_cc(uint8_t cc, const std::string& func_name) = 0;
    virtual void emit_ret() = 0;
    virtual void emit_ret_cc(uint8_t cc) = 0;
    
    // Bank switching
    virtual void emit_bank_call(uint8_t bank, const std::string& func_name) = 0;
    
    // Special
    virtual void emit_halt() = 0;
    virtual void emit_di() = 0;
    virtual void emit_ei() = 0;
    
    // Memory/IO
    virtual void emit_io_read(uint8_t offset) = 0;
    virtual void emit_io_write(uint8_t offset) = 0;
    
    // Comment/debug
    virtual void emit_comment(const std::string& comment) = 0;
};
```

### C Backend (MVP)

```cpp
class CEmitter : public CodeEmitter {
private:
    std::ostream& out;
    GeneratorOptions options;
    int indent_level = 0;
    
    void emit_indent() {
        for (int i = 0; i < indent_level; i++) out << "    ";
    }

public:
    CEmitter(std::ostream& output, const GeneratorOptions& opts)
        : out(output), options(opts) {}
    
    void begin_function(const std::string& name, uint8_t bank, uint16_t addr) override {
        out << "\n/* Bank " << (int)bank << " @ 0x" << std::hex << addr << " */\n";
        out << "void " << name << "(GBContext* ctx) {\n";
        indent_level = 1;
    }
    
    void end_function() override {
        indent_level = 0;
        out << "}\n";
    }
    
    void emit_mov_reg_reg(uint8_t dst, uint8_t src) override {
        static const char* reg_names[] = {"A", "B", "C", "D", "E", "H", "L"};
        emit_indent();
        out << "ctx->" << reg_names[dst] << " = ctx->" << reg_names[src] << ";\n";
    }
    
    void emit_add8(uint8_t src) override {
        emit_indent();
        out << "GB_ADD_A(ctx, " << get_operand(src) << ");\n";
    }
    
    void emit_jump(const std::string& label) override {
        emit_indent();
        out << "goto " << label << ";\n";
    }
    
    void emit_jump_cc(uint8_t cc, const std::string& label) override {
        static const char* cond_code[] = {"!ctx->flag_z", "ctx->flag_z", 
                                          "!ctx->flag_c", "ctx->flag_c"};
        emit_indent();
        out << "if (" << cond_code[cc] << ") { ctx->cycles += 4; goto " << label << "; }\n";
    }
    
    // ... remaining implementations
};
```

### Future: LLVM Backend

```cpp
// For future implementation - not part of MVP
class LLVMEmitter : public CodeEmitter {
    // Uses LLVM C API or C++ API to generate LLVM IR
    // Enables:
    // - Direct compilation to native code
    // - Advanced optimizations
    // - Multiple target architectures
    // - JIT fallback for self-modifying code
};
```

---

## Bank-Aware Code Generation

Bank switching is the most significant complexity difference from CHIP-8.

### Bank Tracking During Analysis

```cpp
struct BankState {
    uint8_t rom_bank = 1;       // Current ROM bank (0 is fixed)
    uint8_t ram_bank = 0;       // Current RAM bank
    bool ram_enabled = false;
    bool is_known = true;       // False if bank state is uncertain
};

struct BankedAddress {
    uint8_t bank;
    uint16_t address;
    
    std::string to_label() const {
        std::ostringstream ss;
        ss << "bank" << std::setw(2) << std::setfill('0') << (int)bank
           << "_" << std::hex << std::setw(4) << address;
        return ss.str();
    }
};

// Track bank state at each instruction
class BankTracker {
    std::map<uint16_t, BankState> state_at_address;
    std::set<BankedAddress> known_code_locations;
    
public:
    // Called when analyzing a write to bank switch register
    void handle_bank_write(uint16_t addr, const AnalysisContext& ctx);
    
    // Get all possible banks for an address in the switchable region
    std::set<uint8_t> get_possible_banks(uint16_t addr);
    
    // Check if a call crosses banks
    bool is_cross_bank_call(uint8_t from_bank, uint16_t from_addr,
                            uint16_t target_addr);
};
```

### Generated Code for Bank Switching

```c
// Option 1: Per-bank function namespaces (MVP approach)
void bank01_func_4000(GBContext* ctx);
void bank02_func_4000(GBContext* ctx);
void bank03_func_4000(GBContext* ctx);

// Cross-bank call
void bank01_func_4100(GBContext* ctx) {
    // ... code ...
    
    // Switch to bank 2 and call
    ctx->rom_bank = 2;
    bank02_func_4000(ctx);
    
    // Note: bank may have changed during call!
    // For games with complex banking, may need runtime dispatch
}

// Option 2: Dispatch table (for complex cases)
typedef void (*BankFunc)(GBContext*);

// Lookup table: [bank][function_index] -> function pointer
extern BankFunc g_bank_functions[256][MAX_FUNCS_PER_BANK];

void dispatch_banked_call(GBContext* ctx, uint16_t addr) {
    uint8_t bank = ctx->rom_bank;
    BankFunc func = lookup_function(bank, addr);
    if (func) {
        func(ctx);
    } else {
        gb_panic("Unknown banked call", bank, addr);
    }
}
```

### Handling Unknown Bank State

```c
// When analysis can't determine the bank statically,
// generate runtime dispatch
void unknown_bank_call_4000(GBContext* ctx) {
    switch (ctx->rom_bank) {
        case 1: bank01_func_4000(ctx); break;
        case 2: bank02_func_4000(ctx); break;
        case 3: bank03_func_4000(ctx); break;
        // ... all possible banks ...
        default: gb_panic("Unexpected bank", ctx->rom_bank);
    }
}
```

---

## Implementation Roadmap

### Phase 1: Foundation (Weeks 1-4)

**Goal**: Minimal working recompiler for simple 32KB ROMs (no banking)

- [ ] Project setup (CMake, directory structure)
- [ ] ROM loader with header parsing
- [ ] MBC type detection
- [ ] SM83 instruction decoder (all ~500 opcodes)
- [ ] Basic IR builder (semantic representation)
- [ ] CPU context structure
- [ ] Memory bus implementation (no banking yet)
- [ ] C code emitter (basic implementation)
- [ ] Simple test ROM working (e.g., hello world)

**Deliverable**: Can recompile and run a 32KB no-MBC ROM

### Phase 2: Control Flow & Analysis (Weeks 5-7)

**Goal**: Proper function detection and control flow

- [ ] Control flow analyzer
- [ ] Jump/call target identification
- [ ] Basic block construction
- [ ] Function boundary detection
- [ ] Handle conditional jumps (JP cc, JR cc)
- [ ] RST vector handling
- [ ] Reachability analysis
- [ ] Single-function mode (fallback for complex cases)

**Deliverable**: Can handle ROMs with multiple functions and loops

### Phase 3: Bank Switching (Weeks 8-10)

**Goal**: Support for MBC1/MBC3/MBC5 games

- [ ] Bank tracker implementation
- [ ] MBC1 support (most common)
- [ ] MBC3 support (Pokémon)
- [ ] MBC5 support (later games)
- [ ] Cross-bank call detection
- [ ] Per-bank function generation
- [ ] Runtime bank dispatch for unknown states
- [ ] RAM banking support

**Deliverable**: Can recompile a banked ROM (e.g., Tetris DX)

### Phase 4: PPU (Weeks 11-14)

**Goal**: Visual output

- [ ] Background rendering
- [ ] Window rendering
- [ ] Sprite rendering (8x8 and 8x16)
- [ ] Scanline timing
- [ ] VBlank interrupt
- [ ] LCD STAT interrupt
- [ ] VRAM access timing (basic)
- [ ] Palette handling

**Deliverable**: Games display correctly

### Phase 5: Interrupts & Timing (Weeks 15-17)

**Goal**: Accurate timing and interrupt handling

- [ ] Full interrupt controller
- [ ] Timer implementation (DIV, TIMA, TMA, TAC)
- [ ] Timer interrupt
- [ ] Joypad input
- [ ] Joypad interrupt
- [ ] DMA transfers
- [ ] Cycle-accurate yielding

**Deliverable**: Timing-sensitive games work

### Phase 6: Audio (Weeks 18-20)

**Goal**: Sound output

- [ ] Channel 1 (Pulse with sweep)
- [ ] Channel 2 (Pulse)
- [ ] Channel 3 (Wave)
- [ ] Channel 4 (Noise)
- [ ] Audio mixing
- [ ] SDL audio backend

**Deliverable**: Games have sound

### Phase 7: Polish & Optimization (Weeks 21-24)

**Goal**: Production quality

- [ ] IR optimization passes (constant propagation, dead code elimination)
- [ ] Test ROM compatibility (Blargg's tests, Mooneye)
- [ ] Commercial game testing
- [ ] Debug overlay (ImGui)
- [ ] Performance profiling
- [ ] Save state support
- [ ] Save file support (battery-backed RAM)
- [ ] Documentation

**Deliverable**: Release-ready recompiler

### Future Enhancements (Post-MVP)

- [ ] **LLVM Backend**: Alternative code generation for better optimization
- [ ] **Game Boy Color support**: Double-speed mode, color palettes
- [ ] **Super Game Boy support**: Border, enhanced colors
- [ ] **Link cable emulation**: For multiplayer games
- [ ] **Debugger integration**: Breakpoints, memory viewer
- [ ] **Web target**: WASM output for browser play

---

## Key Lessons from CHIP-8 Project

1. **Start simple**: Get basic ROMs working before complex ones
2. **Single-function mode**: Essential for ROMs with complex control flow
3. **Reachability analysis**: Don't try to compile data as code
4. **Cooperative yielding**: Let main loop handle timing/events
5. **Platform abstraction**: Keep generated code platform-agnostic
6. **Quirk modes**: Hardware variations need configurable behavior
7. **Embedded ROM data**: Include original bytes for memory access
8. **Per-game settings**: Different games need different configurations
9. **Debug overlay**: Essential for development and debugging
10. **Batch compilation**: Useful for ROM collections

---

## References

- [CHIP-8 Recompiled Source](https://github.com/arcanite24/chip8recompiled)
- [N64Recomp](https://github.com/Mr-Wiseguy/N64Recomp) - Inspiration
- [Pan Docs](https://gbdev.io/pandocs/) - GameBoy technical reference
- [SM83 Instruction Set](https://gbdev.io/gb-opcodes/optables/) - Opcode tables
- [Mooneye Test ROMs](https://github.com/Gekkio/mooneye-test-suite) - Accuracy tests
- [BGB Emulator](https://bgb.bircd.org/) - Reference emulator with debugger
