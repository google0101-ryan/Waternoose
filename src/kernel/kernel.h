#pragma once

class IModule;

namespace Kernel
{

void RegisterModuleForName(const char* name, IModule* mod);
IModule* GetModuleByName(const char* name);

}