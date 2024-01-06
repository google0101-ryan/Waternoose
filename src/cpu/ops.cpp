#include <cpu/CPU.h>
#include <memory/memory.h>
#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <bit>
#include <time.h>

#include <kernel/kernel.h>
#include <kernel/Module.h>
#include <loader/xex.h>
#include "CPU.h"

uint32_t GenShiftMask64(uint8_t me, uint8_t mb)
{
	uint32_t maskmb = ~0u >> mb;
	uint32_t maskme = ~0u << (31 - me);
	return (mb <= me) ? maskmb & maskme : maskmb | maskme;
}

uint32_t GenShiftMask(uint8_t me)
{
	uint32_t maskme = ~0u >> me;
	return maskme;
}

template<class T>
T sign_extend(T x, const int bits) 
{
    T m = 1;
    m <<= bits - 1;
    return (x ^ m) - m;
}

void CPUThread::twi(uint32_t instruction)
{
	uint8_t to = (instruction >> 21) & 0x1F;
	uint8_t ra = (instruction >> 16) & 0x1F;
	int64_t simm = (int64_t)(int16_t)(instruction & 0xFFFF);
	int64_t a = state.regs[ra];

	bool trap = false;
	if ((a < simm) && ((to >> 4) & 1)) trap = true;
	if ((a > simm) && ((to >> 3) & 1)) trap = true;
	if ((a == simm) && ((to >> 2) & 1)) trap = true;
	if (((uint64_t)a < (uint64_t)simm) && ((to >> 1) & 1)) trap = true;
	if (((uint64_t)a > (uint64_t)simm) && ((to >> 0) & 1)) trap = true;

	printf("twi %d,r%d,%ld\n", to, ra, simm);

	if (trap)
	{
		printf("TRAP\n");
		exit(1);
	}
}

void CPUThread::mulli(uint32_t instruction)
{
	uint8_t rt = (instruction >> 21) & 0x1F;
	uint8_t ra = (instruction >> 16) & 0x1F;
	int64_t si = (int64_t)(int16_t)(instruction & 0xFFFF);

	state.regs[rt] = (int64_t)state.regs[ra] * si;

	printf("mulli r%d,r%d,%ld\n", rt, ra, si);
}

void CPUThread::subfic(uint32_t instruction)
{
	uint8_t rt = (instruction >> 21) & 0x1F;
	uint8_t ra = (instruction >> 16) & 0x1F;
	uint64_t simm = (uint64_t)(int64_t)(int16_t)(instruction & 0xFFFF);

	state.regs[rt] = simm - state.regs[ra];

	state.xer.ca = state.regs[ra] <= simm;

	printf("subfic r%d,r%d,0x%08lx\n", rt, ra, simm);
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

void CPUThread::cmpi(uint32_t instruction)
{
	uint8_t bf = (instruction >> 23) & 0x7;
	bool l = (instruction >> 21) & 1;
	uint8_t ra = (instruction >> 16) & 0x1F;
	int64_t ui = (int64_t)(int16_t)(instruction & 0xFFFF);

	if (l)
	{
		state.UpdateCRn<int64_t>(state.regs[ra], ui, bf);
		printf("cmpdi cr%d,r%d,0x%08lx\n", bf, ra, ui);
	}
	else
	{
		state.UpdateCRn<int32_t>(state.regs[ra], ui, bf);
		printf("cmpwi cr%d,r%d,0x%08lx\n", bf, ra, ui);
	}
}

void CPUThread::addic(uint32_t instruction)
{
	int16_t si = (int16_t)(instruction & 0xFFFF);
	uint8_t rt = (instruction >> 21) & 0x1F;
	uint8_t ra = (instruction >> 16) & 0x1F;

	if (si < 0)
		state.xer.ca = (state.regs[ra] < -si);

	if (si < 0)
		printf("subic r%d,r%d,%d\n", rt, ra, -si);
	else
		printf("addic r%d,r%d,%d\n", rt, ra, si);
	state.regs[rt] = state.regs[ra] + (int64_t)si;

	if (si >= 0)
		state.xer.ca = state.regs[rt] < state.regs[ra];
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

void CPUThread::sc(uint32_t instruction)
{
	uint32_t lev = (instruction >> 5) & 0x7F;
	assert(lev == 2);

	uint32_t modNum = (state.regs[11] >> 12) & 0xF;
	uint32_t ordinal = state.regs[11] & 0xFFF;

	auto& name = xexRef.GetLibraries()[modNum].name;
	Kernel::GetModuleByName(name.c_str())->CallFunctionByOrdinal(ordinal, *this);
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

void CPUThread::rlwimi(uint32_t instruction)
{
	uint8_t rs = (instruction >> 21) & 0x1F;
	uint8_t ra = (instruction >> 16) & 0x1F;
	uint8_t sh = (instruction >> 11) & 0x1F;
	uint8_t mb = (instruction >> 6) & 0x1F;
	uint8_t me = (instruction >> 1) & 0x1F;
	bool rc = instruction & 1;

	uint32_t r = std::rotl<uint32_t>(state.regs[rs], sh);
	uint64_t mask = GenShiftMask64(me, mb);
	state.regs[ra] = (r & mask) | (state.regs[ra] & ~mask);

	if (rc)
		state.UpdateCRn<int32_t>(state.regs[ra], 0, 0);
	
	printf("rlwimi r%d,r%d,%d,0x%02x,0x%02x\n", ra, rs, sh, mb, me);
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

	printf("ori r%d,r%d,0x%04x\n", ra, rs, ui);
}

void CPUThread::andi(uint32_t instruction)
{
	uint8_t rs = (instruction >> 21) & 0x1F;
	uint8_t ra = (instruction >> 16) & 0x1F;
	uint16_t ui = instruction & 0xFFFF;

	state.regs[ra] = state.regs[rs] & ui;
	state.UpdateCRn<int64_t>(state.regs[ra], 0, 0);

	printf("andi. r%d,r%d,0x%04x\n", ra, rs, ui);
}

void CPUThread::lwarx(uint32_t instruction)
{
	uint8_t rt = (instruction >> 21) & 0x1F;
	uint8_t ra = (instruction >> 16) & 0x1F;
	uint8_t rb = (instruction >> 11) & 0x1F;

	uint32_t ea;
	if (ra == 0)
		ea = state.regs[rb];
	else
		ea = state.regs[ra] + state.regs[rb];
	
	state.regs[rt] = Memory::Read32(ea);

	printf("lwarx r%d, r%d(r%d)\n", rt, ra, rb);
}

void CPUThread::lwzx(uint32_t instruction)
{
	uint8_t rt = (instruction >> 21) & 0x1F;
	uint8_t ra = (instruction >> 16) & 0x1F;
	uint8_t rb = (instruction >> 11) & 0x1F;

	uint32_t ea;
	if (ra == 0)
		ea = state.regs[rb];
	else
		ea = state.regs[ra] + state.regs[rb];
	
	state.regs[rt] = Memory::Read32(ea);

	printf("lwzx r%d, r%d(r%d)\n", rt, ra, rb);
}

void CPUThread::cmpl(uint32_t instruction)
{
	uint8_t ra = (instruction >> 16) & 0x1F;
	uint8_t rb = (instruction >> 11) & 0x1F;
	bool l = (instruction >> 21) & 1;
	uint8_t bf = (instruction >> 23) & 0x7;

	if (l)
	{
		uint64_t a = state.regs[ra];
		uint64_t b = state.regs[rb];
		state.UpdateCRn<uint64_t>(a, b, bf);
		printf("cmpld cr%d,r%d,r%d\n", bf, ra, rb);
	}
	else
	{
		uint32_t a = state.regs[ra];
		uint32_t b = state.regs[rb];
		state.UpdateCRn<uint32_t>(a, b, bf);
		printf("cmplw cr%d,r%d,r%d\n", bf, ra, rb);
	}
}

void CPUThread::subf(uint32_t instruction)
{
	uint8_t rt = (instruction >> 21) & 0x1F;
	uint8_t ra = (instruction >> 16) & 0x1F;
	uint8_t rb = (instruction >> 11) & 0x1F;
	bool oe = (instruction >> 10) & 1;
	bool rc = instruction & 1;
	assert(!oe);

	state.regs[rt] = state.regs[ra] - state.regs[rb];

	if (rc)
		state.UpdateCRn<int64_t>(state.regs[rt], 0, 0);
	
	printf("subf r%d,r%d,r%d\n", rt, ra, rb);
}

void CPUThread::andc(uint32_t instruction)
{
	uint8_t rs = (instruction >> 21) & 0x1F;
	uint8_t ra = (instruction >> 16) & 0x1F;
	uint8_t rb = (instruction >> 11) & 0x1F;
	bool rc = instruction & 1;

	state.regs[ra] = state.regs[rs] & ~state.regs[rb];

	if (rc)
		state.UpdateCRn<int64_t>(state.regs[ra], 0, 0);
	
	printf("andc r%d,r%d,r%d\n", ra, rs, rb);
}

void CPUThread::mfmsr(uint32_t instruction)
{
	uint8_t rt = (instruction >> 21) & 0x1F;

	state.regs[rt] = state.msr;

	printf("mfmsr r%d\n", rt);
}

void CPUThread::subfe(uint32_t instruction)
{
	uint8_t rt = (instruction >> 21) & 0x1F;
	uint8_t ra = (instruction >> 16) & 0x1F;
	uint8_t rb = (instruction >> 11) & 0x1F;
	bool oe = (instruction >> 10) & 1;
	bool rc = instruction & 1;

	state.regs[rt] = ~state.regs[ra] + state.regs[rb] + state.xer.ca;

	if (rc)
		state.UpdateCRn<int64_t>(state.regs[rt], 0, 0);
	
	state.xer.ca = state.regs[ra] < (state.regs[rb] + state.xer.ca);

	printf("subfe r%d,r%d,r%d\n", rt, ra, rb);
}

void CPUThread::stwcx(uint32_t instruction)
{
	assert(instruction & 1);

	uint8_t rt = (instruction >> 21) & 0x1F;
	uint8_t ra = (instruction >> 16) & 0x1F;
	uint8_t rb = (instruction >> 11) & 0x1F;

	uint32_t ea;
	if (ra == 0)
		ea = state.regs[rb];
	else
		ea = state.regs[ra] + state.regs[rb];

	// This is pretty much always preceeded by lwarx, 
	// so I think it's safe to assume as much
	Memory::Write32(ea, state.regs[rt]);
	state.CR.cr0 = 2;

	printf("stwcx. r%d, r%d(r%d)\n", rt, ra, rb);
}

void CPUThread::mtmsrd(uint32_t instruction)
{
	uint8_t rt = (instruction >> 21) & 0x1F;
	uint8_t ra = (instruction >> 16) & 0x1F;

	if (ra & 1)
	{
		state.msr = state.regs[rt];
		if (rt == 13)
			state.msr = 0x8000;
		else
			state.msr = 0;
	}

	printf("mtmsrd r%d,%d\n", rt, ra);
}

void CPUThread::mullw(uint32_t instruction)
{
	uint8_t rt = (instruction >> 21) & 0x1F;
	uint8_t ra = (instruction >> 16) & 0x1F;
	uint8_t rb = (instruction >> 11) & 0x1F;
	assert(!((instruction >> 10) & 1));
	bool rc = instruction & 1;

	state.regs[rt] = (uint64_t)(uint32_t)state.regs[ra] * (uint64_t)(uint32_t)state.regs[rb];

	if (rc)
		state.UpdateCRn<int32_t>(state.regs[rt], 0, 0);
	
	printf("mullw r%d,r%d,r%d\n", rt, ra, rb);
}

void CPUThread::add(uint32_t instruction)
{
	uint8_t rt = (instruction >> 21) & 0x1F;
	uint8_t ra = (instruction >> 16) & 0x1F;
	uint8_t rb = (instruction >> 11) & 0x1F;
	bool oe = (instruction >> 10) & 1;
	bool rc = instruction & 1;
	assert(!oe);

	state.regs[rt] = state.regs[ra] + state.regs[rb];

	if (rc)
		state.UpdateCRn<int64_t>(state.regs[rt], 0, 0);
	
	printf("add%s r%d,r%d,r%d\n", rc ? "." : "", rt, ra, rb);
}

void CPUThread::dcbt(uint32_t instruction)
{
	printf("dcbt\n");
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

void CPUThread::divwu(uint32_t instruction)
{
	uint8_t rt = (instruction >> 21) & 0x1F;
	uint8_t ra = (instruction >> 16) & 0x1F;
	uint8_t rb = (instruction >> 11) & 0x1F;
	assert(!((instruction >> 10) & 1));
	bool rc = (instruction & 1);

	uint64_t dividend = (uint32_t)state.regs[ra];
	uint64_t divisor = (uint32_t)state.regs[rb];
	state.regs[rt] = (uint32_t)(dividend / divisor);

	if (rc)
		state.UpdateCRn<int32_t>(state.regs[rt], 0, 0);
	
	printf("divwu r%d,r%d,r%d\n", rt, ra, rb);
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
	case 0x120:
		state.ctr = state.regs[rs];
		printf("mtctr r%d\n", rs);
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

void CPUThread::mftb(uint32_t instruction)
{
	uint64_t time_ = time(NULL);
	uint8_t rt = (instruction >> 21) & 0x1F;
	uint16_t tbr = (instruction >> 11) & 0x3FF;
	tbr = ((tbr >> 5) & 0x1F) | ((tbr & 0x1F) << 5);
	state.regs[rt] = time_;
	printf("mftb r%d\n", rt);
}

void CPUThread::lwz(uint32_t instruction)
{
	int64_t ds = (int64_t)(int16_t)(instruction & 0xFFFF);
	uint8_t ra = (instruction >> 16) & 0x1F;
	uint8_t rs = (instruction >> 21) & 0x1F;

	uint32_t ea = 0;
	if (ra == 0)
		ea = ds;
	else
		ea = state.regs[ra] + ds;
	
	printf("lwz r%d, %ld(r%d)\n", rs, ds, ra);

	state.regs[rs] = Memory::Read32(ea);
}

void CPUThread::lbz(uint32_t instruction)
{
	int16_t ds = (int16_t)(instruction & 0xFFFF);
	uint8_t ra = (instruction >> 16) & 0x1F;
	uint8_t rs = (instruction >> 21) & 0x1F;

	uint32_t ea = 0;
	if (ra == 0)
		ea = ds;
	else
		ea = state.regs[ra] + ds;
	
	printf("lbz r%d, %d(r%d)\n", rs, ds, ra);

	state.regs[rs] = Memory::Read8(ea);
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

void CPUThread::stb(uint32_t instruction)
{
	uint8_t rt = (instruction >> 21) & 0x1F;
	uint8_t ra = (instruction >> 16) & 0x1F;
	int16_t ds = instruction & 0xFFFC;

	uint32_t ea;
	if (!ra)
		ea = (int32_t)ds;
	else
		ea = state.regs[ra] + ds;
	
	Memory::Write8(ea, state.regs[rt]);

	printf("stb r%d, %d(r%d)\n", rt, ds, ra);
}

void CPUThread::lhz(uint32_t instruction)
{
	uint8_t rt = (instruction >> 21) & 0x1F;
	uint8_t ra = (instruction >> 16) & 0x1F;
	int16_t ds = instruction & 0xFFFF;

	uint32_t ea;
	if (!ra)
		ea = (int32_t)ds;
	else
		ea = state.regs[ra] + ds;
	
	state.regs[rt] = Memory::Read16(ea);

	printf("lhz r%d, %d(r%d)\n", rt, ds, ra);
}

void CPUThread::sth(uint32_t instruction)
{
	uint8_t rt = (instruction >> 21) & 0x1F;
	uint8_t ra = (instruction >> 16) & 0x1F;
	int16_t ds = instruction & 0xFFFF;

	uint32_t ea;
	if (!ra)
		ea = (int32_t)ds;
	else
		ea = state.regs[ra] + ds;
	
	Memory::Write16(ea, state.regs[rt]);

	printf("sth r%d, %d(r%d)\n", rt, ds, ra);
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
