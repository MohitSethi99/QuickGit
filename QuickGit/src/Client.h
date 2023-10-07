#pragma once

#include <git2.h>

#include "Utils.h"

#define COMMIT_SHORT_ID_LEN 8
#define COMMIT_MSG_LEN 128
#define COMMIT_NAME_LEN 40
#define COMMIT_DATE_LEN 24

#define LOCAL_BRANCH_PREFIX "refs/heads/"
#define REMOTE_BRANCH_PREFIX "refs/remotes/"

namespace QuickGit
{
	struct CommitData
	{
		char Message[COMMIT_MSG_LEN];
		char AuthorName[COMMIT_NAME_LEN];
		char AuthorDate[COMMIT_DATE_LEN];
		char CommitID[COMMIT_SHORT_ID_LEN];

		git_commit* Commit;
		git_time_t CommitTime;
		UUID ID;
	};

	enum class BranchType { Remote, Local };

	struct BranchData
	{
		std::string Name;
		BranchType Type;
		uint32_t Color;

		const char* ShortName() const
		{
			const size_t startOffset = (Type == BranchType::Remote ? sizeof(REMOTE_BRANCH_PREFIX) : sizeof(LOCAL_BRANCH_PREFIX)) - 1;
			return Name.c_str() + startOffset;
		}
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
		std::unordered_map<UUID, std::vector<git_reference*>> BranchHeads;
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

		static git_reference* BranchCreate(RepoData* repo, const char* branchName, git_commit* commit, bool& outValidName);
		static bool BranchRename(RepoData* repo, git_reference* branch, const char* name, bool& outValidName);
		static bool BranchDelete(RepoData* repo, git_reference* branch);
		static bool BranchCheckout(git_reference* branch, bool force = false);
		static bool BranchReset(RepoData* repo, git_commit* commit, git_reset_t resetType);
		static bool CommitCheckout(git_commit* commit, bool force = false);
		static bool CreatePatch(git_commit* commit, std::string& out);
	};
}
