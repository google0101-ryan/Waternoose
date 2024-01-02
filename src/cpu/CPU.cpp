#include <cpu/CPU.h>
#include <memory/memory.h>
#include <cstdlib>
#include <cstdio>
#include <cstring>

CPUThread::CPUThread(uint32_t entryPoint, uint32_t stackSize)
{
	std::memset(&state, 0, sizeof(state));

	state.pc = entryPoint;

	if (stackSize < 16*1024)
	{
		stackSize = 16*1024;
	}

	uint32_t stackBase = Memory::VirtAllocMemoryRange(0x70000000, 0x7F000000, stackSize);
	Memory::AllocMemory(stackBase, stackSize);

	printf("Stack base is 0x%08x\n", stackBase);

	state.regs[1] = (stackBase+stackSize);
}

void CPUThread::Run()
{
	uint32_t instr = Memory::Read32(state.pc);
	state.pc += 4;

	printf("0x%08x (0x%08lx): ", instr, state.pc-4);
	
	if (((instr >> 26) & 0x3F) == 18)
	{
		branch(instr);
	}
	else if (((instr >> 26) & 0x3F) == 19 && ((instr >> 1) & 0x3FF) == 16)
	{
		bclr(instr);
	}
	else if (((instr >> 26) & 0x3F) == 31 && ((instr >> 1) & 0x3FF) == 339)
	{	
		mfspr(instr);
	}
	else if (((instr >> 26) & 0x3F) == 36)
	{
		stw(instr);
	}
	else if (((instr >> 26) & 0x3F) == 62)
	{
		std(instr);
	}
	else
	{
		printf("Failed to execute instruction: 0x%08x\n", instr);
		exit(1);
	}
}

uint8_t GetCRBit(const uint32_t bit) { return 1 << (3 - (bit % 4)); }

bool CPUThread::CondPassed(uint8_t bo, uint8_t bi)
{
	bool bo0 = (bo & 0x10) ? 1 : 0;
	bool bo1 = (bo & 0x08) ? 1 : 0;
	bool bo2 = (bo & 0x04) ? 1 : 0;
	bool bo3 = (bo & 0x02) ? 1 : 0;

	if (!bo2) state.ctr--;
	bool ctr_ok = bo2 | ((state.ctr != 0) ^ bo3);
	bool cond_ok = bo0 | ((state.GetCR(bi >> 2) & GetCRBit(bi) ? 1 : 0) ^ (~bo1 & 1));

	return ctr_ok && cond_ok;
}