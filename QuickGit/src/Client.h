#pragma once

#include <git2.h>

#include "Utils.h"

#define COMMIT_SHORT_ID_LEN 7
#define COMMIT_ID_LEN 41
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
		char CommitID[COMMIT_ID_LEN];

		size_t MessageSize = 0;
		git_commit* Commit = nullptr;
		git_time_t CommitTime = 0;
		UUID ID = 0;
	};

	enum class BranchType { Remote, Local };

	struct BranchData
	{
		eastl::string Name;
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
		UUID SelectedCommit = 0;

		eastl::string Name{};
		eastl::string Filepath{};
		size_t UncommittedFiles = 0;

		UUID Head = 0;
		git_reference* HeadBranch = nullptr;
		eastl::hash_map<git_reference*, BranchData> Branches;
		eastl::vector<CommitData> Commits;
		eastl::hash_map<UUID, eastl::vector<git_reference*>> BranchHeads;
		eastl::hash_map<UUID, uint64_t> CommitsIndexMap;

		~RepoData()
		{
			for (auto& commitData : Commits)
				git_commit_free(commitData.Commit);

			for (auto& [branchRef, _] : Branches)
				git_reference_free(branchRef);

			git_repository_free(Repository);
		}
	};

	struct Patch
	{
		git_delta_t Status;
		uint64_t OldFileSize;
		uint64_t NewFileSize;
		eastl::string File;
		eastl::string Patch;
	};

	struct Diff
	{
		eastl::vector<Patch> Patches;
	};

	class Client
	{
	public:
		static void Init();
		static void Shutdown();

		static bool InitRepo(const eastl::string_view& path);
		static eastl::vector<eastl::unique_ptr<RepoData>>& GetRepositories();

		static void UpdateHead(RepoData& repoData);
		static void Fill(RepoData* data, git_repository* repo);
		static void FillDiff(git_diff* diff, Diff& out);
		static bool GenerateDiff(git_commit* commit, Diff& out, uint32_t contextLines = 0);
		static bool GenerateDiff(git_commit* oldCommit, git_commit* newCommit, Diff& out, uint32_t contextLines = 0);
		static bool GenerateDiffWithWorkDir(git_commit* commit, Diff& outUnstaged, Diff& outStaged, uint32_t contextLines = 0);
		static bool GenerateDiffWithWorkDir(git_repository* repo, Diff& outUnstaged, Diff& outStaged, uint32_t contextLines = 0);

		static git_reference* BranchCreate(RepoData* repo, const char* branchName, git_commit* commit, bool& outValidName);
		static bool BranchRename(RepoData* repo, git_reference* branch, const char* name, bool& outValidName);
		static bool BranchDelete(RepoData* repo, git_reference* branch);
		static bool BranchCheckout(git_reference* branch, bool force = false);
		static bool BranchReset(RepoData* repo, git_commit* commit, git_reset_t resetType);
		static bool CommitCheckout(git_commit* commit, bool force = false);
		static bool CreatePatch(git_commit* commit, eastl::string& out);

		static bool AddToIndex(git_repository* repo, const char* filepath);
		static bool RemoveFromIndex(git_repository* repo, const char* filepath);
		static bool Commit(RepoData* repo, const char* summary, const char* description);
	};
}
