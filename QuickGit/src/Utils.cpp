#include "pch.h"
#include "Utils.h"

#include <git2.h>

namespace QuickGit
{
	UUID Utils::GenUUID(const char* str)
	{
		UUID hash = 0;
		while (*str)
			hash = (hash << 5) + *str++;
		return hash;
	}

	UUID Utils::GenUUID(const git_commit* commit)
	{
		const git_oid* oid = git_commit_id(commit);
		return GenUUID(oid);
	}

	UUID Utils::GenUUID(const git_oid* commitId)
	{
		return GenUUID(git_oid_tostr_s(commitId));
	}

	UUID Utils::GenUUID(const git_reference* ref)
	{
		const git_oid* oid = git_reference_target(ref);
		return GenUUID(oid);
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
