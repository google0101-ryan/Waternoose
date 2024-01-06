#include <cpu/CPU.h>
#include <memory/memory.h>
#include <loader/xex.h>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <cassert>
#include "CPU.h"

CPUThread::CPUThread(uint32_t entryPoint, uint32_t stackSize, XexLoader& ref)
: xexRef(ref)
{
	std::memset(&state, 0, sizeof(state));

	state.pc = entryPoint;

	if (stackSize < 16*1024)
	{
		stackSize = 16*1024;
	}

	uint32_t stackBase = Memory::VirtAllocMemoryRange(0x70000000, 0x7F000000, stackSize);
	Memory::AllocMemory(stackBase, stackSize);

	uint32_t pcrAddress = Memory::VirtAllocMemoryRange(0xE0000000, 0xFFD00000, 0x2D8);
	Memory::AllocMemory(pcrAddress, 0x2D8);

	uint32_t tlsAddr = Memory::VirtAllocMemoryRange(0xE0000000, 0xFFD00000, 4096);
	Memory::AllocMemory(tlsAddr, 4096);

	uint32_t xthreadAddr = Memory::VirtAllocMemoryRange(0xE0000000, 0xFFD00000, 4096);
	Memory::AllocMemory(xthreadAddr, 4096);

	Memory::Write32(pcrAddress+0x00, tlsAddr);
	Memory::Write32(pcrAddress+0x100, xthreadAddr);

	printf("Stack base is 0x%08x\n", stackBase);

	state.regs[1] = (stackBase+stackSize);
	state.regs[13] = pcrAddress;
}

void CPUThread::Run()
{
	uint32_t instr = Memory::Read32(state.pc);
	state.pc += 4;

	printf("0x%08x (0x%08lx): ", instr, state.pc-4);

	if (((instr >> 26) & 0x3F) == 3)
	{
		twi(instr);
	}
	else if (((instr >> 26) & 0x3F) == 7)
	{
		mulli(instr);
	}
	else if (((instr >> 26) & 0x3F) == 8)
	{
		subfic(instr);
	}
	else if (((instr >> 26) & 0x3F) == 10)
	{
		cmpli(instr);
	}
	else if (((instr >> 26) & 0x3F) == 11)
	{
		cmpi(instr);
	}
	else if (((instr >> 26) & 0x3F) == 12)
	{
		addic(instr);
	}
	else if (((instr >> 26) & 0x3F) == 14)
	{
		addi(instr);
	}
	else if (((instr >> 26) & 0x3F) == 15)
	{
		addis(instr);
	}
	else if (((instr >> 26) & 0x3F) == 16)
	{
		bc(instr);
	}
	else if (((instr >> 26) & 0x3F) == 17)
	{
		sc(instr);
	}
	else if (((instr >> 26) & 0x3F) == 18)
	{
		branch(instr);
	}
	else if (((instr >> 26) & 0x3F) == 19 && ((instr >> 1) & 0x3FF) == 16)
	{
		bclr(instr);
	}
	else if (((instr >> 26) & 0x3F) == 20)
	{
		rlwimi(instr);
	}
	else if (((instr >> 26) & 0x3F) == 21)
	{
		rlwinm(instr);
	}
	else if (((instr >> 26) & 0x3F) == 24)
	{
		ori(instr);
	}
	else if (((instr >> 26) & 0x3F) == 28)
	{
		andi(instr);
	}
	else if (((instr >> 26) & 0x3F) == 31 && ((instr >> 1) & 0x3FF) == 20)
	{	
		lwarx(instr);
	}
	else if (((instr >> 26) & 0x3F) == 31 && ((instr >> 1) & 0x3FF) == 23)
	{	
		lwzx(instr);
	}
	else if (((instr >> 26) & 0x3F) == 31 && ((instr >> 1) & 0x3FF) == 32)
	{	
		cmpl(instr);
	}
	else if (((instr >> 26) & 0x3F) == 31 && ((instr >> 1) & 0x3FF) == 40)
	{	
		subf(instr);
	}
	else if (((instr >> 26) & 0x3F) == 31 && ((instr >> 1) & 0x3FF) == 60)
	{	
		andc(instr);
	}
	else if (((instr >> 26) & 0x3F) == 31 && ((instr >> 1) & 0x3FF) == 83)
	{	
		mfmsr(instr);
	}
	else if (((instr >> 26) & 0x3F) == 31 && ((instr >> 1) & 0x3FF) == 136)
	{	
		subfe(instr);
	}
	else if (((instr >> 26) & 0x3F) == 31 && ((instr >> 1) & 0x3FF) == 150)
	{	
		stwcx(instr);
	}
	else if (((instr >> 26) & 0x3F) == 31 && ((instr >> 1) & 0x3FF) == 178)
	{	
		mtmsrd(instr);
	}
	else if (((instr >> 26) & 0x3F) == 31 && ((instr >> 1) & 0x3FF) == 235)
	{	
		mullw(instr);
	}
	else if (((instr >> 26) & 0x3F) == 31 && ((instr >> 1) & 0x3FF) == 246)
	{	
		dcbt(instr);
	}
	else if (((instr >> 26) & 0x3F) == 31 && ((instr >> 1) & 0x3FF) == 266)
	{	
		add(instr);
	}
	else if (((instr >> 26) & 0x3F) == 31 && ((instr >> 1) & 0x3FF) == 278)
	{	
		dcbt(instr);
	}
	else if (((instr >> 26) & 0x3F) == 31 && ((instr >> 1) & 0x3FF) == 339)
	{	
		mfspr(instr);
	}
	else if (((instr >> 26) & 0x3F) == 31 && ((instr >> 1) & 0x3FF) == 371)
	{	
		mftb(instr);
	}
	else if (((instr >> 26) & 0x3F) == 31 && ((instr >> 1) & 0x3FF) == 444)
	{	
		or_(instr);
	}
	else if (((instr >> 26) & 0x3F) == 31 && ((instr >> 1) & 0x3FF) == 459)
	{	
		divwu(instr);
	}
	else if (((instr >> 26) & 0x3F) == 31 && ((instr >> 1) & 0x3FF) == 467)
	{	
		mtspr(instr);
	}
	else if (((instr >> 26) & 0x3F) == 32)
	{
		lwz(instr);
	}
	else if (((instr >> 26) & 0x3F) == 34)
	{
		lbz(instr);
	}
	else if (((instr >> 26) & 0x3F) == 36)
	{
		stw(instr);
	}
	else if (((instr >> 26) & 0x3F) == 37)
	{
		stwu(instr);
	}
	else if (((instr >> 26) & 0x3F) == 38)
	{
		stb(instr);
	}
	else if (((instr >> 26) & 0x3F) == 40)
	{
		lhz(instr);
	}
	else if (((instr >> 26) & 0x3F) == 44)
	{
		sth(instr);
	}
	else if (((instr >> 26) & 0x3F) == 58)
	{
		ld(instr);
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

void CPUThread::Dump()
{
	for (int i = 0; i < 32; i++)
		printf("r%d\t->\t0x%08lx\n", i, state.regs[i]);
	for (int i = 0; i < 7; i++)
		printf("cr%d\t->\t%d\n", i, state.GetCR(i));
	printf("[%s]\n", state.xer.ca ? "c" : ".");
}

void CPUThread::SetArg(int num, uint64_t value)
{
	printf("Setting arg%d: 0x%08lx\n", num, value);
	assert(num < 6);
	state.regs[3 + num] = value;
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