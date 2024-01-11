#include "xboxkrnl.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <assert.h>
#include <ctime>
#include <sys/time.h>
#include <time.h>
#include <fstream>
#include <vfs/VFS.h>

#include <memory/memory.h>
#include <loader/xex.h>

#define KE_UNIMPLEMENTED(x) printf("WARN: Call to unimplemented/unknown function " x "\n"); return;

long long timeInMilliseconds(void) {
    struct timeval tv;

    gettimeofday(&tv,NULL);
    return (((long long)tv.tv_sec)*1000)+(tv.tv_usec/1000);
}

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
	modHandle = 0x10000000; // One below the lowest XEX module handle, to avoid conflicts
}

void XboxKrnlModule::CallFunctionByOrdinal(uint32_t ordinal, CPUThread &caller)
{
	switch (ordinal)
	{
	case 0x03:
		DbgPrint(caller);
		return;
	case 0x0A:
		ExAllocatePoolWithTag(caller);
		return;
	case 0x10:
		ExGetXConfigSetting(caller);
		return;
	case 0x11:
		ExInitializeReadWriteLock(caller);
		return;
	case 0x15:
		ExRegisterTitleTerminationNotification(caller);
		return;
	case 0x4D:
		KeAcquireSpinLockAtRaisedIrql(caller);
		return;
	case 0x5F:
		KeEnterCriticalRegion(caller);
		return;
	case 0x66:
		KeGetCurrentProcessType(caller);
		return;
	case 0x6F:
		KeInitializeDPC(caller);
		return;
	case 0x74:
		KeInitializeSemaphore(caller);
		return;
	case 0x75:
		KeInitializeTimerEx(caller);
		return;
	case 0x7D:
		KeLeaveCriticalRegion(caller);
		return;
	case 0x83:
		KeQueryPerformanceCounter(caller);
		return;
	case 0x84:
		KeQuerySystemTime(caller);
		return;
	case 0x85:
		KeRaiseIrqlToDPC(caller);
		return;
	case 0x89:
		KeLeaveCriticalRegion(caller);
		return;
	case 0xB4:
		KfReleaseSpinLock(caller);
		return;
	case 0xBA:
		MmAllocatePhysicalMemory(caller);
		return;
	case 0xc5:
		MmQueryAllocationSize(caller);
		return;
	case 0xcc:
		NtAllocateVirtualMemory(caller);
		return;
	case 0xd2:
		NtCreateFile(caller);
		return;
	case 0xe7:
		NtQueryFullAttributesFile(caller);
		return;
	case 0xee:
		NtQueryVirtualMemory(caller);
		return;
	case 0x113:
		ObTranslateSymbolicLink(caller);
		return;
	case 0x125:
		RtlEnterCriticalSection(caller);
		return;
	case 0x12C:
		RtlInitAnsiString(caller);
		return;
	case 0x12E:
		RtlInitializeCriticalSection(caller);
		return;
	case 0x130:
		RtlLeaveCriticalSection(caller);
		return;
	case 0x13A:
		_snprintf(caller);
		return;
	case 0x140:
		RltTimeToTimeFields(caller);
		return;
	case 0x152:
		KeAllocTLS(caller);
		return;
	case 0x154:
		KeTlsGetValue(caller);
		return;
	case 0x195:
		XexGetModuleHandle(caller);
		return;
	case 0x199:
		XexLoadImage(caller);
		return;
	case 0x1AA:
		KE_UNIMPLEMENTED("ExDebugMonitorServices");
	case 0x20D:
		KE_UNIMPLEMENTED("DrvSetUserBindingCallback");
	case 0x28F:
		KE_UNIMPLEMENTED("DrvSetDeviceConfigChangeCallback");
	case 0x328:
		KE_UNIMPLEMENTED("DrvSetMicArrayStartCallback");
	case 0x35B:
		KE_UNIMPLEMENTED("DrvSetAudioLatencyCallback");
	case 0x386:
		KE_UNIMPLEMENTED("XInputdSetFailedConnectionOrBindCallback");
	case 0x28A:
		NtAllocateEncryptedMemory(caller);
		return;
	case 0x334:
		KE_UNIMPLEMENTED("EtxProducerRegister");
	}

	printf("Unknown xboxkrnl.exe function called with ordinal 0x%08x\n", ordinal);
	exit(1);
}

std::ofstream debug_log;

bool XboxKrnlModule::IsExportVariable(uint32_t ordinal)
{
	switch (ordinal)
	{
	case 0x0000000C:
	case 0x0000000E:
	case 0x00000012:
	case 0x00000017:
	case 0x0000001B:
	case 0x0000001C:
	case 0x00000036:
	case 0x0000003A:
	case 0x0000003E:
	case 0x00000059:
	case 0x000000AD:
	case 0x000000B5:
	case 0x000000CB:
	case 0x00000106:
	case 0x00000112:
	case 0x00000156:
	case 0x00000157:
	case 0x00000158:
	case 0x00000193:
	case 0x000001AE:
	case 0x000001AF:
	case 0x000001BE:
	case 0x000001BF:
	case 0x000001C0:
	case 0x000001C1:
	case 0x0000025C:
	case 0x00000266:
	case 0x0000026B:
	case 0x0000026D:
	case 0x0000026E:
	case 0x000002AB:
	case 0x000002DB:
	case 0x000002DC:
	case 0x000002F1:
	case 0x00000345:
		return true;
	}

	return false;
}

void XboxKrnlModule::DbgPrint(CPUThread &caller)
{
	if (!debug_log.is_open())
		debug_log.open("debug.log");

	arg_index = 0;
	uint32_t srcPtr = GetNextArg(caller.GetState());
	char* fmt = (char*)Memory::GetRawPtrForAddr(srcPtr);

	bool exitFmt = false;
	char fill_char;
	int fill_char_count;
	while (*fmt)
	{
		fill_char = 0;
		fill_char_count = 0;
		switch (*fmt)
		{
		case '%':
		{
			while (!exitFmt)
			{
				fmt++;
				switch (*fmt)
				{
				case '0' ... 'F': if (!fill_char) fill_char = *fmt; else fill_char_count = *fmt - '0'; break;
				case 'x':
				case 'X':
				{
					uint32_t hex = GetNextArg(caller.GetState());
					int digit_count = 0;
					uint32_t num = hex;
					while (num)
					{
						num >>= 4;
						digit_count++;
					}
					for (int i = 0; i < fill_char_count - digit_count; i++)
						debug_log.write(&fill_char, 1);
					char buf[4096];
					int len = snprintf(buf, 4096, *fmt == 'X' ? "%X" : "%x", hex);
					debug_log.write(buf, len);
					exitFmt = true;
					break;
				}
				default:
					printf("Unknown format char '%c'\n", *fmt);
					exit(1);
				}
			}
			fmt++;
			break;
		}
		default:
		{
			char c = *fmt;
			debug_log.write(&c, 1);
			fmt++;
		}
		}
	}
}

void XboxKrnlModule::ExAllocatePoolWithTag(CPUThread &caller)
{
	arg_index = 0;
	uint64_t size = GetNextArg(caller.GetState());
	uint32_t tag = GetNextArg(caller.GetState());

	printf("ExAllocatePoolWithTag(0x%08lx, 0x%08x)\n", size, tag);

	caller.GetState().regs[3] = Memory::VirtAllocMemoryRange(0x3A000000, 0x3FBEFFFF, size);
	Memory::AllocMemory(caller.GetState().regs[3], size);
}

void XboxKrnlModule::ExGetXConfigSetting(CPUThread &caller)
{
	arg_index = 0;
	uint16_t category = GetNextArg(caller.GetState());
	uint16_t setting = GetNextArg(caller.GetState());
	uint32_t bufPtr = GetNextArg(caller.GetState());
	uint16_t bufSize = GetNextArg(caller.GetState());
	uint32_t reqSizePtr = GetNextArg(caller.GetState());

	uint64_t result = 0;

	switch (category)
	{
	case 0xb:
	{
		result = (uint64_t)-1LL;
		break;
	}
	default:
		printf("Invalid category assigned: %d\n", category);
		exit(1);
	}

	caller.GetState().regs[3] = result;

	printf("ExGetXConfigSetting(%d, %d, 0x%08x, %d, 0x%08x)\n", category, setting, bufPtr, bufSize, reqSizePtr);
}

void XboxKrnlModule::ExInitializeReadWriteLock(CPUThread &caller)
{
	uint32_t outPtr = caller.GetState().regs[3];

	Memory::Write32(outPtr+0x00, (uint32_t)-1);
	Memory::Write32(outPtr+0x04, 0);
	Memory::Write32(outPtr+0x08, 0);
	Memory::Write32(outPtr+0x0C, 0);

	printf("ExInitializeReadWriteLock(0x%08x)\n", outPtr);
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

void XboxKrnlModule::KeAcquireSpinLockAtRaisedIrql(CPUThread &caller)
{
	printf("KeAcquireSpinLockAtRaisedIrql(0x%08x)\n", caller.GetState().regs[3]);
}

void XboxKrnlModule::KeEnterCriticalRegion(CPUThread &caller)
{
	printf("KeEnterCriticalRegion()\n");
}

void XboxKrnlModule::KeGetCurrentProcessType(CPUThread &caller)
{
	printf("KeGetCurrentProcessType()\n");
	caller.GetState().regs[3] = 2;
}

void XboxKrnlModule::KeInitializeDPC(CPUThread &caller)
{
	uint32_t dpcPtr = caller.GetState().regs[3];
	uint32_t routine = caller.GetState().regs[4];
	uint32_t context = caller.GetState().regs[5];

	Memory::Write16(dpcPtr+0x00, 19); // type
	Memory::Write8(dpcPtr+0x02, 0); // selected CPU
	Memory::Write8(dpcPtr+0x03, 0); // desired CPU
	Memory::Write32(dpcPtr+0x04, 0); // list_entry.flink
	Memory::Write32(dpcPtr+0x08, 0); // list_entry.blink
	Memory::Write32(dpcPtr+0x0C, routine);
	Memory::Write32(dpcPtr+0x10, context);
	Memory::Write32(dpcPtr+0x14, 0);
	Memory::Write32(dpcPtr+0x18, 0);

	printf("KeInitializeDPC(0x%08x, 0x%08x, 0x%08x)\n", dpcPtr, routine, context);
}

void XboxKrnlModule::KeInitializeSemaphore(CPUThread &caller)
{
	uint32_t ptr = caller.GetState().regs[3];
	uint32_t count = caller.GetState().regs[4];
	uint32_t limit = caller.GetState().regs[5];

	Memory::Write8(ptr+0x00, 5);
	Memory::Write8(ptr+0x01, 0);
	Memory::Write8(ptr+0x02, 0);
	Memory::Write8(ptr+0x03, 0);
	Memory::Write32(ptr+0x04, count);
	Memory::Write32(ptr+0x08, 0);
	Memory::Write32(ptr+0x0C, 0);
	Memory::Write32(ptr+0x10, limit);

	printf("KeInitializeSemaphore(0x%08x,%d,%d)\n", ptr, count, limit);
}

void XboxKrnlModule::KeInitializeTimerEx(CPUThread &caller)
{
	uint32_t timerPtr = caller.GetState().regs[3];
	uint32_t type = caller.GetState().regs[4];
	uint32_t processType = caller.GetState().regs[5];

	Memory::Write8(timerPtr+0x00, type+8);
	Memory::Write8(timerPtr+0x02, processType);
	Memory::Write8(timerPtr+0x03, 0);
	Memory::Write32(timerPtr+0x04, 0);
	// Offset 0x10: KTIMER
	Memory::Write64(timerPtr+0x10, 0);
	Memory::Write32(timerPtr+0x34, 0);

	printf("KeInitializeTimerEx(0x%08x)\n", timerPtr);
}

void XboxKrnlModule::KeLeaveCriticalRegion(CPUThread &caller)
{
	printf("KeLeaveCriticalRegion()\n");
}

void XboxKrnlModule::KeQueryPerformanceCounter(CPUThread &caller)
{
	printf("KeQueryPerformanceCounter()\n");
	caller.GetState().regs[3] = 50000000;
}

void XboxKrnlModule::KeQuerySystemTime(CPUThread &caller)
{
	uint32_t timePtr = caller.GetState().regs[3];
	printf("KeQuerySystemTime(0x%08x)\n", timePtr);
	Memory::Write64(timePtr, time(NULL));
}

void XboxKrnlModule::KeRaiseIrqlToDPC(CPUThread &caller)
{
	uint8_t old_irql = Memory::Read8(caller.GetState().pcr_address+0x18);
	Memory::Write8(caller.GetState().pcr_address+0x18, 2);
	caller.GetState().regs[3] = old_irql;
	printf("KeRaiseIrqlToDPC()\n");
}

void XboxKrnlModule::KeReleaseSpinLockAtRaisedIrql(CPUThread &caller)
{
	printf("KeReleaseSpinLockAtRaisedIrql(0x%08x)\n", caller.GetState().regs[3]);
}

void XboxKrnlModule::KfReleaseSpinLock(CPUThread &caller)
{
	printf("KfReleaseSpinLock(0x%08x)\n", caller.GetState().regs[3]);
	Memory::Write8(caller.GetState().pcr_address+0x18, (uint8_t)caller.GetState().regs[4]);
}

void XboxKrnlModule::MmAllocatePhysicalMemory(CPUThread &caller)
{
	int flags = caller.GetState().regs[3];
	uint32_t region_size = caller.GetState().regs[4];

	uint32_t base = Memory::VirtAllocMemoryRange(0xE0000000, 0xFFD00000, region_size);
	Memory::AllocMemory(base, region_size);

	caller.GetState().regs[3] = base;

	printf("MmAllocatePhysicalMemory(%d, 0x%08x, 0x%x, 0x%08x, 0x%08x, 0x%08x)\n", 
			flags, (uint32_t)caller.GetState().regs[4], 
			(uint32_t)caller.GetState().regs[5], (uint32_t)caller.GetState().regs[6], 
			(uint32_t)caller.GetState().regs[7], (uint32_t)caller.GetState().regs[8]);
}

void XboxKrnlModule::MmQueryAllocationSize(CPUThread &caller)
{
	uint32_t base = caller.GetState().regs[3];
	AllocInfo info;
	uint32_t size = 0;
	if (Memory::GetAllocInfo(base, info))
		size = info.regionSize;
	caller.GetState().regs[3] = size;
	printf("MmQueryAllocationSize(0x%08x)\n", base);
}

// TODO: Make this not suck
void XboxKrnlModule::NtAllocateVirtualMemory(CPUThread &caller)
{
	uint32_t region_size_ptr = caller.GetState().regs[4];
	uint32_t out_addr_ptr = caller.GetState().regs[3];
	uint32_t alloc_type = caller.GetState().regs[5];

	uint32_t region_size = Memory::Read32(region_size_ptr);

	uint32_t addr = Memory::Read32(out_addr_ptr);
	if (!addr)
	{
		if (alloc_type & 0x20000000)
			addr = Memory::VirtAllocMemoryRange(0x7A000000, 0x7EFFFFFF, region_size);
		else
			addr = Memory::VirtAllocMemoryRange(0x20000000, 0x2FFFFFFF, region_size);
	}
	Memory::AllocMemory(addr, region_size);

	Memory::Write32(out_addr_ptr, addr);

	printf("NtAllocateVirtualMemory(0x%08lx, 0x%08x, 0x%08x) = 0x%08x\n", caller.GetState().regs[3], region_size, out_addr_ptr, addr);

	caller.GetState().regs[3] = 0;
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

void XboxKrnlModule::NtQueryFullAttributesFile(CPUThread &caller)
{
	arg_index = 0;
	uint32_t objAttrPtr = GetNextArg(caller.GetState());
	uint32_t openInfo = GetNextArg(caller.GetState());

	uint32_t ansiPtr = Memory::Read32(objAttrPtr+0x4);
	uint32_t namePtr = Memory::Read32(ansiPtr+0x04);

	std::string name = (char*)Memory::GetRawPtrForAddr(namePtr);

	printf("NtQueryFullAttributesFile(\"%s\" (0x%08x), 0x%08x)\n", name.c_str(), objAttrPtr, openInfo);

	FileHandle_t handle = VFS::OpenFile(name, OPENMODE_READ | OPENMODE_BINARY);
	if (handle == FILE_INVALID_HANDLE)
	{
		caller.GetState().regs[3] = 0xC000000FL;
		return;
	}

	assert(0);
}

void XboxKrnlModule::NtQueryVirtualMemory(CPUThread &caller)
{
	arg_index = 0;
	uint32_t base_addr = GetNextArg(caller.GetState());
	uint32_t outBasicInfoPtr = GetNextArg(caller.GetState());

	printf("NtQueryVirtualMemory(0x%08x, 0x%08x)\n", base_addr, outBasicInfoPtr);
	
	AllocInfo info;
	if (!Memory::GetAllocInfo(base_addr, info))
	{
		caller.GetState().regs[3] = 0xC000000DL;
		return;
	}

	Memory::Write32(outBasicInfoPtr+0x00, info.baseAddress);
	Memory::Write32(outBasicInfoPtr+0x04, info.baseAddress);
	Memory::Write32(outBasicInfoPtr+0x08, 0x4);
	Memory::Write32(outBasicInfoPtr+0x0C, info.regionSize);
	Memory::Write32(outBasicInfoPtr+0x10, 0x1000);
	Memory::Write32(outBasicInfoPtr+0x14, 0x4);
	Memory::Write32(outBasicInfoPtr+0x18, 0x20000);

	caller.GetState().regs[3] = 0;
}

void XboxKrnlModule::ObTranslateSymbolicLink(CPUThread& caller)
{
	printf("TODO: ObTranslateSymbolicLink\n");
	caller.GetState().regs[3] = (uint64_t)-1U;
}

void XboxKrnlModule::RtlEnterCriticalSection(CPUThread &caller)
{
	uint32_t critPtr = caller.GetState().regs[3];

	if (Memory::Read32(critPtr) != 0)
	{
		// TODO: Recursive locks and waiting on locks
		printf("Critical section already entered!\n");
		// exit(1);
	}

	Memory::Write64(critPtr, 1);
	Memory::Write64(critPtr+0x08, 1);

	caller.GetState().regs[3] = 0;

	printf("RtlEnterCriticalSection(0x%08x)\n", critPtr);
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

void XboxKrnlModule::RtlInitializeCriticalSection(CPUThread &caller)
{
	arg_index = 0;
	uint32_t sectionPtr = GetNextArg(caller.GetState());

	Memory::Write64(sectionPtr+0x00, 0); // LockCount
	Memory::Write64(sectionPtr+0x08, 0); // RecursionCount
	Memory::Write32(sectionPtr+0x0C, 0); // OwningThread
	Memory::Write64(sectionPtr+0x10, 0); // SpinCount

	caller.GetState().regs[3] = 0;

	printf("RtlInitializeCriticalSection(0x%08x)\n", sectionPtr);
}

void XboxKrnlModule::RtlLeaveCriticalSection(CPUThread &caller)
{
	uint32_t critPtr = caller.GetState().regs[3];

	Memory::Write64(critPtr, 0);
	Memory::Write64(critPtr+0x08, 0);

	caller.GetState().regs[3] = 0;

	printf("RtlLeaveCriticalSection(0x%08x)\n", critPtr);
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

void XboxKrnlModule::RltTimeToTimeFields(CPUThread &caller)
{
	arg_index = 0;
	uint32_t timePtr = GetNextArg(caller.GetState());
	time_t time_val = Memory::Read64(timePtr);
	uint32_t outPtr = GetNextArg(caller.GetState());

	tm* t = localtime(&time_val);
	
	Memory::Write16(outPtr, (t->tm_year + 1900) - 1601);
	Memory::Write16(outPtr+2, t->tm_mon);
	Memory::Write16(outPtr+4, t->tm_mday);
	Memory::Write16(outPtr+6, t->tm_hour);
	Memory::Write16(outPtr+8, t->tm_min);
	Memory::Write16(outPtr+10, t->tm_sec);
	Memory::Write16(outPtr+12, timeInMilliseconds() % 999);
	Memory::Write16(outPtr+14, t->tm_wday);

	printf("RltTimeToTimeFields(0x%08x, 0x%08x)\n", timePtr, outPtr);
}

void XboxKrnlModule::KeAllocTLS(CPUThread &caller)
{
	uint32_t addr = caller.GetState().tls_lowest_alloced;
	caller.GetState().tls_lowest_alloced += 0x80;

	printf("KeAllocTLS()\n");

	caller.GetState().regs[3] = addr / 0x80;
}

void XboxKrnlModule::KeTlsGetValue(CPUThread &caller)
{
	uint32_t slot = caller.GetState().regs[3];
	caller.GetState().regs[3] = Memory::Read32(caller.GetState().tls_addr+(slot-1)*0x80);
	printf("KeTlsGetValue(%d)\n", slot);
}

void XboxKrnlModule::XexGetModuleHandle(CPUThread &caller)
{
	arg_index = 0;
	uint32_t namePtr = GetNextArg(caller.GetState());
	uint32_t handlePtr = GetNextArg(caller.GetState());

	std::string name = (char*)Memory::GetRawPtrForAddr(namePtr);

	printf("XexGetModuleHandle(\"%s\", 0x%08x)\n", name.c_str(), handlePtr);

	auto mod = Kernel::GetModuleByName(name.c_str());
	if (!mod)
	{
		printf("No such module with name \"%s\"\n", name.c_str());
		exit(1);
	}

	Memory::Write32(handlePtr, mod->GetHandle());
	caller.GetState().regs[3] = 0;
}

void XboxKrnlModule::XexLoadImage(CPUThread &caller)
{
	auto& cpuState = caller.GetState();
	arg_index = 0;
	uint32_t namePtr = GetNextArg(cpuState);
	uint32_t typeInfo = GetNextArg(cpuState);
	uint32_t version = GetNextArg(cpuState);
	uint32_t handlePtr = GetNextArg(cpuState);

	char* name = (char*)Memory::GetRawPtrForAddr(namePtr);

	printf("Loading module \"%s\"\n", name);

	std::ifstream file(caller.xexRef.GetPath()+"/"+name, std::ios::ate | std::ios::binary);

	if (!file.is_open())
	{
		cpuState.regs[3] = (int64_t)(int32_t)0xC000000FL;
		return;
	}

	size_t size = file.tellg();
	file.seekg(0, std::ios::beg);
	char* buf = new char[size];
	file.read(buf, size);
	
	XexLoader mod = XexLoader((uint8_t*)buf, size, caller.xexRef.GetPath()+"/"+name);

	uint32_t old_lr = cpuState.lr;
	uint32_t old_pc = cpuState.pc;
	auto& old_xex = caller.xexRef;
	cpuState.lr = 0xBCBCBCBC;

	cpuState.regs[3] = mod.GetHandle();
	cpuState.regs[4] = 1;
	cpuState.regs[5] = 0;

	caller.xexRef = mod;

	cpuState.pc = mod.GetEntryPoint();

	while (!caller.DoneRunningEntry())
	{
		caller.Run();
	}

	cpuState.pc = old_pc;
	cpuState.lr = old_lr;
	caller.xexRef = old_xex;
}

void XboxKrnlModule::NtAllocateEncryptedMemory(CPUThread &caller)
{
	uint32_t region_size = caller.GetState().regs[4];
	uint32_t out_addr_ptr = caller.GetState().regs[5];

	uint32_t addr = Memory::VirtAllocMemoryRange(0x8C000000, 0x8FFFFFFF, region_size);
	Memory::AllocMemory(addr, region_size);

	Memory::Write32(out_addr_ptr, addr);

	printf("NtAllocateEncryptedMemory(0x%08lx, 0x%08x, 0x%08x) = 0x%08x\n", caller.GetState().regs[3], region_size, out_addr_ptr, addr);

	caller.GetState().regs[3] = 0;
}
