#pragma once

#include <string>

typedef int FileHandle_t;
#define FILE_INVALID_HANDLE (FileHandle_t)-1

enum FileOpenMode : int
{
	OPENMODE_READ = 1,
	OPENMODE_WRITE = 2,
	OPENMODE_BINARY = 4
};

namespace VFS
{

/// @brief This will set the directory from which all subdirectories will be created
/// @param rootDir The directory where all console files will be stored, such as save data
void SetRootDirectory(std::string rootDir);

/// @brief This will create a directory named `mntPath` under the root directory, and then associate it with `devicePath`. 
/// Example: `MountDirectory('/SystemRoot', 'SystemRoot0');`: This will create the folder 'SystemRoot0' under the root dir. 
/// It then redirects filesystem operations starting with `\\SystemRoot` to this folder 
/// @param devicePath The path of the device. This is an XBOX path
/// @param mntPath The path on the host, relative to the root directory
void MountDirectory(std::string devicePath, std::string mntPath);

FileHandle_t OpenFile(std::string path, int openMode);

}