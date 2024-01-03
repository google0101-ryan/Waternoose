#include <cpu/CPU.h>
#include <memory/memory.h>
#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <bit>
#include "CPU.h"

uint64_t GenShiftMask64(uint8_t me, uint8_t mb)
{
	mb = 63 - mb;
	me = 63 - me;

	uint64_t mask = 0;
	for (int i = me; i <= mb; i++)
		mask |= (1ULL << i);
	return mask;
}

template<class T>
T sign_extend(T x, const int bits) 
{
    T m = 1;
    m <<= bits - 1;
    return (x ^ m) - m;
}

void CPUThread::cmpli(uint32_t instruction)
{
	uint8_t bf = (instruction >> 23) & 0x7;
	bool l = (instruction >> 21) & 1;
	uint8_t ra = (instruction >> 16) & 0x1F;
	uint64_t ui = instruction & 0xFFFF;

	if (l)
	{
		state.UpdateCRn<uint64_t>(state.regs[ra], ui, bf);
		printf("cmpldi cr%d,r%d,0x%08lx\n", bf, ra, ui);
	}
	else
	{
		state.UpdateCRn<uint32_t>(state.regs[ra], ui, bf);
		printf("cmplwi cr%d,r%d,0x%08lx\n", bf, ra, ui);
	}
}

void CPUThread::addi(uint32_t instruction)
{
	int16_t si = (int16_t)(instruction & 0xFFFF);
	uint8_t ra = (instruction >> 16) & 0x1F;
	uint8_t rt = (instruction >> 21) & 0x1F;

	if (ra == 0)
	{
		printf("li r%d, 0x%08lx\n", rt, (int64_t)si);
		state.regs[rt] = (int64_t)si;
	}
	else
	{
		if (si < 0)
			printf("subi r%d,r%d,%d\n", rt, ra, -si);
		else
			printf("addi r%d,r%d,%d\n", rt, ra, si);
		state.regs[rt] = state.regs[ra] + (int64_t)si;
	}
}

void CPUThread::addis(uint32_t instruction)
{
	int32_t si = ((instruction & 0xFFFF) << 16);
	uint8_t ra = (instruction >> 16) & 0x1F;
	uint8_t rt = (instruction >> 21) & 0x1F;

	if (ra == 0)
	{
		printf("lis r%d, 0x%08lx\n", rt, (int64_t)si);
		state.regs[rt] = (int64_t)si;
	}
	else
	{
		if (si < 0)
			printf("subis r%d,r%d,%d\n", rt, ra, -si);
		else
			printf("addis r%d,r%d,%d\n", rt, ra, si);
		state.regs[rt] = state.regs[ra] + (int64_t)si;
	}
}

void CPUThread::bc(uint32_t instruction)
{
	uint8_t bi = (instruction >> 16) & 0x1F;
	uint8_t bo = (instruction >> 21) & 0x1F;
	int16_t bd = (int16_t)(instruction & 0xFFFC);
	bool aa = (instruction >> 1) & 1;
	bool lk = instruction & 1;

	uint32_t target;
	if (aa)
		target = (uint16_t)bd;
	else
		target = (state.pc - 4) + (int64_t)bd;
	
	printf("bc%s 0x%08x\n", lk ? "l" : "", target);

	if (CondPassed(bo, bi))
	{
		state.pc = target;
	}
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

void CPUThread::rlwinm(uint32_t instruction)
{
	uint8_t rs = (instruction >> 21) & 0x1F;
	uint8_t ra = (instruction >> 16) & 0x1F;
	uint8_t sh = (instruction >> 11) & 0x1F;
	uint8_t mb = (instruction >> 6) & 0x1F;
	uint8_t me = (instruction >> 1) & 0x1F;
	bool rc = instruction & 1;

	uint32_t r = std::rotl<uint32_t>(state.regs[rs], sh);
	uint64_t mask = GenShiftMask64(me+32, mb+32);
	state.regs[ra] = r & mask;

	if (rc)
		state.UpdateCRn<int32_t>(state.regs[ra], 0, 0);
	
	printf("rlwinm r%d,r%d,%d,0x%02x,0x%02x\n", ra, rs, sh, mb, me);
}

void CPUThread::ori(uint32_t instruction)
{
	uint8_t rs = (instruction >> 21) & 0x1F;
	uint8_t ra = (instruction >> 16) & 0x1F;
	uint16_t ui = instruction & 0xFFFF;

	state.regs[ra] = state.regs[rs] | ui;

	printf("ori r%d,r%d,0x%04x\n", rs, ra, ui);
}

void CPUThread::or_(uint32_t instruction)
{
	uint8_t rs = (instruction >> 21) & 0x1F;
	uint8_t ra = (instruction >> 16) & 0x1F;
	uint8_t rb = (instruction >> 11) & 0x1F;
	bool rc = (instruction & 1);

	state.regs[ra] = state.regs[rs] | state.regs[rb];

	if (rc)
		state.UpdateCRn<int64_t>(state.regs[ra], 0L, 0);

	if (rs == rb)
		printf("mr r%d,r%d\n", ra, rs);
	else
		printf("or%s r%d,r%d,r%d\n", rc ? "." : "", ra, rs, rb);
}

void CPUThread::mtspr(uint32_t instruction)
{
	uint8_t rs = (instruction >> 21) & 0x1F;
	uint16_t spr = (instruction >> 11) & 0x3FF;

	switch (spr)
	{
	case 0x100:
		state.lr = state.regs[rs];
		printf("mtlr r%d\n", rs);
		break;
	default:
		printf("Write to unknown SPR 0x%08x\n", spr);
		exit(1);
	}
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

void CPUThread::lwz(uint32_t instruction)
{
	int16_t ds = (int16_t)(instruction & 0xFFFF);
	uint8_t ra = (instruction >> 16) & 0x1F;
	uint8_t rs = (instruction >> 21) & 0x1F;

	uint32_t ea = 0;
	if (ra == 0)
		ea = ds;
	else
		ea = state.regs[ra] + ds;
	
	printf("lwz r%d, %d(r%d)\n", rs, ds, ra);

	state.regs[rs] = Memory::Read32(ea);
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

void CPUThread::stwu(uint32_t instruction)
{
	int16_t ds = (int16_t)(instruction & 0xFFFF);
	uint8_t ra = (instruction >> 16) & 0x1F;
	uint8_t rs = (instruction >> 21) & 0x1F;

	assert(ra != 0);
	
	uint32_t ea = state.regs[ra] + ds;
	
	printf("stwu r%d, %d(r%d)\n", rs, ds, ra);

	state.regs[ra] = ea;

	Memory::Write32(ea, state.regs[rs]);
}

void CPUThread::ld(uint32_t instruction)
{
	uint8_t rt = (instruction >> 21) & 0x1F;
	uint8_t ra = (instruction >> 16) & 0x1F;
	int16_t ds = instruction & 0xFFFC;

	uint32_t ea;
	if (!ra)
		ea = (int32_t)ds;
	else
		ea = state.regs[ra] + ds;
	
	state.regs[rt] = Memory::Read64(ea);

	printf("ld r%d, %d(r%d)\n", rt, ds, ra);
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
