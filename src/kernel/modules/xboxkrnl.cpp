#include "xboxkrnl.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory/memory.h>
#include <assert.h>

XboxKrnlModule krnlModule;

int arg_index = 0;
uint64_t GetNextArg(cpuState_t& state)
{
	if (arg_index <= 5)
	{
		return state.regs[3+arg_index++];
	}

	printf("TODO: Stack args\n");
	exit(1);
}

XboxKrnlModule::XboxKrnlModule()
: IModule("xboxkrnl.exe")
{
}

void XboxKrnlModule::Initialize()
{
	Kernel::RegisterModuleForName(GetName().c_str(), this);
}

void XboxKrnlModule::CallFunctionByOrdinal(uint32_t ordinal, CPUThread &caller)
{
	switch (ordinal)
	{
	case 0x15:
		ExRegisterTitleTerminationNotification(caller);
		return;
	case 0x66:
		KeGetCurrentProcessType(caller);
		return;
	case 0xd2:
		NtCreateFile(caller);
		return;
	case 0x113:
		ObTranslateSymbolicLink(caller);
		return;
	case 0x12C:
		RtlInitAnsiString(caller);
		return;
	case 0x13A:
		_snprintf(caller);
		return;
	case 0x28A:
		NtAllocateEncryptedMemory(caller);
		return;
	}

	printf("Unknown xboxkrnl.exe function called with ordinal 0x%08x\n", ordinal);
	exit(1);
}

void XboxKrnlModule::ExRegisterTitleTerminationNotification(CPUThread &caller)
{
	uint32_t terminationStructPtr = caller.GetState().regs[3];
	uint32_t create = caller.GetState().regs[4];

	if (create)
	{
		// Add notification to the kernel
	}
	else
	{
		// Remove notification from the kernel
	}

	printf("ExRegisterTitleTerminationNotification(0x%08x, %d)\n", terminationStructPtr, create);
}

void XboxKrnlModule::KeGetCurrentProcessType(CPUThread &caller)
{
	printf("KeGetCurrentProcessType()\n");
	caller.GetState().regs[3] = 2;
}

void XboxKrnlModule::NtCreateFile(CPUThread &caller)
{
	arg_index = 0;
	uint32_t handleOut = GetNextArg(caller.GetState());
	uint32_t desiredAccess = GetNextArg(caller.GetState());
	uint32_t objectAttrsPtr = GetNextArg(caller.GetState());
	uint32_t ioStatusPtr = GetNextArg(caller.GetState());
	uint32_t allocSizeptr = GetNextArg(caller.GetState());

	assert(allocSizeptr == 0);

	if (!objectAttrsPtr)
	{
		caller.GetState().regs[3] = 0xC000000D;
		return;
	}

	uint32_t namePtr = Memory::Read32(objectAttrsPtr+4);
	const char* path = (char*)Memory::GetRawPtrForAddr(Memory::Read32(namePtr+4));

	printf("Opening file \"%s\"\n", path);
	caller.GetState().regs[3] = (uint32_t)-1U;
}

void XboxKrnlModule::ObTranslateSymbolicLink(CPUThread& caller)
{
	printf("TODO: ObTranslateSymbolicLink\n");
	caller.GetState().regs[3] = (uint64_t)-1U;
}

void XboxKrnlModule::RtlInitAnsiString(CPUThread &caller)
{
	// WIN32 STRING struct has the following layout:
	// USHORT Length;
	// USHORT MaximumLength;
	// PCHAR Buffer;

	arg_index = 0;
	
	uint32_t dstPtr = GetNextArg(caller.GetState());
	uint32_t strPtr = GetNextArg(caller.GetState());

	if (strPtr)
	{
		const char* str = (const char*)Memory::GetRawPtrForAddr(strPtr);
		uint16_t len = (uint16_t)strlen(str);
		Memory::Write16(dstPtr+0, len);
		Memory::Write16(dstPtr+2, len+1);
		printf("RtlInitAnsiString(0x%08x, \"%s\" (0x%08x))\n", dstPtr, str, strPtr);
	}
	else
	{
		Memory::Write16(dstPtr+0, 0);
		Memory::Write16(dstPtr+2, 0);
		printf("RtlInitAnsiString(0x%08x, NULL\n", dstPtr);
	}
	Memory::Write32(dstPtr+4, strPtr);
}

void XboxKrnlModule::_snprintf(CPUThread &caller)
{
	uint32_t dstPtr = caller.GetState().regs[3];
	uint32_t dstMaxLen = caller.GetState().regs[4];
	uint32_t sourceBuffer = caller.GetState().regs[5];
	
	// So, this is a bit complicated
	// That's because of the whole va_args thing
	char* source = (char*)Memory::GetRawPtrForAddr(sourceBuffer);
	char* dest = (char*)Memory::GetRawPtrForAddr(dstPtr);
	source[dstMaxLen+1] = 0;
	
	printf("snprintf(0x%08x, %d, 0x%08x, ...)\n", dstPtr, dstMaxLen, sourceBuffer);
	printf("(%s)\n", source);

	bool exitFmt = false;
	bool is_short = false;

	arg_index = 3;

	char* p = source, *d = dest;
	while (*p && (d - dest) < dstMaxLen)
	{
		size_t remaining = dstMaxLen - (d - dest);
		switch (*p)
		{
		case '%':
		{
			exitFmt = false;
			while (!exitFmt)
			{
				p++;
				switch (*p)
				{
				case 'h':
					is_short = true;
					break;
				case 's':
				{
					uint32_t strAddr = GetNextArg(caller.GetState());
					char* str = (char*)Memory::GetRawPtrForAddr(strAddr);
					size_t len = strlen(str);
					strncpy(d, str, len < remaining ? len : remaining);
					d += len < remaining ? len : remaining;
					exitFmt = true;
					break;
				}
				default:
					printf("Unknown format character %c\n", *p);
					exit(1);
				}
			}
			break;
		}
		default:
			*d = *p;
			p++;
			d++;
			break;
		}
	}
}

void XboxKrnlModule::NtAllocateEncryptedMemory(CPUThread &caller)
{
	uint32_t region_size = caller.GetState().regs[4];
	uint32_t out_addr_ptr = caller.GetState().regs[5];

	uint32_t addr = Memory::VirtAllocMemoryRange(0x8C000000, 0x8FFFFFFF, region_size);
	Memory::AllocMemory(addr, region_size);

	Memory::Write32(out_addr_ptr, addr);

	printf("NtAllocateEncryptedMemory(0x%08lx, 0x%08x, 0x%08x)\n", caller.GetState().regs[3], region_size, out_addr_ptr);

	caller.GetState().regs[3] = 0;
}
