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
private:
	void branch(uint32_t instruction); // 18
	void bclr(uint32_t instruction); // 19 16
	void mfspr(uint32_t instruction); // 32 339
	void stw(uint32_t instruction); // 36
	void std(uint32_t instruction); // 62
private:
	bool CondPassed(uint8_t bo, uint8_t bi);
private:
	cpuState_t state;
};