#pragma once

#include <stdint.h>
#include <stddef.h>
#include <vector>
#include <string>

/// @brief The header of a .xex file
typedef struct
{
	char magic[4]; // "XEX2"
	uint32_t module_flags;
	uint32_t header_size;
	uint32_t rsv0;
	uint32_t sec_info_offset;
	uint32_t optional_header_count;
} xexHeader_t;

/// @brief Following the XEX header are `xexHeader_t::optional_header_count` optional headers, which either contain an offset to a struct, or a value
typedef struct
{
	uint32_t id;
	union
	{
		uint32_t offset;
		uint32_t value;
	};
}  optionalHeader_t;

typedef struct
{
	uint32_t info_size;
	uint16_t encryption_type;
	uint16_t compression_type;
} fileFormatInfo_t;

typedef struct
{
	uint32_t data_size;
	uint32_t zero_size;
} basicCompression_t;

typedef struct
{
	uint32_t blockSize;
	uint8_t blockHash[20];
} normalCompressionBlock_t;

typedef struct
{
	uint32_t windowSize;
	normalCompressionBlock_t firstBlock;
} normalCompressionHeader_t;

typedef struct
{
	uint32_t size;
	struct
	{
		uint32_t size;
		uint32_t count;
	} stringTable;
} importHeader_t;

typedef struct
{
	union
	{
		uint32_t value;
		struct
		{
			uint32_t info : 4;
			uint32_t page_count : 28;
		};
	};
	char data_digest[0x14];
} pageDescriptor_t;

typedef struct
{
	uint32_t size;
	char next_import_digest[0x14];
	uint32_t id;
	uint32_t version_value;
	uint32_t version_min_value;
	uint16_t name_index;
	uint16_t count;
} libraryHeader_t;

typedef struct
{
	libraryHeader_t header;
	std::vector<uint32_t> imports;
	std::string name;
} xexLibrary_t;

extern uint32_t mainXexBase, mainXexSize;

class XexLoader
{
public:
	/// @brief Loads a .xex file from a buffer into memory
	/// @param buffer The contents of the .xex file, loaded into memory
	/// @param len The size, in bytes, of the file buffer
	XexLoader(uint8_t* buffer, size_t len);

	uint32_t GetEntryPoint() const;
	uint32_t GetStackSize() const;

	/// @brief Get the list of libraries imported by the .xex file
	const std::vector<xexLibrary_t>& GetLibraries() const {return libraries;}
	size_t GetLibraryIndexByName(const char* name) const;
private:
	void ParseFileInfo(uint32_t offset);
	void ParseLibraryInfo(uint32_t offset, xexLibrary_t& lib, int index);

	int ReadImageBasicCompressed(uint8_t* buffer, size_t xex_len, char** outBuffer);
	int ReadImageCompressed(uint8_t* buffer, size_t xex_len, char** outBuffer);

	xexHeader_t header;

	uint8_t* buffer;
	uint8_t session_key[16];

	uint16_t compressionFormat, encryptionFormat;
	uint32_t fileInfoOffset = 0;
	fileFormatInfo_t info;

	uint32_t baseAddress;
	uint32_t entryPoint;
	uint32_t stackSize = 1024*1024;

	uint32_t importBaseAddr;
	
	std::vector<xexLibrary_t> libraries;

	uint32_t image_size();
};