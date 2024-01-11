#pragma once

#include <cstdint>
#include <cstdio>
#include <cstdlib>

#include <types.h>

class XexLoader;

/// @brief Contains most of the CPU state, including registers, SPRs, etc. 
/// We declare this in a struct so the scheduler can access it to save/restore CPU state on a context switch
typedef struct
{
	uint32_t pcr_address;
	uint32_t tls_addr;
	uint32_t tls_lowest_alloced;

	uint64_t pc;
	uint64_t regs[32];
	uint64_t ctr;
	uint64_t lr;
	uint64_t msr;

	union
	{
		uint64_t u;
		double d;
	} fr[32];

	uint128_t vfr[128];
	uint128_t vscr_vec;

	struct
	{
		uint32_t cr0;
		uint32_t cr1;
		uint32_t cr2;
		uint32_t cr3;
		uint32_t cr4;
		uint32_t cr5;
		uint32_t cr6;
		uint32_t cr7;
	} CR;

	uint32_t GetCR(int num)
	{
		switch (num)
		{
		case 0: return CR.cr0;
		case 1: return CR.cr1;
		case 2: return CR.cr2;
		case 3: return CR.cr3;
		case 4: return CR.cr4;
		case 5: return CR.cr5;
		case 6: return CR.cr6;
		case 7: return CR.cr7;
		default:
			printf("Read from unknown CR%d\n", num);
			exit(1);
		}
	}

	void SetCR(int num, uint32_t val)
	{
		switch (num)
		{
		case 0: CR.cr0 = val; break;
		case 1: CR.cr1 = val; break;
		case 2: CR.cr2 = val; break;
		case 3: CR.cr3 = val; break;
		case 4: CR.cr4 = val; break;
		case 5: CR.cr5 = val; break;
		case 6: CR.cr6 = val; break;
		case 7: CR.cr7 = val; break;
		default:
			printf("Write to unknown CR%d\n", num);
			exit(1);
		}
	}

	template<typename T>
	void UpdateCRn(T x, T y, int num)
	{
		if (x < y) SetCR(num, 0x8);
		else if (x > y) SetCR(num, 0x4);
		else SetCR(num, 0x2);
	}

	struct XER
	{
		bool ca;
	} xer;
} cpuState_t;

/// @brief This will represent one of 6 hardware threads running at a time. 
/// Ideally, we'll move these to their own threads and then have the scheduler dispatch guest threads
/// as needed to these CPU threads, 
/// in order to increase performance
class CPUThread
{
public:
	CPUThread(uint32_t entryPoint, uint32_t stackSize, XexLoader& ref);

	void Run();
	void Dump();

	void SetArg(int num, uint64_t value);

	cpuState_t& GetState() {return state;}

	bool DoneRunningEntry() {return state.pc == 0xBCBCBCBC;}
public:
	XexLoader& xexRef;
private:
	void twi(uint32_t instruction); // 3
	void lvx128(uint32_t instruction); // 4 12
	void stvx128(uint32_t instruction); // 4 30
	void vslb(uint32_t instruction); // 4 260
	void vspltb(uint32_t instruction); // 4 524
	void vspltisb(uint32_t instruction); // 4 780
	void vspltish(uint32_t instruction); // 4 844
	void vspltisw128(uint32_t instruction); // 4 119
	void vor(uint32_t instruction); // 4 1156
	void mulli(uint32_t instruction); // 7
	void subfic(uint32_t instruction); // 8
	void cmpli(uint32_t instruction); // 10
	void cmpi(uint32_t instruction); // 11
	void addic(uint32_t instruction); // 12
	void addicx(uint32_t instruction); // 13
	void addi(uint32_t instruction); // 14
	void addis(uint32_t instruction); // 15
	void bc(uint32_t instruction); // 16
	void sc(uint32_t instruction); // 17
	void branch(uint32_t instruction); // 18
	void bclr(uint32_t instruction); // 19 16
	void bctr(uint32_t instruction); // 19 528
	void rlwimi(uint32_t instruction); // 20
	void rlwinm(uint32_t instruction); // 21
	void ori(uint32_t instruction); // 24
	void oris(uint32_t instruction); // 25
	void andi(uint32_t instruction); // 28
	void andis(uint32_t instruction); // 29
	void rldicl(uint32_t instruction); // 30 0
	void rldicr(uint32_t instruction); // 30 1
	void cmp(uint32_t instruction); // 31 0
	void lvsl(uint32_t instruction); // 31 6
	void subfc(uint32_t instruction); // 31 8
	void lwarx(uint32_t instruction); // 31 20
	void lwzx(uint32_t instruction); // 31 23
	void slw(uint32_t instruction); // 31 24
	void cntlzw(uint32_t instruction); // 31 26
	void sld(uint32_t instruction); // 31 27
	void and_(uint32_t instruction); // 31 28
	void cmpl(uint32_t instruction); // 31 32
	void subf(uint32_t instruction); // 31 40
	void andc(uint32_t instruction); // 31 60
	void mfmsr(uint32_t instruction); // 31 83
	void lbzx(uint32_t instruction); // 31 87
	void neg(uint32_t instruction); // 31 104
	void nor(uint32_t instruction); // 31 124
	void subfe(uint32_t instruction); // 31 136
	void stdx(uint32_t instruction); // 31 149
	void stwcx(uint32_t instruction); // 31 150
	void stwx(uint32_t instruction); // 31 151
	void mtmsrd(uint32_t instruction); // 31 178
	void subfze(uint32_t instruction); // 31 200
	void addze(uint32_t instruction); // 31 202
	void mullw(uint32_t instruction); // 31 235
	void add(uint32_t instruction); // 31 266
	void dcbt(uint32_t instruction); // 31 278
	void xor_(uint32_t instruction); // 31 316
	void mfspr(uint32_t instruction); // 31 339
	void mftb(uint32_t instruction); // 31 371
	void sthx(uint32_t instruction); // 31 407
	void or_(uint32_t instruction); // 31 444
	void divwu(uint32_t instruction); // 31 459
	void mtspr(uint32_t instruction); // 31 467
	void divd(uint32_t instruction); // 31 489
	void stvlx(uint32_t instruction); // 31 647
	void stwbrx(uint32_t instruction); // 31 662
	void stvrx(uint32_t instruction); // 31 679
	void srawi(uint32_t instruction); // 31 824
	void dcbz(uint32_t instruction); // 31 1014
	void lwz(uint32_t instruction); // 32
	void lwzu(uint32_t instruction); // 33
	void lbz(uint32_t instruction); // 34
	void lbzu(uint32_t instruction); // 35
	void stw(uint32_t instruction); // 36
	void stwu(uint32_t instruction); // 37
	void stb(uint32_t instruction); // 38
	void stbu(uint32_t instruction); // 39
	void lhz(uint32_t instruction); // 40
	void sth(uint32_t instruction); // 44
	void lfs(uint32_t instruction); // 48
	void lfd(uint32_t instruction); // 50
	void stfs(uint32_t instruction); // 52
	void stfd(uint32_t instruction); // 54
	void ld(uint32_t instruction); // 58
	void fmuls(uint32_t instruction); // 59 25
	void std(uint32_t instruction); // 62
	void fcmpu(uint32_t instruction); // 63 0
	void frsp(uint32_t instruction); // 63 12
	void fdiv(uint32_t instruction); // 63 18
	void fsqrt(uint32_t instruction); // 63 22
	void fmul(uint32_t instruction); // 63 25
	void fctid(uint32_t instruction); // 63 814
	void fcfid(uint32_t instruction); // 63 846
private:
	bool CondPassed(uint8_t bo, uint8_t bi);
private:
	cpuState_t state;
};