#include <cpu/CPU.h>
#include <memory/memory.h>
#include <cstdio>
#include <cstdlib>
#include "CPU.h"

template<class T>
T sign_extend(T x, const int bits) 
{
    T m = 1;
    m <<= bits - 1;
    return (x ^ m) - m;
}

void CPUThread::branch(uint32_t instruction)
{
	bool lk = (instruction & 1);
	bool aa = (instruction >> 1) & 1;
	int32_t li = sign_extend<int32_t>(instruction & 0x3FFFFFC, 26);

	if (lk)
		state.lr = state.pc;
	
	if (aa)
		state.pc = li;
	else
		state.pc = (state.pc-4)+li;
	
	printf("b%s 0x%08lx\n", lk ? "l" : "", state.pc);
}

void CPUThread::bclr(uint32_t instruction)
{
	uint8_t bo = (instruction >> 21) & 0x1F;
	uint8_t bi = (instruction >> 16) & 0x1F;
	bool lk = instruction & 1;

	uint64_t old_lr = state.lr;
	if (lk) state.lr = state.pc;

	if (CondPassed(bo, bi))
	{
		state.pc = state.lr;
	}

	printf("bclr\n");
}

void CPUThread::mfspr(uint32_t instruction) 
{
	uint8_t rt = (instruction >> 21) & 0x1F;
	uint16_t spr = (instruction >> 11) & 0x3FF;

	switch (spr)
	{
	case 0x100:
		state.regs[rt] = state.lr;
		printf("mflr r%d\n", rt);
		break;
	default:
		printf("Read from unknown SPR 0x%08x\n", spr);
		exit(1);
	}
}

void CPUThread::stw(uint32_t instruction)
{
	int16_t ds = (int16_t)(instruction & 0xFFFF);
	uint8_t ra = (instruction >> 16) & 0x1F;
	uint8_t rs = (instruction >> 21) & 0x1F;

	uint32_t ea = 0;
	if (ra == 0)
		ea = ds;
	else
		ea = state.regs[ra] + ds;
	
	printf("stw r%d, %d(r%d)\n", rs, ds, ra);

	Memory::Write32(ea, state.regs[rs]);
}

void CPUThread::std(uint32_t instruction)
{
	int16_t ds = (int16_t)(instruction & 0xFFFF);
	uint8_t ra = (instruction >> 16) & 0x1F;
	uint8_t rs = (instruction >> 21) & 0x1F;

	uint32_t ea = 0;
	if (ra == 0)
		ea = ds;
	else
		ea = state.regs[ra] + ds;
	
	printf("std r%d, %d(r%d)\n", rs, ds, ra);

	Memory::Write64(ea, state.regs[rs]);
}
