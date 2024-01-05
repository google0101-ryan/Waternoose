#include "VFS.h"

#include <filesystem>
#include <unordered_map>

std::string rootPath;

bool isPathValid(std::string path)
{
	const char* p = path.c_str();

	if (*p == '/')
		return false;

	while (*p)
	{
		if (*p == '.' && *(p+1) == '.')
			return false;
		if (*p == '/' && *(p+1) == '/')
			return false;
		p++;
	}
	return true;
}

void VFS::SetRootDirectory(std::string rootDir)
{
	// This must be absolute, or else relative to the current directory
	if (!isPathValid(rootDir))
		printf("Tried to set root directory to invalid path \"%s\"\n", rootDir.c_str());

	rootPath = rootDir;

	if (!std::filesystem::exists(rootPath))
		std::filesystem::create_directories(rootPath);
}

std::unordered_map<std::string, std::string> mountPoints;

void VFS::MountDirectory(std::string devicePath, std::string mntPath)
{
	if (!isPathValid(mntPath))
		printf("Invalid path: \"%s\"\n", mntPath.c_str());
	
	mountPoints[devicePath] = mntPath;

	if (!std::filesystem::exists(rootPath + "/" + mntPath))
		std::filesystem::create_directory(rootPath + "/" + mntPath);
}
