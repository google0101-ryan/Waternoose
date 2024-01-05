#include <kernel/kernel.h>
#include <kernel/Module.h>
#include <unordered_map>

namespace Kernel
{

std::unordered_map<std::string, IModule*> modules;

void RegisterModuleForName(const char *name, IModule *mod)
{
	modules[name] = mod;
}

IModule *GetModuleByName(const char *name)
{
	return modules[name];
}

}