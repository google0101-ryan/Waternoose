#include <memory/memory.h>
#include <util.h>
#include <stddef.h>
#include <sys/mman.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <fstream>
#include <bitset>
#include <loader/xex.h>
#include <cpu/CPU.h>
#include <tmmintrin.h>
#include "memory.h"

extern uint32_t mainThreadStackSize;

std::vector<AllocInfo> allocInfo;

uint8_t** readPages, **writePages;
#define PAGE_SIZE (4*1024)
#define MAX_ADDRESS_SPACE 0xFFFF0000
std::bitset<MAX_ADDRESS_SPACE / PAGE_SIZE> usedPages;

void Memory::Initialize()
{
	readPages = new uint8_t*[MAX_ADDRESS_SPACE / PAGE_SIZE];
	writePages = new uint8_t*[MAX_ADDRESS_SPACE / PAGE_SIZE];
	usedPages.reset();

	for (size_t i = 0; i < 64*1024; i += 4096)
		usedPages[i / 4096] = true;
}

void Memory::Dump()
{
	std::ofstream file("mem.dump");
	for (uint32_t i = mainXexBase; i < mainXexBase+mainXexSize; i += 4)
	{
		uint32_t data = bswap32(Read32(i));
		file.write((char*)&data, 4);
	}
	file.close();

	file.open("stack.dump");
	for (uint32_t i = 0x70000000; i < 0x70000000+mainThreadStackSize; i += 4)
	{
		uint32_t data = bswap32(Read32(i));
		file.write((char*)&data, 4);
	}
	file.close();
}

void *Memory::AllocMemory(uint32_t baseAddress, uint32_t size)
{
	void* ret = mmap((void*)baseAddress, size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);

	if (ret == MAP_FAILED)
	{
		printf("Failed to allocate memory: %s\n", strerror(errno));
		exit(1);
	}

	size = (size + PAGE_SIZE) & ~PAGE_SIZE;

	for (int i = 0; i < size; i += PAGE_SIZE)
	{
		readPages[(baseAddress + i) / PAGE_SIZE] = ((uint8_t*)ret+i);
		writePages[(baseAddress + i) / PAGE_SIZE] = ((uint8_t*)ret+i);
	}

	return ret;
}

uint32_t Memory::VirtAllocMemoryRange(uint32_t beginAddr, uint32_t endAddr, uint32_t size)
{
	uint32_t requiredPages = size / PAGE_SIZE;

	// Find the lowest free range that fits the size
	uint32_t candidate = 0;
	uint32_t freePages = 0;
	for (uint32_t i = beginAddr; i < endAddr; i += PAGE_SIZE)
	{
		if (!candidate && !usedPages[i / PAGE_SIZE])
		{
			candidate = i / PAGE_SIZE;
			freePages = 1;
		}
		else if (!usedPages[i / PAGE_SIZE])
		{
			freePages++;
			if (freePages == requiredPages)
				break;
		}
		else
		{
			candidate = 0;
		}
	}

	if (candidate == 0)
	{
		printf("ERROR: Failed to allocate virtual memory in range [0x%08x -> 0x%08x]\n", beginAddr, endAddr);
		exit(1);
	}

	// Mark the pages as used
	for (int i = 0; i < requiredPages; i++)
		usedPages.set(candidate+i, true);
	
	AllocInfo info;
	info.baseAddress = candidate*PAGE_SIZE;
	info.regionSize = requiredPages*PAGE_SIZE;
	allocInfo.push_back(info);
	
	return candidate*PAGE_SIZE;
}

uint8_t *Memory::GetRawPtrForAddr(uint32_t addr)
{
	if (!readPages[addr / PAGE_SIZE])
	{
		printf("Read raw ptr from unmapped addr 0x%08x\n", addr);
		exit(1);
	}

	return &readPages[addr / PAGE_SIZE][addr % PAGE_SIZE];
}

bool Memory::GetAllocInfo(uint32_t addr, AllocInfo& outInfo)
{
	for (auto& info : allocInfo)
	{
		if (info.baseAddress <= addr && info.baseAddress+info.regionSize > addr)
		{
			outInfo = info;
			return true;
		}
	}
	
	printf("Failed to get allocation info for region 0x%08x\n", addr);
	return false;
}

uint8_t Memory::Read8(uint32_t addr, bool slow)
{
	if (!slow)
	{
		if (!readPages[addr / PAGE_SIZE])
		{
			return Read8(addr, true);
		}

		return readPages[addr / PAGE_SIZE][addr % PAGE_SIZE];
	}
	else
	{
		switch (addr)
		{
		case 0x15A:
			return 0x06;
		default:
			printf("Read8 from unmapped addr 0x%08x\n", addr);
			exit(1);
		}
	}
}

uint16_t Memory::Read16(uint32_t addr, bool slow)
{
	if (!slow)
	{
		if (!readPages[addr / PAGE_SIZE])
		{
			return Read16(addr, true);
		}

		return bswap16(*(uint16_t*)&readPages[addr / PAGE_SIZE][addr % PAGE_SIZE]);
	}
	else
	{
		switch (addr)
		{
		case 0x10158:
			return bswap16(0x2); // Some kind of console type (maybe debug vs retail?). xbdm.xex relies on this while booting
		case 0x1015A:
			return 0; // More xbdm.xex nonsense
		case 0x1015C:
			return bswap16(0x4f80); // According to assert messages inside xbdm, this is the console's firmware revision
		case 0x1015E:
			return 0; // Setting this to 0x8000 will cause a bunch of extra stuff to happen inside xbdm
		case 0x10164:
			// If bit 9 is set, then xbdm will enforce in-order execution of I/O (EIEIO)
			return 0;
		case 0x8e03860a: // Some kind of weird page mapped by the OS
			return 0;
		default:
			printf("Read16 from unmapped address 0x%08x\n", addr);
			exit(1);
		}
	}
}

uint32_t Memory::Read32(uint32_t addr, bool slow)
{
	if (!slow)
	{
		if (!readPages[addr / PAGE_SIZE])
		{
			return Read32(addr, true);
		}

		return bswap32(*(uint32_t*)&readPages[addr / PAGE_SIZE][addr % PAGE_SIZE]);
	}
	else
	{
		static int system_ticks = 0;
		switch (addr)
		{
		case 0x59:
			return 0;
		case 0xbd:
			return bswap32(system_ticks++);
		case 0x156:
			return bswap32(0x20);
		case 0x10156:
			return bswap32(0x2000000); // Setting this to 0x2000000 causes some kind of memory address to be set to 1
		default:
			printf("Read32 from unmapped address 0x%08x\n", addr);
			exit(1);
		}
	}
}

uint64_t Memory::Read64(uint32_t addr)
{
	if (!readPages[addr / PAGE_SIZE])
	{
		printf("Read64 from unmapped addr 0x%08x\n", addr);
		exit(1);
	}

	return bswap64(*(uint64_t*)&readPages[addr / PAGE_SIZE][addr % PAGE_SIZE]);
}

unsigned __int128 swap(unsigned __int128 n)
{
    unsigned __int128 m;
    const uint64_t* src = (const uint64_t*)(&n);
    uint64_t* dest = (uint64_t*)(&m);
    dest[1] = bswap64(src[0]);
    dest[0] = bswap64(src[1]);
    return m;
}

__uint128_t Memory::Read128(uint32_t addr)
{
	if (!readPages[addr / PAGE_SIZE])
	{
		printf("Read128 from unmapped addr 0x%08x\n", addr);
		exit(1);
	}

	__uint128_t t = *(__uint128_t*)&readPages[addr / PAGE_SIZE][addr % PAGE_SIZE];
	return swap(t);
}

void Memory::Write8(uint32_t addr, uint8_t data)
{
	if (!writePages[addr / PAGE_SIZE])
	{
		printf("Write8 to unmapped addr 0x%08x\n", addr);
		exit(1);
	}

	writePages[addr / PAGE_SIZE][addr % PAGE_SIZE] = data;
}

void Memory::Write16(uint32_t addr, uint16_t data)
{
	if (!writePages[addr / PAGE_SIZE])
	{
		printf("Write16 to unmapped addr 0x%08x\n", addr);
		exit(1);
	}

	*(uint16_t*)&writePages[addr / PAGE_SIZE][addr % PAGE_SIZE] = bswap16(data);
}

void Memory::Write32(uint32_t addr, uint32_t data)
{
	if (!writePages[addr / PAGE_SIZE])
	{
		printf("Write32 to unmapped addr 0x%08x\n", addr);
		exit(1);
	}

	*(uint32_t*)&writePages[addr / PAGE_SIZE][addr % PAGE_SIZE] = bswap32(data);
}

void Memory::Write64(uint32_t addr, uint64_t data)
{
	if (!writePages[addr / PAGE_SIZE])
	{
		printf("Write64 to unmapped addr 0x%08x\n", addr);
		exit(1);
	}

	*(uint64_t*)&writePages[addr / PAGE_SIZE][addr % PAGE_SIZE] = bswap64(data);
}

void Memory::Write128(uint32_t addr, __uint128_t data)
{
	if (!writePages[addr / PAGE_SIZE])
	{
		printf("Write64 to unmapped addr 0x%08x\n", addr);
		exit(1);
	}

	data = swap(data);
	*(__uint128_t*)&writePages[addr / PAGE_SIZE][addr % PAGE_SIZE] = data;
}
