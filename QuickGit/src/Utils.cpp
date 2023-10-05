#include "pch.h"
#include "Utils.h"

namespace QuickGit
{
	UUID Utils::GenUUID(const char* str)
	{
		UUID hash = 0;
		while (*str)
			hash = (hash << 5) + *str++;
		return hash;
	}

	uint32_t Utils::GenerateColor(const char* str)
	{
		uint32_t r = 0xFF;
		uint32_t g = 0xFF;
		uint32_t b = 0xFF;

		for (const char* p = str; *p != '\0'; ++p)
		{
			r ^= static_cast<uint32_t>(*p);
			g ^= static_cast<uint32_t>(*p) << 4;
			b ^= static_cast<uint32_t>(*p) << 8;
		}

		const uint32_t threshold = 0xB0;
		r = (r % (0xFF - threshold)) + threshold;
		g = (g % (0xFF - threshold)) + threshold;
		b = (b % (0xFF - threshold)) + threshold;

		return 0xAA000000u | (r << 16) | (g << 8) | b;
	}
}
