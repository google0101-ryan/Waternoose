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
#include <loader/lzx.h>
#include <kernel/kernel.h>

static uint32_t handle = 0x10000001;

XexLoader* xam;

uint32_t mainXexBase, mainXexSize;

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

XexLoader::XexLoader(uint8_t *buffer, size_t len, std::string path)
: IModule(path.substr(path.find_last_of('/')+1).c_str())
{
	this->buffer = buffer;
	this->path = path.substr(0, path.find_last_of('/'));
	this->xexHandle = handle++;

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

	mainXexSize = image_size();

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
			mainXexBase = baseAddress;
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
	printf("%d, %d\n", encryptionFormat, compressionFormat);
	char* outBuffer;
	uint32_t uncompressedSize;
	switch (compressionFormat)
	{
	case 1:
		uncompressedSize = ReadImageBasicCompressed(buffer, len, &outBuffer);
		break;
	case 2:
		uncompressedSize = ReadImageCompressed(buffer, len, &outBuffer);
		break;
	default:
		printf("Unknown compression format %d\n", compressionFormat);
		exit(1);
	}

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

	// Load exports
	exportBaseAddr = bswap32(*(uint32_t*)&buffer[header.sec_info_offset+0x160]);
	exportTable.magic[0] = Memory::Read32(exportBaseAddr+0x00);
	exportTable.magic[1] = Memory::Read32(exportBaseAddr+0x04);
	exportTable.magic[2] = Memory::Read32(exportBaseAddr+0x08);
	exportTable.modulenumber[0] = Memory::Read32(exportBaseAddr+0x0C);
	exportTable.modulenumber[1] = Memory::Read32(exportBaseAddr+0x10);
	exportTable.version[0] = Memory::Read32(exportBaseAddr+0x14);
	exportTable.version[1] = Memory::Read32(exportBaseAddr+0x18);
	exportTable.version[2] = Memory::Read32(exportBaseAddr+0x1C);
	exportTable.imagebaseaddr = Memory::Read32(exportBaseAddr+0x20);
	exportTable.count = Memory::Read32(exportBaseAddr+0x24);
	exportTable.base = Memory::Read32(exportBaseAddr+0x28);

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
		
		ParseLibraryInfo(importBaseAddr+libraryoffs+sizeof(libraryHeader_t), lib, libraries.size(), lib.name);

		libraries.push_back(lib);

		libraryoffs += libHdr.size;
	}

	Kernel::RegisterModuleForName(GetName().c_str(), this);
}

uint32_t XexLoader::GetEntryPoint() const
{
	return entryPoint;
}

uint32_t XexLoader::GetStackSize() const
{
	return stackSize;
}

size_t XexLoader::GetLibraryIndexByName(const char *name) const
{
	for (int i = 0; i < libraries.size(); i++)
	{
		auto& lib = libraries[i];
		if (lib.name == name)
			return i;
	}
	return SIZE_MAX;
}

uint32_t XexLoader::LookupOrdinal(uint32_t ordinal)
{
	ordinal -= exportTable.base;
	if (ordinal >= exportTable.count)
	{
		printf("ERROR: Imported unknown function 0x%08x\n", ordinal);
		exit(1);
	}

	uint32_t num = ordinal;
	uint32_t ordinal_offset = Memory::Read32(exportBaseAddr+sizeof(xexExport_t)+(num*4));
	ordinal_offset += exportTable.imagebaseaddr << 16;
	return ordinal_offset;
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

void XexLoader::ParseLibraryInfo(uint32_t offset, xexLibrary_t &lib, int index, std::string& name)
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
		if ((record >> 24) == 1 && name != "xam.xex")
		{
			Memory::Write32(recordAddr+0x00, 0x39600000 | (index << 12) | (record & 0xFFFF));
			Memory::Write32(recordAddr+0x04, 0x44000042);
			Memory::Write32(recordAddr+0x08, 0x4e800020);
			Memory::Write32(recordAddr+0x0C, 0x60000000);
		}
		else if ((record >> 24) == 1 && name == "xam.xex")
		{
			// We instead write a stub to call xam.xex functions since we LLE it
			assert(this != xam); // Should never happen, but just in case

			// Get exports from xam.xex
			uint32_t addr = xam->LookupOrdinal(record & 0xFFFF);
			Memory::Write32(recordAddr+0x00, 0x3D600000 | addr >> 16);
			Memory::Write32(recordAddr+0x04, 0x616B0000 | (addr & 0xFFFF));
			Memory::Write32(recordAddr+0x08, 0x7D6903A6);
			Memory::Write32(recordAddr+0x0C, 0x4E800420);
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

int XexLoader::ReadImageCompressed(uint8_t *buffer, size_t xex_len, char **outBuffer)
{
	const uint32_t exe_length = (uint32_t)(xex_len - header.header_size);
	const uint8_t* exe_buffer = (const uint8_t*)(buffer + header.header_size);

	uint8_t* compress_buffer = NULL;
	const uint8_t* p = NULL;
	uint8_t* d = NULL;

	bool free_input = false;
	const uint8_t* input_buffer = exe_buffer;
	size_t input_size = exe_length;

	switch (encryptionFormat)
	{
	case 0:
		break;
	case 1:
		free_input = true;
		input_buffer = (const uint8_t*)calloc(1, exe_length);
		aes_decrypt_buffer(session_key, exe_buffer, exe_length, (uint8_t*)input_buffer, exe_length);
		break;
	}

	normalCompressionHeader_t hdr = *(normalCompressionHeader_t*)(buffer + fileInfoOffset + sizeof(fileFormatInfo_t));
	hdr.windowSize = bswap32(hdr.windowSize);
	hdr.firstBlock.blockSize = bswap32(hdr.firstBlock.blockSize);

	normalCompressionBlock_t curBlock = hdr.firstBlock;

	compress_buffer = (uint8_t*)calloc(1, exe_length);

	p = input_buffer;
	d = compress_buffer;

	int result_code = 0;

	uint32_t blockOffs = sizeof(normalCompressionBlock_t);
	while (curBlock.blockSize)
	{
		const uint8_t* pnext = p + curBlock.blockSize;
		normalCompressionBlock_t next_block = *(normalCompressionBlock_t*)p;
		blockOffs += sizeof(normalCompressionBlock_t);

		next_block.blockSize = bswap32(next_block.blockSize);

		p += 4;
		p += 20;

		while (true)
		{
			const size_t chunk_size = (p[0] << 8) | p[1];
			p += 2;
			if (!chunk_size)
				break;
			
			memcpy(d, p, chunk_size);
			p += chunk_size;
			d += chunk_size;
		}

		p = pnext;
		curBlock = next_block;
	}

	uint32_t uncompressed_size = image_size();
	char* out = new char[uncompressed_size];

	(*outBuffer) = out;

	if (!result_code)
	{
		std::memset(out, 0, uncompressed_size);

		result_code = lzx_decompress(compress_buffer, d - compress_buffer, out, uncompressed_size,
          hdr.windowSize, nullptr, 0);
	}

	if (compress_buffer)
		free((void*)compress_buffer);
	if (free_input)
		free((void*)input_buffer);
	return uncompressed_size;
}

uint32_t XexLoader::image_size()
{
	uint32_t pageDescriptorCount = bswap32(*(uint32_t*)&buffer[header.sec_info_offset + 0x180]);

	uint32_t totalSize = 0;

	for (int i = 0; i < pageDescriptorCount; i++)
	{
		uint32_t offs = header.sec_info_offset + 0x184 + (i * 0x18);
		pageDescriptor_t page = *(pageDescriptor_t*)&buffer[offs];
		page.value = bswap32(page.value);
		
		totalSize += page.value * 4096;
	}

	return totalSize;
}
