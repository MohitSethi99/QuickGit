#include "pch.h"
#include "Client.h"

namespace QuickGit
{
	std::vector<git_repository*> g_Repositories;

	void ClientInit()
	{
		git_libgit2_init();
		g_Repositories.reserve(10);
	}

	void ClientShutdown()
	{
		for (git_repository* repo : g_Repositories)
			git_repository_free(repo);
		
		g_Repositories.clear();
		git_libgit2_shutdown();
	}

	bool ClientInitRepo(const std::string_view& path)
	{
		if (!std::filesystem::exists(path))
			return false;

		git_repository* repo = nullptr;
		int error = git_repository_open(&repo, path.data());
		if (error != 0)
			return false;

		g_Repositories.emplace_back(repo);
		return true;
	}

	std::vector<git_repository*>& ClientGetRepositories()
	{
		return g_Repositories;
	}
}
