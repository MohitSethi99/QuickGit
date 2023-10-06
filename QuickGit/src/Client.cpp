#include "pch.h"
#include "Client.h"

#include "Timer.h"

namespace QuickGit
{
	static std::vector<std::unique_ptr<RepoData>> s_Repositories;

	static git_checkout_options s_SafeCheckoutOptions;
	static git_checkout_options s_ForceCheckoutOptions;

	void Client::Init()
	{
		git_libgit2_init();
		s_Repositories.reserve(10);

		s_SafeCheckoutOptions = { GIT_CHECKOUT_OPTIONS_VERSION, GIT_CHECKOUT_SAFE | GIT_CHECKOUT_UPDATE_SUBMODULES };
		s_ForceCheckoutOptions = { GIT_CHECKOUT_OPTIONS_VERSION, GIT_CHECKOUT_FORCE | GIT_CHECKOUT_UPDATE_SUBMODULES };
		//s_SafeCheckoutOptions.progress_cb = checkout_progress;
		s_SafeCheckoutOptions.dir_mode = 0777;
		s_SafeCheckoutOptions.file_mode = 0777;
		//s_ForceCheckoutOptions.progress_cb = checkout_progress;
		s_ForceCheckoutOptions.dir_mode = 0777;
		s_ForceCheckoutOptions.file_mode = 0777;
	}

	void Client::Shutdown()
	{
		s_Repositories.clear();
		git_libgit2_shutdown();
	}

	bool Client::InitRepo(const std::string_view& path)
	{
		if (!std::filesystem::exists(path))
			return false;

		git_repository* repo = nullptr;
		int error = git_repository_open(&repo, path.data());
		if (error != 0)
			return false;

		std::unique_ptr<RepoData> data = std::make_unique<RepoData>();
		Fill(data.get(), repo);
		s_Repositories.emplace_back(std::move(data));
		return true;
	}

	std::vector<std::unique_ptr<RepoData>>& Client::GetRepositories()
	{
		return s_Repositories;
	}

	void Client::UpdateHead(RepoData& repoData)
	{
		git_reference* ref;
		git_repository_head(&ref, repoData.Repository);
		repoData.Head = Utils::GenUUID(ref);
		git_reference_free(ref);

		repoData.HeadBranch = nullptr;
		for (const auto& [branchRef, _] : repoData.Branches)
		{
			if (git_branch_is_head(branchRef) == 1)
			{
				repoData.HeadBranch = branchRef;
				break;
			}
		}
	}

	void Client::Fill(RepoData* data, git_repository* repo)
	{
		if (!data || !repo)
			return;

		for (auto& commitData : data->Commits)
			git_commit_free(commitData.Commit);

		for (auto& [branchRef, _] : data->Branches)
			git_reference_free(branchRef);

		data->Commits.clear();
		data->Branches.clear();
		data->BranchHeads.clear();
		data->CommitsIndexMap.clear();

		data->Repository = repo;

		// Trim the last slash
		std::string filepath = git_repository_workdir(repo);
		size_t len = filepath.length();
		if (filepath[len - 1] == '/')
			filepath[len - 1] = '\0';

		data->Filepath = filepath;

		const char* lastSlash = strrchr(filepath.c_str(), '/');
		data->Name = lastSlash ? lastSlash + 1 : filepath;

		git_status_options statusOptions = GIT_STATUS_OPTIONS_INIT;
		git_status_list* statusList = nullptr;
		// TODO: error check
		[[maybe_unused]] int err = git_status_list_new(&statusList, repo, &statusOptions);
		data->UncommittedFiles = git_status_list_entrycount(statusList);
		git_status_list_free(statusList);

		git_reference_iterator* refIt = nullptr;
		git_reference* ref = nullptr;
		git_reference_iterator_new(&refIt, repo);
		std::unordered_set<std::string> uniqueCommits;
		while (git_reference_next(&ref, refIt) == 0)
		{
			git_reference_t refType = git_reference_type(ref);

			if (refType == GIT_REF_OID)
			{
				const char* refName = git_reference_name(ref);
				git_commit* targetCommit = nullptr;
				git_commit_lookup(&targetCommit, repo, git_reference_target(ref));
				if (data->Branches.find(ref) == data->Branches.end())
				{
					BranchData branchData;
					branchData.Name = refName;
					branchData.Type = git_reference_is_remote(ref) == 1 ? BranchType::Remote : BranchType::Local;
					branchData.Color = Utils::GenerateColor(refName);
					data->Branches[ref] = std::move(branchData);
					data->BranchHeads[Utils::GenUUID(targetCommit)].push_back(ref);
				}

				git_revwalk* walker;
				git_revwalk_new(&walker, repo);
				git_revwalk_push(walker, git_commit_id(targetCommit));

				[[maybe_unused]] const char* targetCommitMessage = git_commit_message(targetCommit);

				git_oid oid;
				while (git_revwalk_next(&oid, walker) == 0)
				{
					std::string idStr = git_oid_tostr_s(&oid);
					if (uniqueCommits.contains(idStr))
						continue;

					uniqueCommits.emplace(idStr);

					git_commit* commit = nullptr;
					if (git_commit_lookup(&commit, repo, &oid) == 0)
					{
						const git_signature* author = git_commit_author(commit);
						const char* commitSummary = git_commit_summary(commit);
						const UUID id = Utils::GenUUID(idStr.c_str());;

						CommitData cd;
						cd.Commit = commit;
						cd.CommitTime = author->when.time;
						cd.ID = id;

						strncpy_s(cd.CommitID, idStr.c_str(), sizeof(cd.CommitID) - 1);

						if (commitSummary)
							strncpy_s(cd.Message, commitSummary, sizeof(cd.Message) - 1);
						else
							memset(cd.Message, 0, sizeof(cd.Message));

						strncpy_s(cd.AuthorName, author->name, sizeof(cd.AuthorName) - 1);

						git_time_t timestamp = author->when.time;
						tm localTime;
						localtime_s(&localTime, &timestamp);
						strftime(cd.AuthorDate, sizeof(cd.AuthorDate), "%d %b %Y %H:%M:%S", &localTime);

						data->Commits.emplace_back(std::move(cd));
						data->CommitsIndexMap.emplace(id, data->Commits.size() - 1);
					}
				}

				git_revwalk_free(walker);
				git_commit_free(targetCommit);
			}
			else
			{
				git_reference_free(ref);
			}
		}
		git_reference_iterator_free(refIt);

		std::sort(data->Commits.begin(), data->Commits.end(), [](const CommitData& lhs, const CommitData& rhs)
		{
			return lhs.CommitTime > rhs.CommitTime;
		});

		UpdateHead(*data);
	}

	git_reference* Client::BranchCreate(RepoData* repo, const char* branchName, git_commit* commit, bool& outValidName)
	{
		Stopwatch sw("CreateBranch");

		int valid = 0;
		int err = git_branch_name_is_valid(&valid, branchName);
		outValidName = valid;

		if (err == 0 && outValidName)
		{
			git_reference* outBranch = nullptr;
			err = git_branch_create(&outBranch, repo->Repository, branchName, commit, 0);

			if (outBranch)
			{
				UUID id = Utils::GenUUID(commit);
				BranchData branchData;
				branchData.Type = BranchType::Local;
				branchData.Name = LOCAL_BRANCH_PREFIX + std::string(branchName);
				branchData.Color = Utils::GenerateColor(branchData.Name.c_str());
				repo->Branches[outBranch] = std::move(branchData);
				repo->BranchHeads[id].push_back(outBranch);
			}

			return outBranch;
		}

		return nullptr;
	}

	bool Client::BranchRename(RepoData* repo, git_reference* branch, const char* name, bool& outValidName)
	{
		int valid = 0;
		int err = git_branch_name_is_valid(&valid, name);

		outValidName = valid;
		git_reference* newRef = nullptr;
		std::string newName = LOCAL_BRANCH_PREFIX + std::string(name);
		UUID oldId = Utils::GenUUID(branch);
		if (err == 0 && outValidName)
		{
			err = git_reference_rename(&newRef, branch, newName.c_str(), 0, nullptr);
		}

		if (err == 0 && outValidName && newRef)
		{
			if (repo->HeadBranch == branch)
			{
				repo->HeadBranch = newRef;
			}

			repo->Branches[newRef] = repo->Branches.at(branch);
			BranchData& data = repo->Branches.at(newRef);
			data.Name = newName;
			data.Color = Utils::GenerateColor(data.Name.c_str());
			repo->Branches.erase(branch);

			auto& branchHeads = repo->BranchHeads.at(oldId);
			for (auto it = branchHeads.begin(); it != branchHeads.end(); ++it)
			{
				if (*it == branch)
				{
					branchHeads.erase(it);
					break;
				}
			}
			branchHeads.push_back(newRef);

			git_reference_free(branch);
		}

		return err == 0 && outValidName;
	}

	bool Client::BranchDelete(RepoData* repo, git_reference* branch)
	{
		int err = git_reference_delete(branch);

		if (err == 0)
		{
			UUID id = Utils::GenUUID(branch);
			repo->Branches.erase(branch);

			auto& branchHeads = repo->BranchHeads.at(id);
			for (auto it = branchHeads.begin(); it != branchHeads.end(); ++it)
			{
				if (*it == branch)
				{
					branchHeads.erase(it);
					break;
				}
			}

			git_reference_free(branch);
		}

		return err == 0;
	}

	bool Client::BranchCheckout(git_reference* branch, bool force /*= false*/)
	{
		Stopwatch sw("CheckoutBranch");

		git_repository* repo = git_reference_owner(branch);
		git_commit* commit;
		int err = git_commit_lookup(&commit, repo, git_reference_target(branch));

		if (err == 0)
		{
			err = git_checkout_tree(repo, reinterpret_cast<git_object*>(commit), force ? &s_ForceCheckoutOptions : &s_SafeCheckoutOptions);
			if (err == 0)
			{
				const char* branchName;
				err = git_branch_name(&branchName, branch);
				if (err == 0)
				{
					std::string branchNameFull = LOCAL_BRANCH_PREFIX + std::string(branchName);
					err = git_repository_set_head(repo, branchNameFull.c_str());
				}
			}
			git_commit_free(commit);
		}

		return err == 0;
	}

	bool Client::BranchReset(RepoData* repo, git_commit* commit, git_reset_t resetType)
	{
		Stopwatch sw("Reset");

		const int err = git_reset(repo->Repository, reinterpret_cast<const git_object*>(commit), resetType, resetType == GIT_RESET_HARD ? &s_ForceCheckoutOptions : &s_SafeCheckoutOptions);
		git_reference* newHead;
		git_repository_head(&newHead, repo->Repository);

		if (err == 0)
		{
			git_reference* oldHead = repo->HeadBranch;
			repo->Branches[newHead] = repo->Branches.at(oldHead);
			repo->Branches.erase(oldHead);
			auto& branchHeads = repo->BranchHeads.at(repo->Head);
			if (branchHeads.size() == 1)
			{
				repo->BranchHeads.erase(repo->Head);
			}
			else
			{
				for (auto it = branchHeads.begin(); it != branchHeads.end(); ++it)
				{
					if (*it == oldHead)
					{
						branchHeads.erase(it);
						break;
					}
				}
			}

			repo->HeadBranch = newHead;
			repo->Head = Utils::GenUUID(newHead);
			repo->BranchHeads[repo->Head].push_back(newHead);
		}
		return err == 0;
	}

	bool Client::CommitCheckout(git_commit* commit, bool force)
	{
		Stopwatch sw("CheckoutCommit");

		git_repository* repo = git_commit_owner(commit);
		int err = git_checkout_tree(repo, reinterpret_cast<git_object*>(commit), force ? &s_ForceCheckoutOptions : &s_SafeCheckoutOptions);
		if (err == 0)
			err = git_repository_set_head_detached(repo, git_commit_id(commit));
		return err == 0;
	}

	bool Client::GenerateDiff(git_commit* commit, std::vector<Diff>& out)
	{
		git_diff* diff = nullptr;
		git_commit* parent = nullptr;
		git_tree* commitTree = nullptr;
		git_tree* parentTree = nullptr;

		int err = git_commit_parent(&parent, commit, 0);
		if (err == 0)
			err = git_commit_tree(&commitTree, commit);
		if (err == 0)
			err = git_commit_tree(&parentTree, parent);

		if (err == 0)
		{
			git_diff_options diffOp = GIT_DIFF_OPTIONS_INIT;
			diffOp.flags = GIT_DIFF_PATIENCE | GIT_DIFF_MINIMAL;
			err = git_diff_tree_to_tree(&diff, git_commit_owner(commit), parentTree, commitTree, &diffOp);
		}

		if (diff)
		{
			size_t diffDeltas = git_diff_num_deltas(diff);
			for (size_t i = 0; i < diffDeltas; ++i)
			{
				const git_diff_delta* delta = git_diff_get_delta(diff, i);
				git_patch* patch = nullptr;
				if (git_patch_from_diff(&patch, diff, i) == 0)
				{
					git_buf patchStr = GIT_BUF_INIT;
					if (git_patch_to_buf(&patchStr, patch) == 0 && patchStr.ptr)
					{
						std::string_view patchString = patchStr.ptr;
						size_t start = patchString.find_first_of('@');
						if (start != std::string::npos)
							out.emplace_back(delta->status, delta->old_file.size, delta->new_file.size, delta->new_file.path, patchStr.ptr + start);
						else
							out.emplace_back(delta->status, delta->old_file.size, delta->new_file.size, delta->new_file.path, "@@BinaryData");
						git_buf_free(&patchStr);
					}

					git_patch_free(patch);
				}
			}
		}

		if (parentTree)
			git_tree_free(parentTree);
		if (commitTree)
			git_tree_free(commitTree);
		if (parent)
			git_commit_free(parent);
		if (diff)
			git_diff_free(diff);

		return err == 0;
	}

	bool Client::CreatePatch(git_commit* commit, std::string& out)
	{
		git_buf buf = GIT_BUF_INIT;
		git_email_create_options op = GIT_EMAIL_CREATE_OPTIONS_INIT;
		int err = git_email_create_from_commit(&buf, commit, &op);
		if (err == 0)
		{
			out = buf.ptr ? buf.ptr : "";
			const size_t patchEnd = out.rfind("--");
			if (patchEnd != std::string::npos)
			{
				out.resize(patchEnd + 2, '\0');
				out += "\nQuickGit";
				out += " 0.0.1";
				out += "\n\n";
			}
			git_buf_free(&buf);
		}

		return err == 0;
	}
}
