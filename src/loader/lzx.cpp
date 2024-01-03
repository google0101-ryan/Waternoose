#include "lzx.h"
#include <algorithm>
#include <climits>
#include <cstring>

#include <mspack.h>
#include <thirdparty/lzx.h>

typedef struct
{
	mspack_system sys;
	void* buffer;
	off_t buffer_size;
	off_t offset;
} mspackMemoryFile_t;

inline bool bit_scan_forward(uint32_t v, uint32_t* out_first_set_index) {
  int i = ffs(v);
  *out_first_set_index = i - 1;
  return i != 0;
}
inline bool bit_scan_forward(uint64_t v, uint32_t* out_first_set_index) {
  int i = __builtin_ffsll(v);
  *out_first_set_index = i - 1;
  return i != 0;
}

inline bool bit_scan_forward(int32_t v, uint32_t* out_first_set_index) {
  return bit_scan_forward(static_cast<uint32_t>(v), out_first_set_index);
}
inline bool bit_scan_forward(int64_t v, uint32_t* out_first_set_index) {
  return bit_scan_forward(static_cast<uint64_t>(v), out_first_set_index);
}

mspackMemoryFile_t* mspack_memory_open(mspack_system* sys, void* buffer, const size_t size)
{
	auto memfile = (mspackMemoryFile_t*)calloc(1, sizeof(mspackMemoryFile_t));
	if (!memfile)
		return NULL;
	memfile->buffer = buffer;
	memfile->buffer_size = (off_t)size;
	memfile->offset = 0;
	return memfile;
}

void mspack_memory_close(mspackMemoryFile_t* file)
{
	auto memfile = (mspackMemoryFile_t*)file;
	free(memfile);
}

int mspack_memory_read(mspack_file* file, void* buffer, int chars)
{
	auto memfile = (mspackMemoryFile_t*)file;
	const off_t remaining = memfile->buffer_size - memfile->offset;
	const off_t total = std::min(static_cast<off_t>(chars), remaining);
	std::memcpy(buffer, (uint8_t*)memfile->buffer + memfile->offset, total);
	memfile->offset += total;
	return (int)total;
}

int mspack_memory_write(mspack_file* file, void* buffer, int chars)
{
	auto memfile = (mspackMemoryFile_t*)file;
	const off_t remaining = memfile->buffer_size - memfile->offset;
	const off_t total = std::min(static_cast<off_t>(chars), remaining);
	std::memcpy((uint8_t*)memfile->buffer + memfile->offset, buffer, total);
	memfile->offset += total;
	return (int)total;
}

void* mspack_memory_alloc(mspack_system*, size_t chars)
{
	return calloc(chars, 1);
}

void mspack_memory_free(void* ptr)
{
	free(ptr);
}

void mspack_memory_copy(void* src, void* dest, size_t chars)
{
	memcpy(dest, src, chars);
}

mspack_system* mspack_memory_sys_create()
{
	auto sys = (mspack_system*)calloc(1, sizeof(mspack_system));
	if (!sys)
		return NULL;
	sys->read = mspack_memory_read;
	sys->write = mspack_memory_write;
	sys->alloc = mspack_memory_alloc;
	sys->free = mspack_memory_free;
	sys->copy = mspack_memory_copy;
	return sys;
}

void mspack_memory_sys_destroy(struct mspack_system* sys) { free(sys); }

int lzx_decompress(const void *lzx_data, size_t lzx_len, void *dest, size_t dest_len, uint32_t window_size, void *window_data, size_t window_data_len)
{
	int res = 1;

	uint32_t window_bits;
	if (!bit_scan_forward(window_size, &window_bits))
		return res;
	
	mspack_system* sys = mspack_memory_sys_create();
	mspackMemoryFile_t* lzxsrc = mspack_memory_open(sys, (void*)lzx_data, lzx_len);
	mspackMemoryFile_t* lzxdst = mspack_memory_open(sys, dest, dest_len);
	lzxd_stream* lzxd = lzxd_init(sys, (mspack_file*)lzxsrc, (mspack_file*)lzxdst, window_bits, 0, 0x8000, (off_t)dest_len, 0);

	if (lzxd)
	{
		if (window_data)
		{
			auto padding_len = window_size - window_data_len;
			memset(&lzxd->window[0], 0, padding_len);
			memcpy(&lzxd->window[padding_len], window_data, window_data_len);
			lzxd->ref_data_size = window_size;
		}

		res = lzxd_decompress(lzxd, (off_t)dest_len);

		lzxd_free(lzxd);
		lzxd = NULL;
	}

	if (lzxsrc)
	{
		mspack_memory_close(lzxsrc);
		lzxsrc = NULL;
	}

	if (lzxdst) 
	{
		mspack_memory_close(lzxdst);
		lzxdst = NULL;
	}

	if (sys) 
	{
		mspack_memory_sys_destroy(sys);
		sys = NULL;
	}

	return res;
}