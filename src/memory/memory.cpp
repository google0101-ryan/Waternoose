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

uint8_t** readPages, **writePages;
#define PAGE_SIZE (4*1024)
#define MAX_ADDRESS_SPACE UINT32_MAX
std::bitset<MAX_ADDRESS_SPACE / PAGE_SIZE> usedPages;

void Memory::Initialize()
{
	readPages = new uint8_t*[MAX_ADDRESS_SPACE / PAGE_SIZE];
	writePages = new uint8_t*[MAX_ADDRESS_SPACE / PAGE_SIZE];
	usedPages.reset();
}

void Memory::Dump()
{
	std::ofstream file("mem.dump");
	for (uint32_t i = 0x82000000; i < 0x82130000; i += 4)
	{
		uint32_t data = bswap32(Read32(i));
		file.write((char*)&data, 4);
	}
	file.close();
}

bool AddrIsUsed(uint32_t addr, uint32_t size)
{
	for (int i = addr; i < addr+size; i += PAGE_SIZE)
	{
		if (usedPages[i / PAGE_SIZE])
		{
			return true;
		}
	}
	return false;
}

void *Memory::AllocMemory(uint32_t baseAddress, uint32_t size)
{
	if (AddrIsUsed(baseAddress, size))
	{
		printf("ERROR: Trying to map used memory!\n");
		exit(1);
	}

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
	for (int i = beginAddr; i < endAddr; i += PAGE_SIZE)
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
	
	return candidate*PAGE_SIZE;
}

uint32_t Memory::Read32(uint32_t addr)
{
	if (!readPages[addr / PAGE_SIZE])
	{
		printf("Read from unmapped addr 0x%08x\n", addr);
		exit(1);
	}

	return bswap32(*(uint32_t*)&readPages[addr / PAGE_SIZE][addr % PAGE_SIZE]);
}

void Memory::Write32(uint32_t addr, uint32_t data)
{
	if (!writePages[addr / PAGE_SIZE])
	{
		printf("Write to unmapped addr 0x%08x\n", addr);
		exit(1);
	}

	*(uint32_t*)&writePages[addr / PAGE_SIZE][addr % PAGE_SIZE] = bswap32(data);
}
