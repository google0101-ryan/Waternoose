#pragma once

#include <kernel/Module.h>
#include <kernel/kernel.h>

class XboxKrnlModule : public IModule
{
public:
	XboxKrnlModule();
	void Initialize();

	void CallFunctionByOrdinal(uint32_t ordinal, CPUThread& caller);
	virtual uint32_t GetHandle() const {return modHandle;}

	bool IsExportVariable(uint32_t ordinal);
private:
	void DbgPrint(CPUThread& caller); // 0x03
	void ExAllocatePoolWithTag(CPUThread& caller); // 0x0A
	void ExGetXConfigSetting(CPUThread& caller); // 0x10
	void ExInitializeReadWriteLock(CPUThread& caller); // 0x11
	void ExRegisterTitleTerminationNotification(CPUThread& caller); // 0x15
	void KeAcquireSpinLockAtRaisedIrql(CPUThread& caller); // 0x4D
	void KeEnterCriticalRegion(CPUThread& caller); // 0x5F
	void KeGetCurrentProcessType(CPUThread& caller); // 0x66
	void KeInitializeDPC(CPUThread& caller); // 0x6F
	void KeInitializeSemaphore(CPUThread& caller); // 0x74
	void KeInitializeTimerEx(CPUThread& caller); // 0x75
	void KeLeaveCriticalRegion(CPUThread& caller); // 0x7D
	void KeQueryPerformanceCounter(CPUThread& caller); // 0x83
	void KeQuerySystemTime(CPUThread& caller); // 0x84
	void KeRaiseIrqlToDPC(CPUThread& caller); // 0x85
	void KeReleaseSpinLockAtRaisedIrql(CPUThread& caller); // 0x89
	void KfReleaseSpinLock(CPUThread& caller); // 0xb4
	void MmAllocatePhysicalMemory(CPUThread& caller); // 0xba
	void MmQueryAllocationSize(CPUThread& caller); // 0xc5
	void NtAllocateVirtualMemory(CPUThread& caller); // 0xcc
	void NtCreateFile(CPUThread& caller); // 0xd2
	void NtQueryFullAttributesFile(CPUThread& caller); // 0xe7
	void NtQueryVirtualMemory(CPUThread& caller); // 0xee
	void ObTranslateSymbolicLink(CPUThread& caller); // 0x113
	void RtlEnterCriticalSection(CPUThread& caller); // 0x125
	void RtlInitAnsiString(CPUThread& caller); // 0x12C
	void RtlInitializeCriticalSection(CPUThread& caller); // 0x12E
	void RtlLeaveCriticalSection(CPUThread& caller); // 0x130
	void _snprintf(CPUThread& caller); // 0x13A
	void RltTimeToTimeFields(CPUThread& caller); // 0x140
	void KeAllocTLS(CPUThread& caller); // 0x152
	void KeTlsGetValue(CPUThread& caller); // 0x154
	void XexGetModuleHandle(CPUThread& caller); // 0x195
	// Warning: This will modify CPU state by calling the new module's entrypoint!
	void XexLoadImage(CPUThread& caller); // 0x199
	void NtAllocateEncryptedMemory(CPUThread& caller); // 0x28A

	uint32_t modHandle; // Set this to some arbitrary (and ridiculous) value to avoid trampling on other xex handles
};

extern XboxKrnlModule krnlModule;