#include <loader/xex.h>
#include <memory/memory.h>
#include <cpu/CPU.h>
#include <cstdio>
#include <fstream>

CPUThread* mainThread;
uint32_t mainThreadStackSize;

void atexit_handler()
{
	mainThread->Dump();
}

int main(int argc, char** argv)
{
	if (argc < 2)
	{
		printf("Usage: %s <xex name>\n", argv[0]);
		return 0;
	}

	char* xam_buf;
	size_t xam_size;

	std::ifstream file("xam.xex", std::ios::binary | std::ios::ate);
	xam_size = file.tellg();
	file.seekg(0, std::ios::beg);
	xam_buf = new char[xam_size];
	file.read(xam_buf, xam_size);
	file.close();

	char* buf;
	size_t size;

	file.open(argv[1], std::ios::binary | std::ios::ate);
	size = file.tellg();
	file.seekg(0, std::ios::beg);
	buf = new char[size];
	file.read(buf, size);
	file.close();

	Memory::Initialize();

	std::atexit(Memory::Dump);

	XexLoader xam((uint8_t*)xam_buf, xam_size);
	// XexLoader loader((uint8_t*)buf, size);
	
	mainThreadStackSize = xam.GetStackSize();
	mainThread = new CPUThread(xam.GetEntryPoint(), xam.GetStackSize());
	mainThread->SetArg(0, 0xBCBCBCBC);
	mainThread->SetArg(1, 1);
	mainThread->SetArg(2, 0);

	std::atexit(atexit_handler);

	while (1)
	{
	 	mainThread->Run();
	}

	delete mainThread;

	return 0;
}