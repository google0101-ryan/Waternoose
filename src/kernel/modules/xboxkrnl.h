#pragma once

#include <kernel/Module.h>
#include <kernel/kernel.h>

class XboxKrnlModule : public IModule
{
public:
	XboxKrnlModule();
	void Initialize();

	void CallFunctionByOrdinal(uint32_t ordinal, CPUThread& caller);
private:
	void ExRegisterTitleTerminationNotification(CPUThread& caller); // 0x15
	void KeGetCurrentProcessType(CPUThread& caller); // 0x66
	void NtCreateFile(CPUThread& caller); // 0xd2
	void ObTranslateSymbolicLink(CPUThread& caller); // 0x113
	void RtlInitAnsiString(CPUThread& caller); // 0x12C
	void _snprintf(CPUThread& caller); // 0x13A
	void NtAllocateEncryptedMemory(CPUThread& caller); // 0x28A
};

extern XboxKrnlModule krnlModule;