/**
 * @file analyzer.cpp
 * @brief Control flow analyzer implementation (stub for MVP)
 */

#include "recompiler/analyzer.h"
#include <algorithm>
#include <queue>
#include <iostream>
#include <iomanip>
#include <sstream>

namespace gbrecomp {

/* ============================================================================
 * Helper Functions
 * ========================================================================== */

static uint32_t make_address(uint8_t bank, uint16_t addr) {
    return (static_cast<uint32_t>(bank) << 16) | addr;
}

static uint8_t get_bank(uint32_t addr) {
    return static_cast<uint8_t>(addr >> 16);
}

static uint16_t get_offset(uint32_t addr) {
    return static_cast<uint16_t>(addr & 0xFFFF);
}

/* ============================================================================
 * AnalysisResult Implementation
 * ========================================================================== */

const Instruction* AnalysisResult::get_instruction(uint8_t bank, uint16_t addr) const {
    uint32_t full_addr = make_addr(bank, addr);
    auto it = addr_to_index.find(full_addr);
    if (it != addr_to_index.end() && it->second < instructions.size()) {
        return &instructions[it->second];
    }
    return nullptr;
}

const BasicBlock* AnalysisResult::get_block(uint8_t bank, uint16_t addr) const {
    uint32_t full_addr = make_addr(bank, addr);
    auto it = blocks.find(full_addr);
    if (it != blocks.end()) {
        return &it->second;
    }
    return nullptr;
}

const Function* AnalysisResult::get_function(uint8_t bank, uint16_t addr) const {
    uint32_t full_addr = make_addr(bank, addr);
    auto it = functions.find(full_addr);
    if (it != functions.end()) {
        return &it->second;
    }
    return nullptr;
}

/* ============================================================================
 * Analysis Implementation
 * ========================================================================== */

AnalysisResult analyze(const ROM& rom, const AnalyzerOptions& options) {
    AnalysisResult result;
    result.rom = &rom;
    result.entry_point = 0x100;
    
    // Add standard GameBoy entry points
    result.interrupt_vectors = {0x40, 0x48, 0x50, 0x58, 0x60};  // Interrupt vectors
    
    Decoder decoder(rom);
    
    // Track addresses to explore
    std::queue<uint32_t> work_queue;
    std::set<uint32_t> visited;
    
    // Entry point is always a function
    result.call_targets.insert(make_address(0, 0x100));
    
    // RST vectors are implicit functions
    for (uint16_t vec : {0x00, 0x08, 0x10, 0x18, 0x20, 0x28, 0x30, 0x38}) {
        result.call_targets.insert(make_address(0, vec));
        work_queue.push(make_address(0, vec));
    }
    
    // Interrupt vectors are functions
    for (uint16_t vec : result.interrupt_vectors) {
        result.call_targets.insert(make_address(0, vec));
        work_queue.push(make_address(0, vec));
    }
    
    // Start from entry point
    work_queue.push(make_address(0, 0x100));
    
    // Explore all reachable code
    while (!work_queue.empty()) {
        uint32_t addr = work_queue.front();
        work_queue.pop();
        
        if (visited.count(addr)) continue;
        
        uint8_t bank = get_bank(addr);
        uint16_t offset = get_offset(addr);
        
        // Only analyze ROM space
        if (offset >= 0x8000) continue;
        
        // Skip if outside ROM bounds
        if (bank > 0 && offset < 0x4000) continue;  // Bank 0 only in 0x0000-0x3FFF
        
        visited.insert(addr);
        
        // Decode instruction
        Instruction instr = decoder.decode(offset, bank);
        
        // Store instruction
        size_t idx = result.instructions.size();
        result.instructions.push_back(instr);
        result.addr_to_index[addr] = idx;
        
        // Track control flow
        if (instr.is_call) {
            result.call_targets.insert(make_address(bank, instr.imm16));
            work_queue.push(make_address(bank, instr.imm16));
            if (!instr.is_conditional) {
                // Non-conditional calls fall through
                work_queue.push(make_address(bank, offset + instr.length));
            }
        } else if (instr.is_jump) {
            if (instr.type == InstructionType::JP_NN || instr.type == InstructionType::JP_CC_NN) {
                result.label_addresses.insert(make_address(bank, instr.imm16));
                work_queue.push(make_address(bank, instr.imm16));
            } else if (instr.type == InstructionType::JR_N || instr.type == InstructionType::JR_CC_N) {
                uint16_t target = offset + instr.length + instr.offset;
                result.label_addresses.insert(make_address(bank, target));
                work_queue.push(make_address(bank, target));
            }
            
            if (instr.is_conditional) {
                // Conditional jumps fall through - this is also a block start
                uint32_t fall_through = make_address(bank, offset + instr.length);
                result.label_addresses.insert(fall_through);
                work_queue.push(fall_through);
            }
        } else if (instr.is_return) {
            // Returns end the block
        } else if (instr.type == InstructionType::RST) {
            result.call_targets.insert(make_address(0, instr.rst_vector));
            work_queue.push(make_address(0, instr.rst_vector));
            work_queue.push(make_address(bank, offset + instr.length));
        } else {
            // Continue to next instruction
            work_queue.push(make_address(bank, offset + instr.length));
        }
    }
    
    // Build basic blocks from instruction boundaries
    std::set<uint32_t> block_starts;
    block_starts.insert(make_address(0, 0x100));  // Entry point
    
    for (uint32_t target : result.call_targets) {
        block_starts.insert(target);
    }
    for (uint32_t target : result.label_addresses) {
        block_starts.insert(target);
    }
    
    // Create blocks
    for (uint32_t start : block_starts) {
        if (!visited.count(start)) continue;
        
        BasicBlock block;
        block.start_address = get_offset(start);
        block.bank = get_bank(start);
        block.is_reachable = true;
        
        if (result.call_targets.count(start)) {
            block.is_function_entry = true;
        }
        
        // Find instructions in this block
        uint32_t curr = start;
        while (visited.count(curr)) {
            auto it = result.addr_to_index.find(curr);
            if (it == result.addr_to_index.end()) break;
            
            block.instruction_indices.push_back(it->second);
            const Instruction& instr = result.instructions[it->second];
            
            block.end_address = get_offset(curr) + instr.length;
            
            // Track successors for control flow
            if (instr.is_jump) {
                if (instr.type == InstructionType::JP_NN || instr.type == InstructionType::JP_CC_NN) {
                    block.successors.push_back(instr.imm16);
                } else if (instr.type == InstructionType::JR_N || instr.type == InstructionType::JR_CC_N) {
                    uint16_t target = get_offset(curr) + instr.length + instr.offset;
                    block.successors.push_back(target);
                }
                // Conditional jumps also fall through
                if (instr.is_conditional) {
                    block.successors.push_back(get_offset(curr) + instr.length);
                }
            }
            
            // Check if this ends the block
            if (instr.is_jump || instr.is_return || instr.is_call) {
                break;
            }
            
            curr = make_address(block.bank, get_offset(curr) + instr.length);
            
            // Check if next instruction starts a new block
            if (block_starts.count(curr)) {
                // Fall through to the new block - add as successor
                block.successors.push_back(get_offset(curr));
                break;
            }
        }
        
        result.blocks[start] = block;
    }
    
    // Create functions from call targets
    for (uint32_t target : result.call_targets) {
        auto block_it = result.blocks.find(target);
        if (block_it == result.blocks.end()) continue;
        
        Function func;
        func.name = generate_function_name(get_bank(target), get_offset(target));
        func.entry_address = get_offset(target);
        func.bank = get_bank(target);
        func.block_addresses.push_back(get_offset(target));
        
        // Add all blocks reachable from this function (simple DFS)
        std::queue<uint32_t> func_queue;
        std::set<uint32_t> func_visited;
        func_queue.push(target);
        
        while (!func_queue.empty()) {
            uint32_t block_addr = func_queue.front();
            func_queue.pop();
            
            if (func_visited.count(block_addr)) continue;
            func_visited.insert(block_addr);
            
            auto blk = result.blocks.find(block_addr);
            if (blk == result.blocks.end()) continue;
            
            // Add this block to function if not already there
            if (block_addr != target) {
                func.block_addresses.push_back(get_offset(block_addr));
            }
            
            // Follow successors (but not into other functions)
            for (uint16_t succ : blk->second.successors) {
                uint32_t succ_addr = make_address(blk->second.bank, succ);
                // Don't follow into other function entry points
                if (result.call_targets.count(succ_addr) && succ_addr != target) continue;
                if (!func_visited.count(succ_addr)) {
                    func_queue.push(succ_addr);
                }
            }
        }
        
        result.functions[target] = func;
    }
    
    // Update stats
    result.stats.total_instructions = result.instructions.size();
    result.stats.total_blocks = result.blocks.size();
    result.stats.total_functions = result.functions.size();
    
    return result;
}

AnalysisResult analyze_bank(const ROM& rom, uint8_t bank, const AnalyzerOptions& options) {
    // For now, just analyze the whole ROM
    // TODO: Filter to specific bank
    return analyze(rom, options);
}

/* ============================================================================
 * Utility Functions
 * ========================================================================== */

std::string generate_function_name(uint8_t bank, uint16_t address) {
    std::ostringstream ss;
    
    // Check for known GameBoy entry points
    if (bank == 0) {
        switch (address) {
            case 0x0000: return "rst_00";
            case 0x0008: return "rst_08";
            case 0x0010: return "rst_10";
            case 0x0018: return "rst_18";
            case 0x0020: return "rst_20";
            case 0x0028: return "rst_28";
            case 0x0030: return "rst_30";
            case 0x0038: return "rst_38";
            case 0x0040: return "int_vblank";
            case 0x0048: return "int_lcd_stat";
            case 0x0050: return "int_timer";
            case 0x0058: return "int_serial";
            case 0x0060: return "int_joypad";
            case 0x0100: return "gb_main";  // Avoid shadowing C main()
        }
    }
    
    ss << "func_";
    if (bank > 0) {
        ss << std::hex << std::setfill('0') << std::setw(2) << (int)bank << "_";
    }
    ss << std::hex << std::setfill('0') << std::setw(4) << address;
    return ss.str();
}

std::string generate_label_name(uint8_t bank, uint16_t address) {
    std::ostringstream ss;
    ss << "loc_";
    if (bank > 0) {
        ss << std::hex << std::setfill('0') << std::setw(2) << (int)bank << "_";
    }
    ss << std::hex << std::setfill('0') << std::setw(4) << address;
    return ss.str();
}

void print_analysis_summary(const AnalysisResult& result) {
    std::cout << "=== Analysis Summary ===" << std::endl;
    std::cout << "Total instructions: " << result.stats.total_instructions << std::endl;
    std::cout << "Total basic blocks: " << result.stats.total_blocks << std::endl;
    std::cout << "Total functions: " << result.stats.total_functions << std::endl;
    std::cout << "Call targets: " << result.call_targets.size() << std::endl;
    std::cout << "Label addresses: " << result.label_addresses.size() << std::endl;
    
    std::cout << "\nFunctions found:" << std::endl;
    for (const auto& [addr, func] : result.functions) {
        std::cout << "  " << func.name << " @ ";
        if (func.bank > 0) {
            std::cout << std::hex << std::setfill('0') << std::setw(2) << (int)func.bank << ":";
        }
        std::cout << std::hex << std::setfill('0') << std::setw(4) << func.entry_address << std::endl;
    }
}

bool is_likely_data(const AnalysisResult& result, uint8_t bank, uint16_t address) {
    uint32_t full_addr = AnalysisResult::make_addr(bank, address);
    return result.addr_to_index.find(full_addr) == result.addr_to_index.end();
}

} // namespace gbrecomp
