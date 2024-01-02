#pragma once

#include <stdint.h>

namespace Memory
{

void Initialize();
void Dump();

/// @brief Map a chunk of memory into the address space, while also allocing memory to back it
/// @param baseAddress The start of the address range to map
/// @param size The size, in bytes, of the allocation
/// @return A pointer to the newly allocated chunk of memory
void* AllocMemory(uint32_t baseAddress, uint32_t size);

/// @brief Used this function to acquire a base address from a range
/// For example, the stack is from `0x70000000 ... 0x7F000000`, so you'd do `VirtAllocMemoryRange(0x70000000, 0x7F000000, stack_size)`
/// Used `AllocMemory` to actually commit the memory, while also mapping it into memory at the appropriate address
/// @param beginAddr The start of the range to alloc from
/// @param endAddr The end of the range to alloc from
/// @param size The size, in bytes, of the allocation
uint32_t VirtAllocMemoryRange(uint32_t beginAddr, uint32_t endAddr, uint32_t size);

uint32_t Read32(uint32_t addr);

void Write32(uint32_t addr, uint32_t data);
void Write64(uint32_t addr, uint64_t data);

}