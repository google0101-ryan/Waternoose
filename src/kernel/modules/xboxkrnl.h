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
private:
	void DbgPrint(CPUThread& caller); // 0x03
	void ExAllocatePoolWithTag(CPUThread& caller); // 0x0A
	void ExGetXConfigSetting(CPUThread& caller); // 0x10
	void ExRegisterTitleTerminationNotification(CPUThread& caller); // 0x15
	void KeGetCurrentProcessType(CPUThread& caller); // 0x66
	void KeQuerySystemTime(CPUThread& caller); // 0x84
	void NtCreateFile(CPUThread& caller); // 0xd2
	void ObTranslateSymbolicLink(CPUThread& caller); // 0x113
	void RtlInitAnsiString(CPUThread& caller); // 0x12C
	void _snprintf(CPUThread& caller); // 0x13A
	void RltTimeToTimeFields(CPUThread& caller); // 0x140
	void XexGetModuleHandle(CPUThread& caller); // 0x195
	// Warning: This will modify CPU state by calling the new module's entrypoint!
	void XexLoadImage(CPUThread& caller); // 0x199
	void NtAllocateEncryptedMemory(CPUThread& caller); // 0x28A

	uint32_t modHandle; // Set this to some arbitrary (and ridiculous) value to avoid trampling on other xex handles
};

extern XboxKrnlModule krnlModule;