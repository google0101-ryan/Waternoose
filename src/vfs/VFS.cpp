#include "VFS.h"

#include <filesystem>
#include <unordered_map>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <vector>

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

void ToLowercase(std::string& str)
{
	std::transform(str.begin(), str.end(), str.begin(), [](unsigned char c) {return std::tolower(c);});
}

void BackslashToForwad(std::string& str)
{
	for (auto& c : str)
	{
		if (c == '\\')
			c = '/';
	}
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

struct MountPoint
{
	std::string mp, path;
};

std::vector<MountPoint> mountPoints;

void VFS::MountDirectory(std::string devicePath, std::string mntPath)
{
	if (!isPathValid(mntPath))
		printf("Invalid path: \"%s\"\n", mntPath.c_str());

	MountPoint mp;
	mp.mp = devicePath;
	mp.path = mntPath;
	
	mountPoints.push_back(mp);

	if (!std::filesystem::exists(rootPath + "/" + mntPath))
		std::filesystem::create_directory(rootPath + "/" + mntPath);
}

bool FindMountpoint(std::string& path)
{
	for (auto& mnt : mountPoints)
	{
		if (!strncmp(mnt.mp.c_str(), path.c_str(), mnt.mp.size()))
		{
			path = path.substr(mnt.mp.size());
			path = rootPath + "/" + mnt.path + path;
			return true;
		}
	}

	return false;
}

std::unordered_map<FileHandle_t, FILE*> openFiles;
FileHandle_t curhandle = 1;

FileHandle_t VFS::OpenFile(std::string path, int openMode)
{
	BackslashToForwad(path);
	if (!FindMountpoint(path))
	{
		printf("Invalid filepath \"%s\"\n", path.c_str());
		return FILE_INVALID_HANDLE;
	}

	std::string perms = "";
	if (openMode & OPENMODE_WRITE)
		perms += "w";
	else if (openMode & OPENMODE_READ)
		perms += "r";
	if (openMode & OPENMODE_BINARY)
		perms += "b";

	FILE* f = fopen(path.c_str(), perms.c_str());
	if (!f)
	{
		printf("Failed to open file \"%s\"\n", path.c_str());
		return FILE_INVALID_HANDLE;
	}
	
	FileHandle_t handle = curhandle++;
	openFiles[handle] = f;

	return handle;
}
