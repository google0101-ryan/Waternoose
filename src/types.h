#pragma once

#include <util.h>

union uint128_t
{
	unsigned __int128 u128;
	uint64_t u64[2];
	uint32_t u32[4];
	uint16_t u16[8];
	uint8_t u8[16];
};
