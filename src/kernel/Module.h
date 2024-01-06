#pragma once

#include <string>
#include <cpu/CPU.h>

class IModule
{
public:
	IModule(const char* name) : name(name) {}
	const std::string& GetName() const {return name;}

	virtual void CallFunctionByOrdinal(uint32_t ordinal, CPUThread& caller) {};
	virtual uint32_t GetHandle() const = 0;
private:
	std::string name;
};