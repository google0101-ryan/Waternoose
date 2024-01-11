#include <cpu/CPU.h>
#include <memory/memory.h>
#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <bit>
#include <time.h>
#include <cmath>
#include <cstring>

#include <kernel/kernel.h>
#include <kernel/Module.h>
#include <loader/xex.h>
#include "CPU.h"

static inline uint64_t XEMASK(uint32_t mstart, uint32_t mstop) 
{
	mstart &= 0x3F;
  	mstop &= 0x3F;
  	uint64_t value =
      (UINT64_MAX >> mstart) ^ ((mstop >= 63) ? 0 : UINT64_MAX >> (mstop + 1));
  	return mstart <= mstop ? value : ~value;
}

uint32_t GenShiftMask(uint8_t me)
{
	uint32_t maskme = ~0u >> me;
	return maskme;
}



uint32_t GenShiftMask64(uint8_t me, uint8_t mb)
{
	uint32_t maskmb = ~0u >> mb;
	uint32_t maskme = ~0u << (31 - me);
	return (mb <= me) ? maskmb & maskme : maskmb | maskme;
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

void CPUThread::lvx128(uint32_t instruction)
{
	uint32_t vd = ((instruction >> 21) & 0x1F) | (((instruction >> 2) & 0x3) << 5);
	uint8_t ra = (instruction >> 16) & 0x1F;
	uint8_t rb = (instruction >> 11) & 0x1F;

	uint32_t ea;
	if (ra == 0)
		ea = state.regs[rb];
	else
		ea = state.regs[ra] + state.regs[rb];
	
	state.vfr[vd].u128 = Memory::Read128(ea);

	printf("lvx128 v%d, r%d, r%d\n", vd, ra, rb);
}

void CPUThread::stvx128(uint32_t instruction)
{
	uint32_t vd = ((instruction >> 21) & 0x1F) | (((instruction >> 2) & 0x3) << 5);
	uint8_t ra = (instruction >> 16) & 0x1F;
	uint8_t rb = (instruction >> 11) & 0x1F;

	uint32_t ea;
	if (ra == 0)
		ea = state.regs[rb];
	else
		ea = state.regs[ra] + state.regs[rb];
	
	Memory::Write128(ea, state.vfr[vd].u128);

	printf("stvx128 v%d, r%d, r%d\n", vd, ra, rb);
}

void CPUThread::vslb(uint32_t instruction)
{
	uint8_t vd = (instruction >> 21) & 0x1F;
	uint8_t va = (instruction >> 16) & 0x1F;
	uint8_t vb = (instruction >> 11) & 0x1F;

	for (int i = 0; i < 16; i++)
	{
		state.vfr[vd].u8[i] = state.vfr[va].u8[i] << (state.vfr[vb].u8[i] & 7);
	}

	printf("vslb v%d,v%d,v%d\n", vd, va, vb);
}

void CPUThread::vspltb(uint32_t instruction)
{
	uint8_t vd = (instruction >> 21) & 0x1F;
	uint8_t uimm = (instruction >> 16) & 0x1F;
	uint8_t vb = (instruction >> 11) & 0x1F;

	uint8_t val = state.vfr[vb].u8[uimm];

	for (int i = 0; i < 16; i++)
		state.vfr[vd].u8[i] = val;

	printf("vspltb v%d,v%d,%d\n", vd, vb, uimm);
}

void CPUThread::vspltisb(uint32_t instruction)
{
	uint8_t vd = (instruction >> 21) & 0x1F;
	uint8_t imm = (instruction >> 16) & 0x1F;
	imm = (imm & 0x10) ? (uint8_t)(imm | 0xF0) : imm;

	for (int i = 0; i < 16; i++)
		state.vfr[vd].u8[i] = imm;

	printf("vspltisb v%d,%d\n", vd, imm);
}

void CPUThread::vspltish(uint32_t instruction)
{
	uint8_t vd = (instruction >> 21) & 0x1F;
	uint16_t imm = (instruction >> 16) & 0x1F;
	imm = (imm & 0x10) ? (uint16_t)(imm | 0xFFF0) : imm;

	for (int i = 0; i < 8; i++)
		state.vfr[vd].u16[i] = imm;

	printf("vspltish v%d,%d\n", vd, imm);
}

void CPUThread::vspltisw128(uint32_t instruction)
{
	uint32_t vd = ((instruction >> 21) & 0x1F) | (((instruction >> 2) & 0x3) << 5);
	uint32_t imm = (instruction >> 11) & 0x1F;
	imm = (imm & 0x10) ? (uint32_t)-1 : imm;

	for (int i = 0; i < 4; i++)
		state.vfr[vd].u32[i] = imm;
	
	printf("vspltisw128 v%d, 0x%08x\n", vd, imm);
}

void CPUThread::vor(uint32_t instruction)
{
	uint8_t vd = (instruction >> 21) & 0x1F;
	uint8_t va = (instruction >> 16) & 0x1F;
	uint8_t vb = (instruction >> 11) & 0x1F;

	state.vfr[vd].u128 = state.vfr[va].u128 | state.vfr[vb].u128;

	printf("vor v%d,v%d,v%d\n", vd, va, vb);
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

	state.regs[rt] = ~state.regs[ra] + simm + 1;

	uint32_t trunc_v2 = (uint32_t)state.regs[ra];
	state.xer.ca = ((uint32_t)simm > (trunc_v2-1)) | (!trunc_v2);

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
	uint64_t si = (int64_t)(int16_t)(instruction & 0xFFFF);
	uint8_t rt = (instruction >> 21) & 0x1F;
	uint8_t ra = (instruction >> 16) & 0x1F;

	if (si < 0)
		printf("subic r%d,r%d,%d\n", rt, ra, -si);
	else
		printf("addic r%d,r%d,%d\n", rt, ra, si);

	state.xer.ca = ((uint32_t)si < !((uint32_t)state.regs[ra]));

	state.regs[rt] = state.regs[ra] + (int64_t)si;
}

void CPUThread::addicx(uint32_t instruction)
{
	int16_t si = (int16_t)(instruction & 0xFFFF);
	uint8_t rt = (instruction >> 21) & 0x1F;
	uint8_t ra = (instruction >> 16) & 0x1F;

	if (si < 0)
		printf("subic. r%d,r%d,%d\n", rt, ra, -si);
	else
		printf("addic. r%d,r%d,%d\n", rt, ra, si);

	state.xer.ca = ((uint32_t)(int64_t)si < !((uint32_t)state.regs[ra]));

	state.regs[rt] = state.regs[ra] + (int64_t)si;

	state.UpdateCRn<int32_t>(state.regs[rt], 0, 0);
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
		state.pc = old_lr;
	}

	printf("bclr\n");
}

void CPUThread::bctr(uint32_t instruction)
{
	uint8_t bo = (instruction >> 21) & 0x1F;
	uint8_t bi = (instruction >> 16) & 0x1F;
	bool lk = instruction & 1;

	if (lk) state.lr = state.pc;

	if (CondPassed(bo, bi))
	{
		state.pc = state.ctr;
	}

	printf("bctr\n");
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
	uint64_t mask = GenShiftMask64(me, mb);
	state.regs[ra] = r & mask;

	if (rc)
		state.UpdateCRn<int32_t>(state.regs[ra], 0, 0);
	
	printf("rlwinm r%d,r%d,%d,0x%02x,0x%02x (0x%08x)\n", ra, rs, sh, mb, me, mask);
}

void CPUThread::ori(uint32_t instruction)
{
	uint8_t rs = (instruction >> 21) & 0x1F;
	uint8_t ra = (instruction >> 16) & 0x1F;
	uint16_t ui = instruction & 0xFFFF;

	state.regs[ra] = state.regs[rs] | ui;

	printf("ori r%d,r%d,0x%04x\n", ra, rs, ui);
}

void CPUThread::oris(uint32_t instruction)
{
	uint8_t rs = (instruction >> 21) & 0x1F;
	uint8_t ra = (instruction >> 16) & 0x1F;
	uint32_t ui = instruction & 0xFFFF;

	state.regs[ra] = state.regs[rs] | (ui << 16);

	printf("oris r%d,r%d,0x%04x\n", ra, rs, ui);
}

void CPUThread::andi(uint32_t instruction)
{
	uint8_t rs = (instruction >> 21) & 0x1F;
	uint8_t ra = (instruction >> 16) & 0x1F;
	uint16_t ui = instruction & 0xFFFF;

	state.regs[ra] = state.regs[rs] & ui;
	state.UpdateCRn<int32_t>(state.regs[ra], 0, 0);

	printf("andi. r%d,r%d,0x%04x\n", ra, rs, ui);
}

void CPUThread::andis(uint32_t instruction)
{
	uint8_t rs = (instruction >> 21) & 0x1F;
	uint8_t ra = (instruction >> 16) & 0x1F;
	uint32_t ui = (instruction & 0xFFFF) << 16;

	state.regs[ra] = state.regs[rs] & ui;
	state.UpdateCRn<int32_t>(state.regs[ra], 0, 0);

	printf("andis. r%d,r%d,0x%04x\n", ra, rs, ui);
}

void CPUThread::rldicl(uint32_t instruction)
{
	uint8_t rt = (instruction >> 21) & 0x1F;
	uint8_t ra = (instruction >> 16) & 0x1F;
	uint16_t sh = ((instruction >> 11) & 0x1F) | (((instruction >> 1) & 1) << 5);
	uint16_t mb = ((instruction >> 6) & 0x1F) | (((instruction >> 5) & 0x1) << 5);

	uint64_t m = XEMASK(mb, 63);
	state.regs[ra] = (std::rotl<uint64_t>(state.regs[rt], sh) & m);

	printf("rldicl r%d,r%d,%d,%d\n", rt, ra, sh, mb);
}

void CPUThread::rldicr(uint32_t instruction)
{
	uint8_t rt = (instruction >> 21) & 0x1F;
	uint8_t ra = (instruction >> 16) & 0x1F;
	uint16_t sh = ((instruction >> 11) & 0x1F) | (((instruction >> 1) & 1) << 5);
	uint16_t mb = ((instruction >> 6) & 0x1F) | (((instruction >> 5) & 0x1) << 5);

	uint64_t m = XEMASK(0, mb);
	state.regs[ra] = (std::rotl<uint64_t>(state.regs[rt], sh) & m);

	printf("rldicr r%d,r%d,%d,%d\n", rt, ra, sh, mb);
}

void CPUThread::cmp(uint32_t instruction)
{
	uint8_t ra = (instruction >> 16) & 0x1F;
	uint8_t rb = (instruction >> 11) & 0x1F;
	bool l = (instruction >> 21) & 1;
	uint8_t bf = (instruction >> 23) & 0x7;

	if (l)
	{
		int64_t a = state.regs[ra];
		int64_t b = state.regs[rb];
		state.UpdateCRn<int64_t>(a, b, bf);
		printf("cmpd cr%d,r%d,r%d\n", bf, ra, rb);
	}
	else
	{
		int32_t a = (int32_t)state.regs[ra];
		int32_t b = (int32_t)state.regs[rb];
		state.UpdateCRn<int32_t>(a, b, bf);
		printf("cmpw cr%d,r%d,r%d\n", bf, ra, rb);
	}
}

#define UINT128(hi, lo) (((__uint128_t) (hi)) << 64 | (lo))

uint128_t vsl_table[16] = 
{
	{.u128 = UINT128(0x0001020304050607, 0x08090A0B0C0D0E0F)},
	{.u128 = UINT128(0x0102030405060708, 0x090A0B0C0D0E0F10)},
	{.u128 = UINT128(0x0203040506070809, 0x0A0B0C0D0E0F1011)},
	{.u128 = UINT128(0x030405060708090A, 0x0B0C0D0E0F101112)},
	{.u128 = UINT128(0x0405060708090A0B, 0x0C0D0E0F10111213)},
	{.u128 = UINT128(0x05060708090A0B0C, 0x0D0E0F1011121314)},
	{.u128 = UINT128(0x060708090A0B0C0D, 0x0E0F101112131415)},
	{.u128 = UINT128(0x0708090A0B0C0D0E, 0x0F10111213141516)},
	{.u128 = UINT128(0x08090A0B0C0D0E0F, 0x1011121314151617)},
	{.u128 = UINT128(0x090A0B0C0D0E0F10, 0x1112131415161718)},
	{.u128 = UINT128(0x0A0B0C0D0E0F1011, 0x1213141516171819)},
	{.u128 = UINT128(0x0B0C0D0E0F101112, 0x131415161718191A)},
	{.u128 = UINT128(0x0C0D0E0F10111213, 0x1415161718191A1B)},
	{.u128 = UINT128(0x0D0E0F1011121314, 0x15161718191A1B1C)},
	{.u128 = UINT128(0x0E0F101112131415, 0x161718191A1B1C1D)},
	{.u128 = UINT128(0x0F10111213141516, 0x1718191A1B1C1D1E)}
};

void CPUThread::lvsl(uint32_t instruction)
{
	uint8_t vd = (instruction >> 21) & 0x1F;
	uint8_t ra = (instruction >> 16) & 0x1F;
	uint8_t rb = (instruction >> 11) & 0x1F;

	uint32_t sh = 0;
	if (ra == 0)
		sh = state.regs[rb];
	else
		sh = state.regs[ra] + state.regs[rb];
	
	sh &= 0xF;

	state.vfr[vd].u128 = vsl_table[sh].u128;

	printf("lvsl v%d,r%d,r%d\n", vd, ra, rb);
}

void CPUThread::subfc(uint32_t instruction)
{
	uint8_t rt = (instruction >> 21) & 0x1F;
	uint8_t ra = (instruction >> 16) & 0x1F;
	uint8_t rb = (instruction >> 11) & 0x1F;
	bool rc = instruction & 1;
	assert(!((instruction >> 10) & 1));

	uint64_t result = state.regs[rb] - state.regs[ra];

	uint32_t trunc_v2 = (uint32_t)state.regs[ra];
	state.xer.ca = ((uint32_t)state.regs[rb] > (trunc_v2-1)) | (!trunc_v2);
	
	state.regs[rt] = result;
	
	if (rc)
		state.UpdateCRn<int32_t>(state.regs[rt], 0, 0);
	
	printf("subfc r%d,r%d,r%d\n", rt, ra, rb);
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

void CPUThread::slw(uint32_t instruction)
{
	uint8_t rs = (instruction >> 21) & 0x1F;
	uint8_t ra = (instruction >> 16) & 0x1F;
	uint8_t rb = (instruction >> 11) & 0x1F;
	bool rc = instruction & 1;
	
	if ((state.regs[rb] >> 6) & 1)
		state.regs[ra] = 0;
	else
		state.regs[ra] = (uint32_t)state.regs[rs] << (state.regs[rb] & 0x3F);
	if (rc)
		state.UpdateCRn<int32_t>(state.regs[ra], 0, 0);

	printf("slw r%d,r%d,r%d\n", rs,ra,rb);
}

void CPUThread::cntlzw(uint32_t instruction)
{
	uint8_t rs = (instruction >> 21) & 0x1F;
	uint8_t ra = (instruction >> 16) & 0x1F;

	uint32_t s = state.regs[rs];
	int n = 0;
	while (n < 32)
	{
		if ((s >> n) & 1) break;
		n++;
	}

	printf("cntlzw r%d,r%d (%d)\n", rs, ra, n);

	state.regs[ra] = n;
	if (instruction & 1)
		state.UpdateCRn<int32_t>(state.regs[ra], 0, 0);
}

void CPUThread::sld(uint32_t instruction)
{
	uint8_t rs = (instruction >> 21) & 0x1F;
	uint8_t ra = (instruction >> 16) & 0x1F;
	uint8_t rb = (instruction >> 11) & 0x1F;
	bool rc = instruction & 1;
	
	if ((state.regs[rb] >> 6) & 1)
		state.regs[ra] = 0;
	else
		state.regs[ra] = state.regs[rs] << (state.regs[rb] & 0x3F);
	if (rc)
		state.UpdateCRn<int32_t>(state.regs[ra], 0, 0);

	printf("sld r%d,r%d,r%d\n", rs,ra,rb);
}

void CPUThread::and_(uint32_t instruction)
{
	uint8_t rs = (instruction >> 21) & 0x1F;
	uint8_t ra = (instruction >> 16) & 0x1F;
	uint8_t rb = (instruction >> 11) & 0x1F;
	bool rc = (instruction & 1);

	state.regs[ra] = state.regs[rs] & state.regs[rb];

	if (rc)
		state.UpdateCRn<int32_t>(state.regs[ra], 0L, 0);

	printf("and%s r%d,r%d,r%d\n", rc ? "." : "", ra, rs, rb);
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

	state.regs[rt] = state.regs[rb] - state.regs[ra];

	if (rc)
		state.UpdateCRn<int32_t>(state.regs[rt], 0, 0);
	
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
		state.UpdateCRn<int32_t>(state.regs[ra], 0, 0);
	
	printf("andc r%d,r%d,r%d\n", ra, rs, rb);
}

void CPUThread::mfmsr(uint32_t instruction)
{
	uint8_t rt = (instruction >> 21) & 0x1F;

	state.regs[rt] = state.msr;

	printf("mfmsr r%d\n", rt);
}

void CPUThread::lbzx(uint32_t instruction)
{
	uint8_t rd = ((instruction >> 21) & 0x1F);
	uint8_t ra = (instruction >> 16) & 0x1F;
	uint8_t rb = (instruction >> 11) & 0x1F;

	uint32_t ea;
	if (ra == 0)
		ea = state.regs[rb];
	else
		ea = state.regs[ra] + state.regs[rb];
	
	state.regs[rd] = Memory::Read8(ea);

	printf("lbzx v%d, r%d, r%d\n", rd, ra, rb);
}

void CPUThread::neg(uint32_t instruction)
{
	uint8_t rt = (instruction >> 21) & 0x1F;
	uint8_t ra = (instruction >> 16) & 0x1F;

	if ((int64_t)state.regs[ra] == INT64_MIN)
	{
		state.regs[rt] = state.regs[ra];
	}
	else
	{
		state.regs[rt] = ~state.regs[ra] + 1;
	}

	printf("neg r%d,r%d\n", rt, ra);

	if (instruction & 1)
		state.UpdateCRn<int32_t>(state.regs[rt], 0, 0);
}

void CPUThread::nor(uint32_t instruction)
{
	uint8_t rs = (instruction >> 21) & 0x1F;
	uint8_t ra = (instruction >> 16) & 0x1F;
	uint8_t rb = (instruction >> 11) & 0x1F;
	bool rc = (instruction & 1);

	state.regs[ra] = ~(state.regs[rs] | state.regs[rb]);

	if (rc)
		state.UpdateCRn<int32_t>(state.regs[ra], 0L, 0);

	printf("nor%s r%d,r%d,r%d (0x%08x)\n", rc ? "." : "", ra, rs, rb, state.regs[ra]);
}

void CPUThread::subfe(uint32_t instruction)
{
	uint8_t rt = (instruction >> 21) & 0x1F;
	uint8_t ra = (instruction >> 16) & 0x1F;
	uint8_t rb = (instruction >> 11) & 0x1F;
	bool oe = (instruction >> 10) & 1;
	bool rc = instruction & 1;
	
	uint32_t v1 = (uint32_t)(~state.regs[ra]);
	uint32_t v2 = (uint32_t)(state.regs[rb]);
	uint32_t v3 = state.xer.ca;
	state.xer.ca = ((v1+v2+v3) < v3) | ((v1+v2) < v1);

	state.regs[rt] = ~state.regs[ra] + state.regs[rb] + state.xer.ca;

	if (rc)
		state.UpdateCRn<int32_t>(state.regs[rt], 0, 0);
	
	

	printf("subfe r%d,r%d,r%d\n", rt, ra, rb);
}

void CPUThread::stdx(uint32_t instruction)
{
	uint8_t rt = (instruction >> 21) & 0x1F;
	uint8_t ra = (instruction >> 16) & 0x1F;
	uint8_t rb = (instruction >> 11) & 0x1F;

	uint32_t ea;
	if (ra == 0)
		ea = state.regs[rb];
	else
		ea = state.regs[ra] + state.regs[rb];

	Memory::Write64(ea, state.regs[rt]);

	printf("stdx r%d, r%d(r%d)\n", rt, ra, rb);
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

void CPUThread::stwx(uint32_t instruction)
{
	uint8_t rt = (instruction >> 21) & 0x1F;
	uint8_t ra = (instruction >> 16) & 0x1F;
	uint8_t rb = (instruction >> 11) & 0x1F;

	uint32_t ea;
	if (ra == 0)
		ea = state.regs[rb];
	else
		ea = state.regs[ra] + state.regs[rb];

	Memory::Write32(ea, state.regs[rt]);

	printf("stwx r%d, r%d(r%d)\n", rt, ra, rb);
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

void CPUThread::subfze(uint32_t instruction)
{
	uint8_t rt = (instruction >> 21) & 0x1F;
	uint8_t ra = (instruction >> 16) & 0x1F;
	assert(!((instruction >> 10) & 1));
	bool rc = instruction & 1;

	state.regs[rt] = ~(state.regs[ra]) + state.xer.ca;
	
	uint32_t v1 = (uint32_t)(~state.regs[ra]);
	uint32_t v2 = 0;
	uint32_t v3 = state.xer.ca;
	state.xer.ca = ((v1+v2+v3) < v3) | ((v1+v2) < v1);

	if (rc)
		state.UpdateCRn<int32_t>(state.regs[rt], 0, 0);
	
	printf("subfze r%d,r%d\n", rt, ra);
}

void CPUThread::addze(uint32_t instruction)
{
	uint8_t rt = (instruction >> 21) & 0x1F;
	uint8_t ra = (instruction >> 16) & 0x1F;
	assert(!((instruction >> 10) & 1));
	bool rc = instruction & 1;

	uint64_t result = state.regs[ra] + state.xer.ca;

	uint32_t v1 = (uint32_t)(state.regs[ra]);
	uint32_t v2 = 0;
	uint32_t v3 = state.xer.ca;
	state.xer.ca = ((v1+v2+v3) < v3) | ((v1+v2) < v1);

	state.regs[rt] = result;
	if (rc)
		state.UpdateCRn<int32_t>(result, 0, 0);
	
	printf("addze r%d,r%d\n", rt, ra);
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
		state.UpdateCRn<int32_t>(state.regs[rt], 0, 0);
	
	printf("add%s r%d,r%d,r%d\n", rc ? "." : "", rt, ra, rb);
}

void CPUThread::dcbt(uint32_t instruction)
{
	printf("dcbt\n");
}

void CPUThread::xor_(uint32_t instruction)
{
	uint8_t rs = (instruction >> 21) & 0x1F;
	uint8_t ra = (instruction >> 16) & 0x1F;
	uint8_t rb = (instruction >> 11) & 0x1F;
	bool rc = (instruction & 1);

	state.regs[ra] = state.regs[rs] ^ state.regs[rb];

	if (rc)
		state.UpdateCRn<int32_t>(state.regs[ra], 0L, 0);

	printf("xor%s r%d,r%d,r%d (0x%08x)\n", rc ? "." : "", ra, rs, rb, state.regs[ra]);
}

void CPUThread::or_(uint32_t instruction)
{
	uint8_t rs = (instruction >> 21) & 0x1F;
	uint8_t ra = (instruction >> 16) & 0x1F;
	uint8_t rb = (instruction >> 11) & 0x1F;
	bool rc = (instruction & 1);

	state.regs[ra] = state.regs[rs] | state.regs[rb];

	if (rc)
		state.UpdateCRn<int32_t>(state.regs[ra], 0L, 0);

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

void CPUThread::divd(uint32_t instruction)
{
	uint8_t rt = (instruction >> 21) & 0x1F;
	uint8_t ra = (instruction >> 16) & 0x1F;
	uint8_t rb = (instruction >> 11) & 0x1F;

	state.regs[rt] = (int64_t)state.regs[ra] / (int64_t)state.regs[rb];

	printf("divd r%d,r%d,r%d\n", rt, ra, rb);
}

void CPUThread::stvlx(uint32_t instruction)
{
	uint8_t rs = (instruction >> 21) & 0x1F;
	uint8_t ra = (instruction >> 16) & 0x1F;
	uint8_t rb = (instruction >> 11) & 0x1F;

	uint32_t ea = 0;
	if (ra == 0)
		ea = state.regs[rb];
	else
		ea = state.regs[ra] + state.regs[rb];
	
	printf("stvlx v%d,r%d,r%d\n", rs, ra, rb);

	uint32_t tail = ea & 15;
	for (int i = 0; i < 16 - tail; i++)
		Memory::Write8(ea+i, state.vfr[rs].u8[i]);
}

void CPUThread::stwbrx(uint32_t instruction)
{
	uint8_t rs = (instruction >> 21) & 0x1F;
	uint8_t ra = (instruction >> 16) & 0x1F;
	uint8_t rb = (instruction >> 11) & 0x1F;

	uint32_t ea = 0;
	if (ra == 0)
		ea = state.regs[rb];
	else
		ea = state.regs[ra] + state.regs[rb];
	
	printf("stwbrx v%d,r%d,r%d\n", rs, ra, rb);

	Memory::Write32(ea, bswap32(state.regs[rs]));

	printf("stwbrx r%d,r%d,r%d\n", rs, ra, rb);
}

void CPUThread::stvrx(uint32_t instruction)
{
	uint8_t rs = (instruction >> 21) & 0x1F;
	uint8_t ra = (instruction >> 16) & 0x1F;
	uint8_t rb = (instruction >> 11) & 0x1F;

	uint32_t ea = 0;
	if (ra == 0)
		ea = state.regs[rb];
	else
		ea = state.regs[ra] + state.regs[rb];
	
	printf("stvrx v%d,r%d,r%d\n", rs, ra, rb);

	uint32_t tail = ea & 15;
	for (int i = 15; i > 15 - tail; i--)
		Memory::Write8(ea+i, state.vfr[rs].u8[i]);
}

void CPUThread::srawi(uint32_t instruction)
{
	uint8_t rs = (instruction >> 21) & 0x1F;
	uint8_t ra = (instruction >> 16) & 0x1F;
	uint8_t sh = (instruction >> 11) & 0x1F;
	bool rc = instruction & 1;

	int32_t rs_ = state.regs[rs];
	state.regs[ra] = rs_ >> sh;
	state.xer.ca = (rs_ < 0) & ((state.regs[ra] << sh) != rs_);
	if (rc)
		state.UpdateCRn<int32_t>(state.regs[ra], 0, 0);
	
	printf("srawi r%d,r%d,%d\n", ra, rs, sh);
}

void CPUThread::dcbz(uint32_t instruction)
{
	uint8_t ra = (instruction >> 16) & 0x1F;
	uint8_t rb = (instruction >> 11) & 0x1F;

	uint32_t ea = 0;
	if (ra)
		ea = state.regs[rb];
	else
		ea = state.regs[ra] + state.regs[rb];
	
	memset(Memory::GetRawPtrForAddr(ea), 0, 0x80);

	printf("dcbz r%d,r%d\n", ra, rb);
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

void CPUThread::sthx(uint32_t instruction)
{
	uint8_t rs = ((instruction >> 21) & 0x1F);
	uint8_t ra = (instruction >> 16) & 0x1F;
	uint8_t rb = (instruction >> 11) & 0x1F;

	uint32_t ea;
	if (ra == 0)
		ea = state.regs[rb];
	else
		ea = state.regs[ra] + state.regs[rb];
	
	Memory::Write16(ea, state.regs[rs]);

	printf("sthx r%d,r%d,r%d\n", rs, ra, rb);
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

void CPUThread::lwzu(uint32_t instruction)
{
	int64_t ds = (int64_t)(int16_t)(instruction & 0xFFFF);
	uint8_t ra = (instruction >> 16) & 0x1F;
	uint8_t rs = (instruction >> 21) & 0x1F;

	uint32_t ea = 0;
	if (ra == 0)
		ea = ds;
	else
		ea = state.regs[ra] + ds;
	
	printf("lwzu r%d, %ld(r%d)\n", rs, ds, ra);

	state.regs[rs] = Memory::Read32(ea);
	state.regs[ra] = ea;
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

void CPUThread::lbzu(uint32_t instruction)
{
	int16_t ds = (int16_t)(instruction & 0xFFFF);
	uint8_t ra = (instruction >> 16) & 0x1F;
	uint8_t rs = (instruction >> 21) & 0x1F;

	uint32_t ea = state.regs[ra] + ds;
	
	printf("lbzu r%d, %d(r%d)\n", rs, ds, ra);

	state.regs[rs] = Memory::Read8(ea);
	state.regs[ra] = ea;
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

void CPUThread::stbu(uint32_t instruction)
{
	uint8_t rt = (instruction >> 21) & 0x1F;
	uint8_t ra = (instruction >> 16) & 0x1F;
	int16_t ds = instruction & 0xFFFC;

	uint32_t ea = state.regs[ra] + ds;
	
	Memory::Write8(ea, state.regs[rt]);

	state.regs[ra] = ea;

	printf("stbu r%d, %d(r%d)\n", rt, ds, ra);
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

void CPUThread::lfs(uint32_t instruction)
{
	uint8_t frt = (instruction >> 21) & 0x1F;
	uint8_t ra = (instruction >> 16) & 0x1F;
	int16_t ds = instruction & 0xFFFC;

	uint32_t ea;
	if (!ra)
		ea = (int32_t)ds;
	else
		ea = state.regs[ra] + ds;
	
	state.fr[frt].d = std::bit_cast<float>(Memory::Read32(ea));

	printf("lfs fr%d, %d(r%d)\n", frt, ds, ra);
}

void CPUThread::lfd(uint32_t instruction)
{
	uint8_t frt = (instruction >> 21) & 0x1F;
	uint8_t ra = (instruction >> 16) & 0x1F;
	int16_t ds = instruction & 0xFFFC;

	uint32_t ea;
	if (!ra)
		ea = (int32_t)ds;
	else
		ea = state.regs[ra] + ds;
	
	state.fr[frt].u = Memory::Read64(ea);

	printf("lfd fr%d, %d(r%d) (%f, 0x%08lx)\n", frt, ds, ra, state.fr[frt].d, state.fr[frt].u);
}

void CPUThread::stfs(uint32_t instruction)
{
	uint8_t frt = (instruction >> 21) & 0x1F;
	uint8_t ra = (instruction >> 16) & 0x1F;
	int16_t ds = instruction & 0xFFFC;

	uint32_t ea;
	if (!ra)
		ea = (int32_t)ds;
	else
		ea = state.regs[ra] + ds;
	
	Memory::Write32(ea, state.fr[frt].u);

	printf("stfs fr%d, %d(r%d)\n", frt, ds, ra);
}

void CPUThread::stfd(uint32_t instruction)
{
	uint8_t frt = (instruction >> 21) & 0x1F;
	uint8_t ra = (instruction >> 16) & 0x1F;
	int16_t ds = instruction & 0xFFFC;

	uint32_t ea;
	if (!ra)
		ea = (int32_t)ds;
	else
		ea = state.regs[ra] + ds;
	
	Memory::Write64(ea, state.fr[frt].u);

	printf("stfd fr%d, %d(r%d)\n", frt, ds, ra);
}

void CPUThread::ld(uint32_t instruction)
{
	uint8_t rt = (instruction >> 21) & 0x1F;
	uint8_t ra = (instruction >> 16) & 0x1F;
	int16_t ds = instruction & 0xFFFC;
	uint8_t update = (instruction & 1);

	uint32_t ea;
	if (!ra)
		ea = (int32_t)ds;
	else
		ea = state.regs[ra] + ds;
	
	state.regs[rt] = Memory::Read64(ea);

	printf("ld%s r%d, %d(r%d)\n", update ? "u" : "", rt, ds, ra);

	if (update)
		state.regs[ra] = ea;
}

void CPUThread::fmuls(uint32_t instruction)
{
	uint8_t frt = (instruction >> 21) & 0x1F;
	uint8_t fra = (instruction >> 16) & 0x1F;
	uint8_t frc = (instruction >> 6) & 0x1F;
	bool rc = instruction & 1;

	state.fr[frt].d = (double)((float)state.fr[fra].d * (float)state.fr[frc].d);

	if (rc)
		state.UpdateCRn<float>(state.fr[frt].d, 0, 0);
	
	printf("fmuls f%d,f%d,f%d\n", frt, fra, frc);
}

void CPUThread::std(uint32_t instruction)
{
	int16_t ds = (int16_t)(instruction & 0xFFFC);
	uint8_t ra = (instruction >> 16) & 0x1F;
	uint8_t rs = (instruction >> 21) & 0x1F;
	uint8_t update = (instruction & 1);

	uint32_t ea = 0;
	if (ra == 0)
		ea = ds;
	else
		ea = state.regs[ra] + ds;
	
	printf("std%s r%d, %d(r%d)\n", update ? "u" : "", rs, ds, ra);

	Memory::Write64(ea, state.regs[rs]);

	if (update)
		state.regs[ra] = ea;
}

void CPUThread::fcmpu(uint32_t instruction)
{
	uint8_t bf = (instruction >> 23) & 0x7;
	uint8_t fra = (instruction >> 16) & 0x1F;
	uint8_t frb = (instruction >> 11) & 0x1F;

	double& a = state.fr[fra].d;
	double& b = state.fr[frb].d;

	if (std::isnan(a) || std::isnan(b))
		state.SetCR(0, 1);
	else
		state.UpdateCRn<double>(a, b, 0);
	
	printf("fcmpu cr%d,f%d,f%d\n", bf, fra, frb);
}

void CPUThread::frsp(uint32_t instruction)
{
	uint8_t frt = (instruction >> 21) & 0x1F;
	uint8_t frb = (instruction >> 11) & 0x1F;

	state.fr[frt].d = (double)(float)state.fr[frb].d;

	printf("frsp f%d,f%d\n", frt, frb);
}

void CPUThread::fdiv(uint32_t instruction)
{
	uint8_t frt = (instruction >> 21) & 0x1F;
	uint8_t fra = (instruction >> 16) & 0x1F;
	uint8_t frb = (instruction >> 11) & 0x1F;

	state.fr[frt].d = state.fr[fra].d / state.fr[frb].d;

	printf("fdiv f%d,f%d,f%d\n", frt, fra, frb);
}

void CPUThread::fsqrt(uint32_t instruction)
{
	uint8_t frt = (instruction >> 21) & 0x1F;
	uint8_t frb = (instruction >> 11) & 0x1F;

	state.fr[frt].d = sqrt(state.fr[frb].d);

	printf("fsqrt f%d,f%d\n", frt, frb);
}

void CPUThread::fmul(uint32_t instruction)
{
	uint8_t frt = (instruction >> 21) & 0x1F;
	uint8_t frb = (instruction >> 11) & 0x1F;
	uint8_t frc = (instruction >> 6) & 0x1F;

	state.fr[frt].d = state.fr[frb].d * state.fr[frc].d;

	printf("fmul f%d,f%d,f%d\n", frt, frb, frc);
}

void CPUThread::fctid(uint32_t instruction)
{
	uint8_t frt = (instruction >> 21) & 0x1F;
	uint8_t frb = (instruction >> 11) & 0x1F;

	double d = state.fr[frb].d;
	if (std::isnan(d))
	{
		state.fr[frt].u = 0x8000000000000000u;
	}
	else
	{
		state.fr[frt].u = (int64_t)d;
	}

	printf("fctid f%d,f%d (0x%08lx)\n", frt, frb);
}

void CPUThread::fcfid(uint32_t instruction)
{
	uint8_t frt = (instruction >> 21) & 0x1F;
	uint8_t frb = (instruction >> 11) & 0x1F;

	uint64_t u = state.fr[frb].u;

	state.fr[frt].d = (double)(int64_t)u;

	printf("fcfid f%d,f%d (%f)\n", frt, frb, state.fr[frt].d);
}
