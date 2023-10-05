#pragma once

namespace QuickGit
{
	typedef uint64_t UUID;

	class Utils
	{
	public:
		static UUID GenUUID(const char* str);
		static uint32_t GenerateColor(const char* str);
	};
}
