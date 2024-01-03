#pragma once

#include <cstdint>
#include <cstdio>
#include <cstdlib>

#include <types.h>

/// @brief Contains most of the CPU state, including registers, SPRs, etc. 
/// We declare this in a struct so the scheduler can access it to save/restore CPU state on a context switch
typedef struct
{
	uint64_t pc;
	uint64_t regs[32]; // These are mostly accessed as single uint32_t registers
	uint64_t ctr;
	uint64_t lr;

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
} cpuState_t;

/// @brief This will represent one of 6 hardware threads running at a time. 
/// Ideally, we'll move these to their own threads and then have the scheduler dispatch guest threads
/// as needed to these CPU threads, 
/// in order to increase performance
class CPUThread
{
public:
	CPUThread(uint32_t entryPoint, uint32_t stackSize);

	void Run();
	void Dump();

	void SetArg(int num, uint64_t value);
private:
	void cmpli(uint32_t instruction); // 10
	void addi(uint32_t instruction); // 14
	void addis(uint32_t instruction); // 15
	void bc(uint32_t instruction); // 16
	void branch(uint32_t instruction); // 18
	void bclr(uint32_t instruction); // 19 16
	void rlwinm(uint32_t instruction); // 21
	void ori(uint32_t instruction); // 24
	void mfspr(uint32_t instruction); // 31 339
	void or_(uint32_t instruction); // 31 444
	void mtspr(uint32_t instruction); // 31 467
	void lwz(uint32_t instruction); // 32
	void stw(uint32_t instruction); // 36
	void stwu(uint32_t instruction); // 37
	void ld(uint32_t instruction); // 58
	void std(uint32_t instruction); // 62
private:
	bool CondPassed(uint8_t bo, uint8_t bi);
private:
	cpuState_t state;
};