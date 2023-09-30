#pragma once

#include <git2.h>

namespace QuickGit
{
	void ClientInit();
	void ClientShutdown();

	bool ClientInitRepo(const std::string_view& path);
	std::vector<git_repository*>& ClientGetRepositories();
}
