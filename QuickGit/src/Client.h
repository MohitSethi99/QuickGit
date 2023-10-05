#pragma once

#include <git2.h>

#include "Utils.h"

#define SHORT_SHA_LENGTH 7
#define COMMIT_ID_LEN 41
#define COMMIT_MSG_LEN 128
#define COMMIT_NAME_LEN 40
#define COMMIT_DATE_LEN 24

namespace QuickGit
{
	struct CommitData
	{
		char Message[COMMIT_MSG_LEN];
		char AuthorName[COMMIT_NAME_LEN];
		char CommitID[COMMIT_ID_LEN];
		char AuthorDate[COMMIT_DATE_LEN];

		git_commit* Commit;
		git_time_t CommitTime;
		UUID ID;
	};

	enum class BranchType { Remote, Local };

	struct BranchData
	{
		std::string Name;
		BranchType Type : 1;
		uint32_t Color;

		git_reference* Branch = nullptr;
	};

	struct RepoData
	{
		git_repository* Repository = nullptr;

		std::string Name{};
		std::string Filepath{};
		size_t UncommittedFiles = 0;

		UUID Head = 0;
		git_reference* HeadBranch = nullptr;
		std::unordered_map<git_reference*, BranchData> Branches;
		std::vector<CommitData> Commits;
		std::unordered_map<UUID, std::vector<BranchData>> BranchHeads;
		std::unordered_map<UUID, uint64_t> CommitsIndexMap;

		~RepoData()
		{
			for (auto& commitData : Commits)
				git_commit_free(commitData.Commit);

			for (auto& [branchRef, _] : Branches)
				git_reference_free(branchRef);

			git_repository_free(Repository);
		}
	};

	struct Diff
	{
		git_delta_t Status;
		uint64_t OldFileSize;
		uint64_t NewFileSize;
		std::string File;
		std::string Patch;
	};

	class Client
	{
	public:
		static void Init();
		static void Shutdown();

		static bool InitRepo(const std::string_view& path);
		static std::vector<std::unique_ptr<RepoData>>& GetRepositories();

		static void UpdateHead(RepoData& repoData);
		static void Fill(RepoData* data, git_repository* repo);
		static bool GenerateDiff(git_commit* commit, std::vector<Diff>& out);

		static git_reference* CreateBranch(const char* branchName, git_commit* commit);
		static bool CheckoutBranch(git_reference* branch, bool force = false);
		static bool CheckoutCommit(git_commit* commit, bool force = false);
		static bool Reset(git_commit* commit, git_reset_t resetType);
		static bool CreatePatch(git_commit* commit, std::string& out);
	};
}
