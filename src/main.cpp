#include <loader/xex.h>
#include <memory/memory.h>
#include <cpu/CPU.h>
#include <cstdio>
#include <fstream>

int main(int argc, char** argv)
{
	if (argc < 2)
	{
		printf("Usage: %s <xex name>\n", argv[0]);
		return 0;
	}

	char* buf;
	size_t size;

	std::ifstream file(argv[1], std::ios::binary | std::ios::ate);
	size = file.tellg();
	file.seekg(0, std::ios::beg);
	buf = new char[size];
	file.read(buf, size);
	file.close();

	Memory::Initialize();

	std::atexit(Memory::Dump);

	XexLoader loader((uint8_t*)buf, size);
	
	CPUThread mainThread(loader.GetEntryPoint(), loader.GetStackSize());

	while (1)
	{
		mainThread.Run();
	}

	return 0;
}