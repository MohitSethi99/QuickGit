#pragma once

struct git_commit;
struct git_oid;
struct git_reference;

namespace QuickGit
{
	typedef uint64_t UUID;

	class Utils
	{
	public:
		static UUID GenUUID(const char* str);
		static UUID GenUUID(const git_commit* commit);
		static UUID GenUUID(const git_oid* commitId);
		static UUID GenUUID(const git_reference* ref);
		static uint32_t GenerateColor(const char* str);
	};
}
