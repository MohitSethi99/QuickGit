#include "pch.h"
#include "Client.h"

namespace QuickGit
{
	static eastl::vector<eastl::unique_ptr<RepoData>> s_Repositories;

	static git_checkout_options s_SafeCheckoutOptions;
	static git_checkout_options s_ForceCheckoutOptions;

	void Client::Init(const git_checkout_progress_cb checkoutProgress /*= nullptr*/)
	{
		git_libgit2_init();
		s_Repositories.reserve(10);

		s_SafeCheckoutOptions = { GIT_CHECKOUT_OPTIONS_VERSION, GIT_CHECKOUT_SAFE | GIT_CHECKOUT_UPDATE_SUBMODULES };
		s_ForceCheckoutOptions = { GIT_CHECKOUT_OPTIONS_VERSION, GIT_CHECKOUT_FORCE | GIT_CHECKOUT_UPDATE_SUBMODULES };
		s_SafeCheckoutOptions.progress_cb = checkoutProgress;
		s_SafeCheckoutOptions.dir_mode = 0777;
		s_SafeCheckoutOptions.file_mode = 0777;
		s_ForceCheckoutOptions.progress_cb = checkoutProgress;
		s_ForceCheckoutOptions.dir_mode = 0777;
		s_ForceCheckoutOptions.file_mode = 0777;
	}

	void Client::Shutdown()
	{
		s_Repositories.clear();
		git_libgit2_shutdown();
	}

	bool Client::InitRepo(const eastl::string_view& path)
	{
		if (!std::filesystem::exists(path.data()))
			return false;

		git_repository* repo = nullptr;
		int error = git_repository_open(&repo, path.data());
		if (error != 0)
			return false;

		eastl::unique_ptr<RepoData> data = eastl::make_unique<RepoData>();
		Fill(data.get(), repo);
		s_Repositories.emplace_back(eastl::move(data));
		return true;
	}

	eastl::vector<eastl::unique_ptr<RepoData>>& Client::GetRepositories()
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

	void FillCommit(git_commit* commit, CommitData* outCommitData)
	{
		const git_signature* author = git_commit_author(commit);
		const char* commitSummary = git_commit_summary(commit);
		const git_oid* oid = git_commit_id(commit);
		const char* strId = git_oid_tostr_s(oid);
		const UUID id = Utils::GenUUID(oid);

		outCommitData->Commit = commit;
		outCommitData->ID = id;

		strncpy_s(outCommitData->CommitID, strId, sizeof(outCommitData->CommitID) - 1);

		if (commitSummary)
		{
			strncpy_s(outCommitData->Message, commitSummary, sizeof(outCommitData->Message) - 1);
			outCommitData->MessageSize = strlen(outCommitData->Message);
		}
		else
		{
			outCommitData->Message[0] = 0;
			outCommitData->MessageSize = 0;
		}

		strncpy_s(outCommitData->AuthorName, author->name, sizeof(outCommitData->AuthorName) - 1);

		git_time_t timestamp = author->when.time;
		tm localTime;
		localtime_s(&localTime, &timestamp);
		strftime(outCommitData->AuthorDate, sizeof(outCommitData->AuthorDate), "%d %b %Y %H:%M:%S", &localTime);
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
		eastl::string filepath = git_repository_workdir(repo);
		size_t len = filepath.length();
		if (filepath[len - 1] == '/')
			filepath[len - 1] = '\0';

		data->Filepath = filepath;

		const char* lastSlash = strrchr(filepath.c_str(), '/');
		data->Name = lastSlash ? lastSlash + 1 : filepath;

		git_status_options statusOptions = GIT_STATUS_OPTIONS_INIT;
		git_status_list* statusList = nullptr;
		git_status_list_new(&statusList, repo, &statusOptions);
		data->UncommittedFiles = git_status_list_entrycount(statusList);
		git_status_list_free(statusList);

		git_reference_iterator* refIt = nullptr;
		git_reference* ref = nullptr;
		git_reference_iterator_new(&refIt, repo);
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
					data->Branches[ref] = eastl::move(branchData);
					data->BranchHeads[Utils::GenUUID(targetCommit)].push_back(ref);
				}
				git_commit_free(targetCommit);
			}
			else
			{
				git_reference_free(ref);
			}
		}
		git_reference_iterator_free(refIt);

		git_revwalk* walker;
		git_revwalk_new(&walker, repo);
		git_revwalk_push_glob(walker, "refs/heads");
		git_revwalk_push_glob(walker, "refs/remotes");
		git_revwalk_sorting(walker, GIT_SORT_TIME | GIT_SORT_TOPOLOGICAL);

		git_oid oid;
		while (git_revwalk_next(&oid, walker) == 0)
		{
			git_commit* commit = nullptr;
			if (git_commit_lookup(&commit, repo, &oid) == 0)
			{
				CommitData cd;
				FillCommit(commit, &cd);
				UUID id = cd.ID;
				data->CommitsIndexMap.emplace(id, data->Commits.size());
				data->Commits.emplace_back(eastl::move(cd));
			}
		}
		git_revwalk_free(walker);

		UpdateHead(*data);
	}

	git_reference* Client::BranchCreate(RepoData* repo, const char* branchName, git_commit* commit, bool& outValidName)
	{
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
				branchData.Name = LOCAL_BRANCH_PREFIX + eastl::string(branchName);
				branchData.Color = Utils::GenerateColor(branchData.Name.c_str());
				repo->Branches[outBranch] = eastl::move(branchData);
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
		eastl::string newName = eastl::string(LOCAL_BRANCH_PREFIX) + name;
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
					eastl::string branchNameFull = eastl::string(LOCAL_BRANCH_PREFIX) + branchName;
					err = git_repository_set_head(repo, branchNameFull.c_str());
				}
			}
		}

		git_commit_free(commit);

		return err == 0;
	}

	bool Client::BranchReset(RepoData* repo, git_commit* commit, git_reset_t resetType)
	{
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
		git_repository* repo = git_commit_owner(commit);
		int err = git_checkout_tree(repo, reinterpret_cast<git_object*>(commit), force ? &s_ForceCheckoutOptions : &s_SafeCheckoutOptions);
		if (err == 0)
			err = git_repository_set_head_detached(repo, git_commit_id(commit));
		return err == 0;
	}

	void Client::FillDiff(git_diff* diff, Diff& out)
	{
		size_t diffDeltas = git_diff_num_deltas(diff);
		for (size_t i = 0; i < diffDeltas; ++i)
		{
			const git_diff_delta* delta = git_diff_get_delta(diff, i);

			#if 0
			if (delta->flags == GIT_DIFF_FLAG_BINARY)
			{
				out.emplace_back(delta->status, delta->old_file.size, delta->new_file.size, delta->new_file.path, "@@BinaryData");
			}
			else
			#endif
			{
				git_patch* patch = nullptr;
				if (git_patch_from_diff(&patch, diff, i) == 0)
				{
					git_buf patchStr = GIT_BUF_INIT;
					if (git_patch_to_buf(&patchStr, patch) == 0 && patchStr.ptr)
					{
						eastl::string_view patchString = patchStr.ptr;
						const size_t start = patchString.find_first_of('@');
						const size_t offset = start != eastl::string::npos ? start : 0;
						out.Patches.emplace_back(delta->status, delta->old_file.size, delta->new_file.size, delta->new_file.path, patchStr.ptr + offset);
					}
					git_buf_free(&patchStr);
				}
				git_patch_free(patch);
			}
		}
	}

	bool Client::GenerateDiff(git_commit* commit, Diff& out, uint32_t contextLines)
	{
		git_commit* parent = nullptr;
		int err = git_commit_parent(&parent, commit, 0);

		bool success = err == 0;
		if (success)
			success = GenerateDiff(parent, commit, out, contextLines);

		git_commit_free(parent);

		return success;
	}

	bool Client::GenerateDiff(git_commit* oldCommit, git_commit* newCommit, Diff& out, uint32_t contextLines)
	{
		git_diff* diff = nullptr;
		git_tree* oldCommitTree = nullptr;
		git_tree* newCommitTree = nullptr;

		int err = git_commit_tree(&newCommitTree, newCommit);
		if (err == 0)
			err = git_commit_tree(&oldCommitTree, oldCommit);

		if (err == 0)
		{
			git_diff_options diffOp = GIT_DIFF_OPTIONS_INIT;
			diffOp.flags = GIT_DIFF_MINIMAL | GIT_DIFF_INDENT_HEURISTIC | GIT_DIFF_UPDATE_INDEX | GIT_DIFF_SHOW_UNTRACKED_CONTENT;
			diffOp.context_lines = contextLines;
			err = git_diff_tree_to_tree(&diff, git_commit_owner(newCommit), oldCommitTree, newCommitTree, &diffOp);
		}

		if (diff)
		{
			FillDiff(diff, out);
		}

		git_diff_free(diff);
		git_tree_free(oldCommitTree);
		git_tree_free(newCommitTree);

		return err == 0;
	}

	bool Client::GenerateDiffWithWorkDir(git_repository* repo, Diff& outUnstaged, Diff& outStaged, uint32_t contextLines)
	{
		git_reference* head = nullptr;
		int err = git_repository_head(&head, repo);

		const git_oid* oid = git_reference_target(head);

		git_commit* commit = nullptr;
		if (err == 0)
		{
			err = git_commit_lookup(&commit, repo, oid);
			err = GenerateDiffWithWorkDir(commit, outUnstaged, outStaged, contextLines) ? 0 : -1;
		}

		git_commit_free(commit);
		git_reference_free(head);

		return err == 0;
	}

	bool Client::GenerateDiffWithWorkDir(git_commit* commit, Diff& outUnstaged, Diff& outStaged, uint32_t contextLines)
	{
		git_diff* unstagedDiff = nullptr;
		git_diff* stagedDiff = nullptr;
		git_tree* commitTree = nullptr;

		int err = git_commit_tree(&commitTree, commit);

		if (err == 0)
		{
			git_repository* repo = git_commit_owner(commit);

			git_diff_options diffOp = GIT_DIFF_OPTIONS_INIT;
			diffOp.flags = GIT_DIFF_MINIMAL | GIT_DIFF_INDENT_HEURISTIC | GIT_DIFF_UPDATE_INDEX | GIT_DIFF_SHOW_UNTRACKED_CONTENT;
			diffOp.context_lines = contextLines;

			err = git_diff_index_to_workdir(&unstagedDiff, repo, nullptr, &diffOp);
			
			if (err == 0)
				err = git_diff_tree_to_index(&stagedDiff, repo, commitTree, nullptr, &diffOp);
		}

		if (err == 0 && unstagedDiff && stagedDiff)
		{
			FillDiff(unstagedDiff, outUnstaged);
			FillDiff(stagedDiff, outStaged);
		}

		git_tree_free(commitTree);
		git_diff_free(unstagedDiff);
		git_diff_free(stagedDiff);
		
		return err == 0;
	}

	bool Client::CreatePatch(git_commit* commit, eastl::string& out)
	{
		git_buf buf = GIT_BUF_INIT;
		git_email_create_options op = GIT_EMAIL_CREATE_OPTIONS_INIT;
		int err = git_email_create_from_commit(&buf, commit, &op);
		if (err == 0)
		{
			out = buf.ptr ? buf.ptr : "";
			const size_t patchEnd = out.rfind("--");
			if (patchEnd != eastl::string::npos)
			{
				out.resize(patchEnd + 2, 0);
				out += "\nQuickGit";
				out += " 0.0.1";
				out += "\n\n";
			}
		}

		git_buf_free(&buf);
		
		return err == 0;
	}

	// Uses commandline to do add/restore as git_apply() is not consistent
	// issue: https://github.com/libgit2/libgit2/issues/6643
	bool Client::AddToIndex(git_repository* repo, const char* filepath)
	{
		std::filesystem::path repoPath = git_repository_workdir(repo);

		git_index* index = nullptr;
		int err = git_repository_index(&index, repo);

		if (err == 0)
		{
			std::filesystem::path currentWorkDir = std::filesystem::current_path();
			std::filesystem::current_path(repoPath);

			eastl::string command = "git add ";
			command += "\"";
			command += filepath;
			command += "\"";
			err = std::system(command.c_str());

			std::filesystem::current_path(currentWorkDir);
		}

		git_index_free(index);

		return err == 0;
	}

	bool Client::RemoveFromIndex(git_repository* repo, const char* filepath)
	{
		std::filesystem::path repoPath = git_repository_workdir(repo);

		git_index* index = nullptr;
		int err = git_repository_index(&index, repo);

		if (err == 0)
		{
			std::filesystem::path currentWorkDir = std::filesystem::current_path();
			std::filesystem::current_path(repoPath);

			eastl::string command = "git restore --staged ";
			command += "\"";
			command += filepath;
			command += "\"";
			err = std::system(command.c_str());

			std::filesystem::current_path(currentWorkDir);
		}

		git_index_free(index);

		return err == 0;
	}

	bool Client::Commit(RepoData* repo, const char* summary, const char* description)
	{
		git_reference* ref = nullptr;
		git_object* parent = nullptr;
		int err = git_revparse_ext(&parent, &ref, repo->Repository, "HEAD");

		git_index* index = nullptr;
		if (err == 0)
			err = git_repository_index(&index, repo->Repository);

		git_oid treeId;
		if (err == 0)
			err = git_index_write_tree(&treeId, index);

		if (err == 0)
			err = git_index_write(index);

		git_tree* tree = nullptr;
		if (err == 0)
			err = git_tree_lookup(&tree, repo->Repository, &treeId);

		git_signature* signature = nullptr;
		if (err == 0)
			err = git_signature_default(&signature, repo->Repository);

		git_oid commitId;
		if (err == 0)
		{
			eastl::string message = summary;
			message += "\n\n";
			message += description;
			err = git_commit_create_v(&commitId, repo->Repository,  "HEAD", signature, signature, nullptr, message.c_str(), tree, parent ? 1 : 0, parent);
		}

		if (err == 0)
		{
			git_commit* commit = nullptr;
			if (git_commit_lookup(&commit, repo->Repository, &commitId) == 0)
			{
				CommitData cd;
				FillCommit(commit, &cd);
				UUID cdId = cd.ID;
				repo->Commits.emplace(repo->Commits.begin(), eastl::move(cd));

				for (auto& [id, idx] : repo->CommitsIndexMap)
					++idx;

				repo->CommitsIndexMap[cdId] = 0;

				UpdateHead(*repo);
			}
		}

		git_index_free(index);
		git_signature_free(signature);
		git_tree_free(tree);
		git_object_free(parent);
		git_reference_free(ref);

		return err == 0;
	}
}
