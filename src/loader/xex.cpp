#include "xex.h"
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <cassert>
#include <fstream>
#include <crypto/rijndael-alg-fst.h>
#include <memory/memory.h>
#include <util.h>

// Am I allowed to have this here? 
// Xenia gets away with it, I'm sure it's fine
static const uint8_t xe_xex2_retail_key[16] = {
    0x20, 0xB1, 0x85, 0xA5, 0x9D, 0x28, 0xFD, 0xC3,
    0x40, 0x58, 0x3F, 0xBB, 0x08, 0x96, 0xBF, 0x91};

void aes_decrypt_buffer(const uint8_t* session_key, const uint8_t* input_buffer,
                        const size_t input_size, uint8_t* output_buffer,
                        const size_t output_size)
{
	uint32_t rk[4 * (MAXNR + 1)];
	uint8_t ivec[16] = {0};
	int32_t Nr = rijndaelKeySetupDec(rk, session_key, 128);
	const uint8_t* ct = input_buffer;
	uint8_t* pt = output_buffer;
	for (size_t n = 0; n < input_size; n += 16, ct += 16, pt += 16)
	{
		rijndaelDecrypt(rk, Nr, ct, pt);
		for (size_t i = 0; i < 16; i++)
		{
			pt[i] ^= ivec[i];
			ivec[i] = ct[i];
		}
	}
}

XexLoader::XexLoader(uint8_t *buffer, size_t len)
{
	this->buffer = buffer;

	header = *(xexHeader_t*)buffer;
	header.module_flags = bswap32(header.module_flags);
	header.header_size = bswap32(header.header_size);
	header.sec_info_offset = bswap32(header.sec_info_offset);
	header.optional_header_count = bswap32(header.optional_header_count);

	if (memcmp(header.magic, "XEX2", 4) != 0)
	{
		char magic[5];
		strncpy(magic, header.magic, 4);
		magic[4] = 0;
		printf("ERROR: Invalid magic: Expected \"XEX2\", got \"%s\"\n", magic);
		exit(1);
	}

	printf("%d optional headers found\n", header.optional_header_count);
	printf("PE data is at offset 0x%08x\n", header.header_size);

	std::vector<optionalHeader_t> optHeaders;
	size_t optHeaderSize = header.optional_header_count*sizeof(optionalHeader_t);
	for (size_t i = 0; i < optHeaderSize; i += sizeof(optionalHeader_t))
	{
		size_t offs = i + sizeof(xexHeader_t);
		optionalHeader_t opt = *(optionalHeader_t*)&buffer[offs];
		opt.id = bswap32(opt.id);
		opt.offset = bswap32(opt.offset);
		optHeaders.push_back(opt);
	}

	// Parse security info, including AES key decryption
	uint8_t* aes_key = (buffer+header.sec_info_offset+336);
	aes_decrypt_buffer(xe_xex2_retail_key, aes_key, 16, session_key, 16);

	printf("AES key is 0x%02x", session_key[0]);
	for (int i = 0; i < 15; i++)
	{
		printf("%02x", session_key[i+1]);
	}
	printf("\n");

	for (auto& hdr : optHeaders)
	{
		switch (hdr.id)
		{
		case 0x2ff:
			break;
		case 0x3ff:
		{
			fileInfoOffset = hdr.offset;
			ParseFileInfo(hdr.offset);
			break;
		}
		case 0x10100:
			entryPoint = hdr.value;
			printf("Image entry point is 0x%08x\n", entryPoint);
			break;
		case 0x10201:
			baseAddress = hdr.value;
			printf("Image base is 0x%08x\n", baseAddress);
			break;
		case 0x103FF:
			importBaseAddr = hdr.offset;
			break;
		case 0x20200:
			stackSize = hdr.value;
			printf("Stack size is 0x%08x\n", hdr.value);
			break;
		default:
			printf("Unknown optional header ID: 0x%08x\n", hdr.id);
		}
	}

	// Decrypt/decompress the file
	assert(encryptionFormat == 1 && compressionFormat == 1);
	char* outBuffer;
	auto uncompressedSize = ReadImageBasicCompressed(buffer, len, &outBuffer);

	std::ofstream out("out.pe");
	out.write(outBuffer, uncompressedSize);
	out.close();

	// We've got the PE header inside outBuffer now
	if (*(uint32_t*)outBuffer != 0x00905a4d /*"PE" followed by 0x9000*/)
	{
		printf("Invalid PE magic\n");
		exit(1);
	}

	printf("Found valid PE header file\n");

	void* base = Memory::AllocMemory(baseAddress, uncompressedSize);
	memcpy(base, outBuffer, uncompressedSize);

	// Now we can patch module calls
	// xam.xex and xboxkrnl.exe are the two most common imports afaict
	importHeader_t importHdr = *(importHeader_t*)&buffer[importBaseAddr];
	importHdr.size = bswap32(importHdr.size);
	importHdr.stringTable.count = bswap32(importHdr.stringTable.count);
	importHdr.stringTable.size = bswap32(importHdr.stringTable.size);

	std::vector<std::string> importNames;
	for (size_t i = 0, j = 0; i < importHdr.stringTable.size && j < importHdr.stringTable.count; j++)
	{
		const char* string = (const char*)&buffer[importBaseAddr+sizeof(importHeader_t)+i];
		importNames.push_back(std::string(string));

		i += strlen(string) + 1;
		if ((i % 4) != 0)
			i += 4 - (i % 4);
		
		printf("Found import \"%s\"\n", string);
	}

	uint32_t libraryoffs = importHdr.stringTable.size + 12;
	while (libraryoffs < importHdr.size)
	{
		libraryHeader_t libHdr = *(libraryHeader_t*)&buffer[importBaseAddr + libraryoffs];
		
		libHdr.count = bswap16(libHdr.count);
		libHdr.id = bswap32(libHdr.id);
		libHdr.name_index = bswap16(libHdr.name_index);
		libHdr.size = bswap32(libHdr.size);
		libHdr.version_min_value = bswap32(libHdr.version_min_value);
		libHdr.version_value = bswap32(libHdr.version_value);
		
		xexLibrary_t lib;
		lib.header = libHdr;
		lib.name = importNames[libHdr.name_index];
		printf("Parsing imports for \"%s\"\n", lib.name.c_str());
		
		ParseLibraryInfo(importBaseAddr+libraryoffs+sizeof(libraryHeader_t), lib, libraries.size());

		libraries.push_back(lib);

		libraryoffs += libHdr.size;
	}
}

uint32_t XexLoader::GetEntryPoint() const
{
	return entryPoint;
}

uint32_t XexLoader::GetStackSize() const
{
	return stackSize;
}

void XexLoader::ParseFileInfo(uint32_t offset)
{
	fileFormatInfo_t fileInfo = *(fileFormatInfo_t*)&buffer[offset];
	fileInfo.info_size = bswap32(fileInfo.info_size);
	fileInfo.compression_type = bswap16(fileInfo.compression_type);
	fileInfo.encryption_type = bswap16(fileInfo.encryption_type);

	printf("Found file info optional header: %d bytes, compression of type %d, encryption of type %d\n", fileInfo.info_size, fileInfo.compression_type, fileInfo.encryption_type);

	compressionFormat = fileInfo.compression_type;
	encryptionFormat = fileInfo.encryption_type;
	info = fileInfo;
}

void XexLoader::ParseLibraryInfo(uint32_t offset, xexLibrary_t &lib, int index)
{
	for (uint32_t i = 0; i < lib.header.count; i++)
	{
		uint32_t recordAddr = bswap32(*(uint32_t*)&buffer[offset]);
		offset += 4;

		uint32_t record = Memory::Read32(recordAddr);

		// Write the following routine to RAM:
		// li r11, mod_func_id
		// sc 2
		// blr
		// nop
		if ((record >> 24) == 1)
		{
			Memory::Write32(recordAddr+0x00, 0x39600000 | (index << 12) | (record & 0xFFFF));
			Memory::Write32(recordAddr+0x04, 0x44000042);
			Memory::Write32(recordAddr+0x08, 0x4e800020);
			Memory::Write32(recordAddr+0x0C, 0x60000000);
		}
	}
}

int XexLoader::ReadImageBasicCompressed(uint8_t *buffer, size_t xex_len, char** outBuffer)
{
	const uint8_t* p = buffer+header.header_size;
	std::vector<basicCompression_t> blocks((info.info_size - 8) / 8);
	uint32_t uncompressedSize = 0;
	for (size_t i = 0; i < (info.info_size - 8) / 8; i++)
	{
		uint32_t offset = fileInfoOffset + 8 + (i * 8);
		basicCompression_t comp = *(basicCompression_t*)&buffer[offset];
		comp.data_size = bswap32(comp.data_size);
		comp.zero_size = bswap32(comp.zero_size);
		blocks.push_back(comp);
		uncompressedSize += comp.data_size + comp.zero_size;
	}

	printf("Image is %d bytes uncompressed\n", uncompressedSize);

	char* out = new char[uncompressedSize];
	*outBuffer = out;
	uint8_t* d = (uint8_t*)out;

	uint32_t rk[4 * (MAXNR + 1)];
	uint8_t ivec[16] = {0};
	int32_t Nr = rijndaelKeySetupDec(rk, session_key, 128);

	for (size_t n = 0; n < blocks.size(); n++)
	{
		const uint32_t dataSize = blocks[n].data_size;
		const uint32_t zeroSize = blocks[n].zero_size;

		const uint8_t* ct = p;
		uint8_t* pt = d;
		for (size_t m = 0; m < dataSize; m += 16, ct += 16, pt += 16)
		{
			rijndaelDecrypt(rk, Nr, ct, pt);
			for (size_t i = 0; i < 16; i++)
			{
				pt[i] ^= ivec[i];
				ivec[i] = ct[i];
			}
		}

		p += dataSize;
		d += dataSize + zeroSize;
	}

	return uncompressedSize;
}
