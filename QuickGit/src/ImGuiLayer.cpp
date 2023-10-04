#include "pch.h"
#include "ImGuiLayer.h"

#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui.h>
#include <imgui_internal.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <icons/IconsMaterialDesignIcons.h>
#include <icons/MaterialDesign.inl>

#include <set>

#include "Timer.h"
#include "Client.h"

#define SHORT_SHA_LENGTH 7
#define COMMIT_ID_LEN 41
#define COMMIT_MSG_LEN 128
#define COMMIT_NAME_LEN 40
#define COMMIT_DATE_LEN 24

struct DescendingComparator
{
	bool operator()(const git_time_t& a, const git_time_t& b) const
	{
		return a > b;
	}
};

namespace QuickGit
{
	typedef uint64_t UUID;

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
		git_reference* Branch = nullptr;

		BranchType Type;
		uint32_t Color;
		std::string Name;
	};

	UUID GenUUID(const char* str)
	{
		UUID hash = 0;
		while (*str)
			hash = (hash << 5) + *str++;
		return hash;
	}

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

	GLFWwindow* g_Window;
	char g_Path[2048];
	std::vector<std::unique_ptr<RepoData>> g_SelectedRepos;
	CommitData* g_SelectedCommit;

	ImFont* g_DefaultFont = nullptr;
	ImFont* g_SmallFont = nullptr;
	ImFont* g_HeadingFont = nullptr;
	ImFont* g_BoldFont = nullptr;

	static void AddIconFont(float fontSize)
	{
		ImGuiIO& io = ImGui::GetIO();

		static constexpr ImWchar icons_ranges[] = { ICON_MIN_MDI, ICON_MAX_MDI, 0 };
		ImFontConfig iconsConfig;
		iconsConfig.MergeMode = true;
		iconsConfig.PixelSnapH = true;
		iconsConfig.GlyphOffset.y = 1.0f;
		iconsConfig.OversampleH = iconsConfig.OversampleV = 1;
		iconsConfig.GlyphMinAdvanceX = 4.0f;
		iconsConfig.SizePixels = 12.0f;

		io.Fonts->AddFontFromMemoryCompressedTTF(MaterialDesign_compressed_data, MaterialDesign_compressed_size, fontSize, &iconsConfig, icons_ranges);
	}

	void SetFont()
	{
		ImGuiIO& io = ImGui::GetIO();
		float fontSize = 16.0f;
		float fontSizeSmall = 12.0f;
		float fontSizeHeading = 22.0f;

		ImFontConfig iconsConfig;
		iconsConfig.MergeMode = false;
		iconsConfig.PixelSnapH = true;
		iconsConfig.OversampleH = iconsConfig.OversampleV = 1;
		iconsConfig.GlyphMinAdvanceX = 4.0f;
		iconsConfig.SizePixels = 12.0f;

		constexpr const char* regularFontPath = "res/fonts/jetbrains-mono/JetBrainsMono-Medium.ttf";
		constexpr const char* boldFontPath = "res/fonts/jetbrains-mono/JetBrainsMono-Bold.ttf";

		g_DefaultFont = io.Fonts->AddFontFromFileTTF(regularFontPath, fontSize, &iconsConfig);
		AddIconFont(fontSize);
		g_SmallFont = io.Fonts->AddFontFromFileTTF(regularFontPath, fontSizeSmall, &iconsConfig);
		AddIconFont(fontSizeSmall);

		g_BoldFont = io.Fonts->AddFontFromFileTTF(boldFontPath, fontSize, &iconsConfig);
		AddIconFont(fontSize);
		g_HeadingFont = io.Fonts->AddFontFromFileTTF(boldFontPath, fontSizeHeading, &iconsConfig);
		AddIconFont(fontSizeHeading);

		io.Fonts->TexGlyphPadding = 1;
		for (int n = 0; n < io.Fonts->ConfigData.Size; n++)
		{
			ImFontConfig* fontConfig = &io.Fonts->ConfigData[n];
			fontConfig->RasterizerMultiply = 1.0f;
		}
		io.Fonts->Build();
	}

	// Define a progress callback function
	void checkout_progress(const char* path, size_t completed_steps, size_t total_steps, [[maybe_unused]] void* payload)
	{
		printf("Checkout progress: %s - %zu/%zu\n", path, completed_steps, total_steps);
	}

	void UpdateHead(RepoData& repoData)
	{
		git_reference* ref;
		git_repository_head(&ref, repoData.Repository);
		const git_oid* id = git_reference_target(ref);
		repoData.Head = GenUUID(git_oid_tostr_s(id));
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

	constexpr uint32_t GenerateColor(const char* str)
	{
		// Initialize color components
		uint32_t r = 0xFF;
		uint32_t g = 0xFF;
		uint32_t b = 0xFF;

		// Calculate the checksum of the input string
		for (const char* p = str; *p != '\0'; ++p) {
			r ^= static_cast<uint32_t>(*p);
			g ^= static_cast<uint32_t>(*p) << 4;
			b ^= static_cast<uint32_t>(*p) << 8;
		}

		// Ensure that the components are in the pastel color range (higher than a threshold)
		const uint32_t threshold = 0xB0; // Adjust this threshold as needed
		r = (r % (0xFF - threshold)) + threshold;
		g = (g % (0xFF - threshold)) + threshold;
		b = (b % (0xFF - threshold)) + threshold;

		// Combine the components into a single 32-bit color value with alpha set to 0xFF
		return 0xAA000000u | (r << 16) | (g << 8) | b;
	}

	void Fill(RepoData* data, git_repository* repo)
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
		std::set<std::string> uniqueCommits;
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
					const git_oid* targetId = git_commit_id(targetCommit);
					BranchData branchData;
					branchData.Branch = ref;
					branchData.Type = git_reference_is_remote(ref) == 1 ? BranchType::Remote : BranchType::Local;
					branchData.Color = GenerateColor(refName);
					branchData.Name = refName;
					data->Branches[ref] = branchData;
					data->BranchHeads[GenUUID(git_oid_tostr_s(targetId))].push_back(std::move(branchData));
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
						const UUID id = GenUUID(idStr.c_str());;

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

	void HeadingText(const char* fmt, ...)
	{
		ImGui::PushFont(g_HeadingFont);
		va_list args;
		va_start(args, fmt);
		ImGui::TextV(fmt, args);
		va_end(args);
		ImGui::PopFont();
	}

	void HeadingTextUnformatted(const char* text, const char* textEnd = nullptr)
	{
		ImGui::PushFont(g_HeadingFont);
		ImGui::TextUnformatted(text, textEnd);
		ImGui::PopFont();
	}

	bool ImGuiInit()
	{
		if (!glfwInit())
			return false;

		const char* glsl_version = "#version 130";
		glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
		glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
		glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

		g_Window = glfwCreateWindow(1280, 720, "QuickGit", nullptr, nullptr);
		if (g_Window == nullptr)
			return false;

		glfwMakeContextCurrent(g_Window);
		glfwSwapInterval(1);

		if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
			return false;

		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO(); (void)io;
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;         // Enable Docking
		io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;       // Enable Multi-Viewport / Platform Windows
		io.ConfigFlags |= ImGuiConfigFlags_DpiEnableScaleViewports;
		io.ConfigFlags |= ImGuiConfigFlags_DpiEnableScaleFonts;

		io.ConfigWindowsMoveFromTitleBarOnly = true;
		io.ConfigDragClickToInputText = true;
		io.ConfigDockingTransparentPayload = true;

		//Colours
		ImGui::StyleColorsDark();
		{
			ImVec4* colors = ImGui::GetStyle().Colors;
			colors[ImGuiCol_Text] = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
			colors[ImGuiCol_TextDisabled] = ImVec4(0.60f, 0.60f, 0.60f, 1.00f);
			colors[ImGuiCol_WindowBg] = ImVec4(0.94f, 0.94f, 0.94f, 1.00f);
			colors[ImGuiCol_ChildBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
			colors[ImGuiCol_PopupBg] = ImVec4(1.00f, 1.00f, 1.00f, 0.98f);
			colors[ImGuiCol_Border] = ImVec4(0.00f, 0.00f, 0.00f, 0.30f);
			colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
			colors[ImGuiCol_FrameBg] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
			colors[ImGuiCol_FrameBgHovered] = ImVec4(0.94f, 0.94f, 0.94f, 1.00f);
			colors[ImGuiCol_FrameBgActive] = ImVec4(0.70f, 0.82f, 0.95f, 0.39f);
			colors[ImGuiCol_TitleBg] = ImVec4(0.86f, 0.86f, 0.86f, 1.00f);
			colors[ImGuiCol_TitleBgActive] = ImVec4(0.86f, 0.86f, 0.86f, 1.00f);
			colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.86f, 0.86f, 0.86f, 1.00f);
			colors[ImGuiCol_MenuBarBg] = ImVec4(0.86f, 0.86f, 0.86f, 1.00f);
			colors[ImGuiCol_ScrollbarBg] = ImVec4(0.86f, 0.86f, 0.86f, 0.53f);
			colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.69f, 0.69f, 0.69f, 0.80f);
			colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.49f, 0.49f, 0.49f, 0.80f);
			colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.49f, 0.49f, 0.49f, 1.00f);
			colors[ImGuiCol_CheckMark] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
			colors[ImGuiCol_SliderGrab] = ImVec4(0.41f, 0.67f, 0.98f, 1.00f);
			colors[ImGuiCol_SliderGrabActive] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
			colors[ImGuiCol_Button] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
			colors[ImGuiCol_ButtonHovered] = ImVec4(0.94f, 0.94f, 0.94f, 1.00f);
			colors[ImGuiCol_ButtonActive] = ImVec4(0.70f, 0.82f, 0.95f, 1.00f);
			colors[ImGuiCol_Header] = ImVec4(0.91f, 0.91f, 0.91f, 1.00f);
			colors[ImGuiCol_HeaderHovered] = ImVec4(0.81f, 0.81f, 0.81f, 0.80f);
			colors[ImGuiCol_HeaderActive] = ImVec4(0.89f, 0.89f, 0.89f, 1.00f);
			colors[ImGuiCol_Separator] = ImVec4(0.81f, 0.81f, 0.81f, 0.62f);
			colors[ImGuiCol_SeparatorHovered] = ImVec4(0.56f, 0.56f, 0.56f, 0.78f);
			colors[ImGuiCol_SeparatorActive] = ImVec4(0.78f, 0.78f, 0.78f, 1.00f);
			colors[ImGuiCol_ResizeGrip] = ImVec4(0.86f, 0.86f, 0.86f, 1.00f);
			colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.65f, 0.65f, 0.65f, 1.00f);
			colors[ImGuiCol_ResizeGripActive] = ImVec4(0.81f, 0.81f, 0.81f, 1.00f);
			colors[ImGuiCol_Tab] = ImVec4(0.91f, 0.91f, 0.91f, 1.00f);
			colors[ImGuiCol_TabHovered] = ImVec4(0.98f, 0.98f, 0.98f, 1.00f);
			colors[ImGuiCol_TabActive] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
			colors[ImGuiCol_TabUnfocused] = ImVec4(0.91f, 0.91f, 0.91f, 1.00f);
			colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.94f, 0.94f, 0.94f, 1.00f);
			colors[ImGuiCol_DockingPreview] = ImVec4(0.26f, 0.59f, 0.98f, 0.22f);
			colors[ImGuiCol_DockingEmptyBg] = ImVec4(0.86f, 0.86f, 0.86f, 1.00f);
			colors[ImGuiCol_PlotLines] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
			colors[ImGuiCol_PlotLinesHovered] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
			colors[ImGuiCol_PlotHistogram] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
			colors[ImGuiCol_PlotHistogramHovered] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
			colors[ImGuiCol_TableHeaderBg] = ImVec4(0.78f, 0.87f, 0.98f, 1.00f);
			colors[ImGuiCol_TableBorderStrong] = ImVec4(0.57f, 0.57f, 0.64f, 1.00f);
			colors[ImGuiCol_TableBorderLight] = ImVec4(0.68f, 0.68f, 0.74f, 1.00f);
			colors[ImGuiCol_TableRowBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
			colors[ImGuiCol_TableRowBgAlt] = ImVec4(0.30f, 0.30f, 0.30f, 0.09f);
			colors[ImGuiCol_TextSelectedBg] = ImVec4(0.26f, 0.59f, 0.98f, 0.35f);
			colors[ImGuiCol_DragDropTarget] = ImVec4(0.26f, 0.59f, 0.98f, 0.95f);
			colors[ImGuiCol_NavHighlight] = ImVec4(0.26f, 0.59f, 0.98f, 0.80f);
			colors[ImGuiCol_NavWindowingHighlight] = ImVec4(0.26f, 0.59f, 0.98f, 0.70f);
			colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.20f, 0.20f, 0.20f, 0.20f);
			colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.20f, 0.20f, 0.20f, 0.35f);

			ImGuiStyle& style = ImGui::GetStyle();
			if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
			{
				style.WindowRounding = 0.0f;
				style.Colors[ImGuiCol_WindowBg].w = 1.0f;
			}
			style.AntiAliasedFill = true;
			style.AntiAliasedLines = true;
			style.AntiAliasedLinesUseTex = true;
			style.WindowPadding = ImVec2(4.0f, 4.0f);
			style.FramePadding = ImVec2(12.0f, 4.0f);
			style.TabMinWidthForCloseButton = 0.1f;
			style.CellPadding = ImVec2(10.0f, 4.0f);
			style.ItemSpacing = ImVec2(8.0f, 4.0f);
			style.ItemInnerSpacing = ImVec2(4.0f, 4.0f);
			style.TouchExtraPadding = ImVec2(0.0f, 0.0f);
			style.IndentSpacing = 12;
			style.ScrollbarSize = 13;
			style.GrabMinSize = 10;
			style.WindowBorderSize = 1.0f;
			style.ChildBorderSize = 0.0f;
			style.PopupBorderSize = 1.5f;
			style.FrameBorderSize = 1.0f;
			style.TabBorderSize = 0.0f;
			style.WindowRounding = 6.0f;
			style.ChildRounding = 0.0f;
			style.FrameRounding = 2.0f;
			style.PopupRounding = 2.0f;
			style.ScrollbarRounding = 3.0f;
			style.GrabRounding = 2.0f;
			style.LogSliderDeadzone = 4.0f;
			style.TabRounding = 3.0f;
			style.WindowTitleAlign = ImVec2(0.0f, 0.5f);
			style.WindowMenuButtonPosition = ImGuiDir_None;
			style.ColorButtonPosition = ImGuiDir_Left;
			style.ButtonTextAlign = ImVec2(0.5f, 0.5f);
			style.SelectableTextAlign = ImVec2(0.0f, 0.0f);
			style.SeparatorTextBorderSize = 2.0f;
			style.DisplaySafeAreaPadding = ImVec2(8.0f, 8.0f);

			style.WindowMinSize.x = 370.0f;
			style.WindowMinSize.y = 185.0f;

			ImGuiColorEditFlags colorEditFlags = ImGuiColorEditFlags_AlphaBar
				| ImGuiColorEditFlags_AlphaPreviewHalf
				| ImGuiColorEditFlags_DisplayRGB
				| ImGuiColorEditFlags_InputRGB
				| ImGuiColorEditFlags_PickerHueBar
				| ImGuiColorEditFlags_Uint8;
			ImGui::SetColorEditOptions(colorEditFlags);
		}

		SetFont();

		ImGui_ImplGlfw_InitForOpenGL(g_Window, true);
		ImGui_ImplOpenGL3_Init(glsl_version);

		ClientInit();

		memset(g_Path, 0, 2048);

	#if 1
		ClientInitRepo("G:/Projects/ArcEngine");
		ClientInitRepo("G:/Projects/MohitSethi99.github.io");
		std::vector<git_repository*>& repos = ClientGetRepositories();
		for (git_repository* repo : repos)
		{
			std::unique_ptr<RepoData> data = std::make_unique<RepoData>();
			Fill(data.get(), repo);
			g_SelectedRepos.emplace_back(std::move(data));
		}
	#endif

		return true;
	}
	
	void ImGuiShutdown()
	{
		ClientShutdown();

		ImGui_ImplOpenGL3_Shutdown();
		ImGui_ImplGlfw_Shutdown();
		ImGui::DestroyContext();

		glfwDestroyWindow(g_Window);
		glfwTerminate();
	}

	bool ShowModal(const char* name)
	{
		constexpr ImGuiWindowFlags modalWindowFlags =
			ImGuiWindowFlags_NoTitleBar |
			ImGuiWindowFlags_NoResize |
			ImGuiWindowFlags_NoSavedSettings;

		const ImVec2 center = ImGui::GetMainViewport()->GetCenter();
		constexpr float modalWidth = 1000.0f;

		ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
		ImGui::SetNextWindowSize({ modalWidth, 0 }, ImGuiCond_Once);

		bool result = ImGui::BeginPopupModal(name, nullptr, modalWindowFlags);

		if (result)
			HeadingTextUnformatted(name);

		return result;
	}

	ImVec4 GetPatchStatusColor(git_delta_t status)
	{
		switch (status)
		{
			case GIT_DELTA_UNMODIFIED	: return ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
			case GIT_DELTA_ADDED		: return ImVec4(0.10f, 0.60f, 0.10f, 1.00f);
			case GIT_DELTA_DELETED		: return ImVec4(0.90f, 0.25f, 0.25f, 1.00f);
			case GIT_DELTA_MODIFIED		: return ImVec4(0.60f, 0.60f, 0.10f, 1.00f);
			case GIT_DELTA_RENAMED		: return ImVec4(0.60f, 0.20f, 0.80f, 1.00f);
			case GIT_DELTA_COPIED 		: return ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
			case GIT_DELTA_IGNORED		: return ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
			case GIT_DELTA_UNTRACKED	: return ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
			case GIT_DELTA_TYPECHANGE	: return ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
			case GIT_DELTA_UNREADABLE	: return ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
			case GIT_DELTA_CONFLICTED	: return ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
			default						: return ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
		}
	}

	int GenerateDiff(git_commit* commit, std::vector<Diff>& out)
	{
		git_diff* diff = nullptr;
		git_commit* parent = nullptr;
		git_tree* commitTree = nullptr;
		git_tree* parentTree = nullptr;
		
		int err = git_commit_parent(&parent, commit, 0);
		if (err >= 0)
			err = git_commit_tree(&commitTree, commit);
		if (err >= 0)
			err = git_commit_tree(&parentTree, parent);

		if (err >= 0)
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

		return err;
	}


	struct Commit
	{
		git_commit* CommitPtr = nullptr;
		char CommitID[COMMIT_ID_LEN];

		std::string AuthorName;
		std::string AuthorEmail;
		std::string AuthorDateTime;
		std::string AuthorTimezoneOffset;

		std::string CommitterName;
		std::string CommitterEmail;
		std::string CommitterDateTime;
		std::string CommitterTimezoneOffset;

		std::string Message;
		std::string Description;
	};


	void GetCommit(git_commit* commit, Commit* out)
	{
		out->CommitPtr = commit;

		const git_oid* oid = git_commit_id(commit);
		strncpy_s(out->CommitID, git_oid_tostr_s(oid), COMMIT_ID_LEN);

		const git_signature* author = git_commit_author(commit);
		const git_signature* committer = git_commit_committer(commit);
		const char* commitSummary = git_commit_summary(commit);
		const char* commitDesc = git_commit_body(commit);

		out->AuthorName = author->name;
		out->AuthorEmail = author->email;
		char sign = committer->when.offset >= 0 ? '+' : '-';
		out->AuthorTimezoneOffset = std::format("{}{:02d}:{:02d}", sign, abs(author->when.offset) / 60, author->when.offset % 60);

		git_time_t timestamp = author->when.time;
		tm localTime;
		localtime_s(&localTime, &timestamp);
		char dateTime[COMMIT_DATE_LEN];
		strftime(dateTime, sizeof(dateTime), "%d %b %Y %H:%M:%S", &localTime);
		out->AuthorDateTime = dateTime;

		out->CommitterName = committer->name;
		out->CommitterEmail = committer->email;
		sign = committer->when.offset >= 0 ? '+' : '-';
		out->CommitterTimezoneOffset = std::format("{}{:02d}:{:02d}", sign, abs(committer->when.offset) / 60, committer->when.offset % 60);

		timestamp = committer->when.time;
		localtime_s(&localTime, &timestamp);
		strftime(dateTime, sizeof(dateTime), "%d %b %Y %H:%M:%S", &localTime);
		out->CommitterDateTime = dateTime;

		out->Message = commitSummary ? commitSummary : "";
		out->Description = commitDesc ? commitDesc : "";
	}

	void ShowRepoWindow(RepoData* repoData, bool* opened)
	{
		constexpr ImGuiTableColumnFlags columnFlags = ImGuiTableColumnFlags_NoHide | ImGuiTableColumnFlags_NoHeaderLabel;
		constexpr ImGuiTableFlags tooltipTableFlags = ImGuiTableFlags_SizingStretchProp;

		if (repoData)
		{
			enum class Action
			{
				None = 0,
				CreateBranch,
				CheckoutBranch,
				CheckoutCommit,
				Reset
			};

			Action action = Action::None;
			bool branchCreated = false;

			ImGui::Begin(repoData->Name.c_str(), opened);

			if (ImGui::IsItemHovered())
			{
				ImGui::BeginTooltip();

				if (ImGui::BeginTable("##RepositoryTooltipTable", 2, tooltipTableFlags))
				{
					ImGui::TableSetupColumn("CommitTooltipTableCol1", columnFlags | ImGuiTableColumnFlags_WidthFixed);
					ImGui::TableSetupColumn("CommitTooltipTableCol2", columnFlags);

					ImGui::TableNextRow();
					ImGui::TableNextColumn();

					ImGui::TextUnformatted("Repository Path:");
					ImGui::TextUnformatted("Uncommitted Files:");
					ImGui::TextUnformatted("Branches:");
					ImGui::TextUnformatted("Commits:");
					ImGui::TextUnformatted("Head:");

					ImGui::TableNextColumn();

					ImGui::TextUnformatted(repoData->Filepath.c_str());
					ImGui::Text("%u", repoData->UncommittedFiles);
					ImGui::Text("%u", repoData->Branches.size());
					ImGui::Text("%u", repoData->Commits.size());
					bool headFound = false;
					if (repoData->BranchHeads.find(repoData->Head) != repoData->BranchHeads.end())
					{
						const auto& branches = repoData->BranchHeads.at(repoData->Head);
						for (auto& branch : branches)
						{
							if (branch.Branch == repoData->HeadBranch)
							{
								ImGui::TextUnformatted(repoData->Branches.at(branch.Branch).Name.c_str());
								headFound = true;
								break;
							}
						}
					}
					if (!headFound)
					{
						uint64_t commitIndex = repoData->CommitsIndexMap.at(repoData->Head);
						ImGui::Text("%s (Detached)", repoData->Commits.at(commitIndex).CommitID);
					}

					ImGui::EndTable();
				}

				ImGui::EndTooltip();
			}

			if (ImGui::BeginTable("RepoViewTable", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_ContextMenuInBody | ImGuiTableFlags_NoPadInnerX | ImGuiTableFlags_NoPadOuterX))
			{
				ImGui::TableNextRow();
				ImGui::TableNextColumn();

				constexpr ImGuiTreeNodeFlags treeFlags = ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_DefaultOpen;
				if (ImGui::TreeNodeEx("Branches", treeFlags))
				{
					for (const auto& [branchRef, branchData] : repoData->Branches)
					{
						bool open = ImGui::TreeNodeEx(branchData.Name.c_str(), treeFlags | ImGuiTreeNodeFlags_Leaf);
						if (open)
							ImGui::TreePop();
					}
					ImGui::TreePop();
				}

				ImGui::TableNextColumn();

				ImGui::Indent();
				if (repoData->HeadBranch)
					ImGui::Text("%s %s", ICON_MDI_SOURCE_BRANCH, repoData->Branches.at(repoData->HeadBranch).Name.c_str());
				else
					ImGui::TextUnformatted("Detached HEAD");

				static ImGuiTextFilter filter;
				filter.Draw();

				git_checkout_options safeCheckoutOp = { GIT_CHECKOUT_OPTIONS_VERSION, GIT_CHECKOUT_SAFE | GIT_CHECKOUT_UPDATE_SUBMODULES };
				//safeCheckoutOp.progress_cb = checkout_progress;
				safeCheckoutOp.dir_mode = 0777;
				safeCheckoutOp.file_mode = 0777;
				git_checkout_options forceCheckoutOp = { GIT_CHECKOUT_OPTIONS_VERSION, GIT_CHECKOUT_FORCE | GIT_CHECKOUT_UPDATE_SUBMODULES };
				//forceCheckoutOp.progress_cb = checkout_progress;
				forceCheckoutOp.dir_mode = 0777;
				forceCheckoutOp.file_mode = 0777;

				constexpr ImGuiTableFlags tableFlags =
					ImGuiTableFlags_PadOuterX |
					ImGuiTableFlags_ContextMenuInBody |
					ImGuiTableFlags_NoHostExtendY |
					ImGuiTableFlags_ScrollY;

				static char* error = nullptr;
				const float lineHeightWithSpacing = ImGui::GetTextLineHeightWithSpacing();

				constexpr uint32_t maxRows = 25000;
				constexpr uint32_t startPage = 0;
				const uint32_t totalPages = static_cast<uint32_t>(repoData->Commits.size()) / maxRows;
				static uint32_t currentPage = 0;
				if (totalPages > 1)
					ImGui::SliderScalar("Pages", ImGuiDataType_U32, &currentPage, &startPage, &totalPages);

				ImGui::Unindent();
				ImGui::Spacing();

				if (ImGui::BeginTable(repoData->Name.c_str(), 4, tableFlags))
				{
					ImGuiStyle& style = ImGui::GetStyle();

					ImGui::TableSetupColumn("Message", columnFlags);
					ImGui::TableSetupColumn("AuthorName", columnFlags | ImGuiTableColumnFlags_WidthFixed);
					ImGui::TableSetupColumn("CommitID", columnFlags | ImGuiTableColumnFlags_WidthFixed);
					ImGui::TableSetupColumn("AuthorDate", columnFlags | ImGuiTableColumnFlags_WidthFixed);

					bool disabled = true;
					uint32_t start = maxRows * currentPage;
					uint32_t end = start + (currentPage == totalPages ? static_cast<uint32_t>(repoData->Commits.size()) % maxRows : maxRows);
					int row = 0;
					for (uint32_t i = start; i < end; ++i)
					{
						CommitData* data = &(repoData->Commits[i]);

						if (filter.IsActive() && !(filter.PassFilter(data->CommitID) || filter.PassFilter(data->Message) || filter.PassFilter(data->AuthorName)))
							continue;

						if (ImGui::IsWindowHovered() && ImGui::IsAnyMouseDown() && row == ImGui::TableGetHoveredRow())
							g_SelectedCommit = data;
						
						++row;

						ImGui::TableNextRow();
						ImGui::TableNextColumn();
						
						if (repoData->BranchHeads.find(data->ID) != repoData->BranchHeads.end())
						{
							std::vector<BranchData>& branches = repoData->BranchHeads.at(data->ID);
							for (BranchData& branch : branches)
							{
								const bool isHeadBranch = branch.Branch == repoData->HeadBranch;
								const char* branchName = repoData->Branches.at(branch.Branch).Name.c_str();
								ImVec2 size = ImGui::CalcTextSize(branchName);
								if (isHeadBranch)
									size.x += lineHeightWithSpacing;
								ImVec2 rectMin = ImGui::GetCurrentWindowRead()->DC.CursorPos;
								size.x += style.FramePadding.x;
								size.y += style.FramePadding.y * 0.75f;
								rectMin.x -= style.FramePadding.x * 0.5f;
								rectMin.y -= style.FramePadding.y * 0.5f;
								ImGui::RenderFrame(rectMin, rectMin + size, branch.Color, true, 2.0f);
								if (isHeadBranch)
								{
									ImGui::PushFont(g_BoldFont);
									ImGui::Text("%s%s", ICON_MDI_CHECK_ALL, branchName);
									ImGui::PopFont();
								}
								else
								{
									ImGui::TextUnformatted(branchName);
								}
								ImGui::SameLine(0, lineHeightWithSpacing);
							}
						}
						
						const bool isHead = data->ID == repoData->Head;
						if (!g_SelectedCommit && isHead)
							g_SelectedCommit = data;
						const bool selected = g_SelectedCommit && g_SelectedCommit->Commit == data->Commit;

						if (disabled && isHead)
							disabled = false;

						if (isHead)	ImGui::PushFont(g_BoldFont);
						if (selected) ImGui::PushStyleColor(ImGuiCol_Text, { 0.9f, 0.9f, 0.9f, 1.0f });
						ImGui::BeginDisabled(disabled);
						
						ImGui::TextUnformatted(data->Message);
						ImGui::TableNextColumn();
						ImGui::TextUnformatted(data->AuthorName);
						ImGui::TableNextColumn();
						ImGui::TextUnformatted(data->CommitID, data->CommitID + SHORT_SHA_LENGTH);
						ImGui::TableNextColumn();
						ImGui::TextUnformatted(data->AuthorDate, data->AuthorDate + strlen(data->AuthorDate) - 3);

						ImGui::EndDisabled();
						if (selected) ImGui::PopStyleColor();
						if (isHead)	ImGui::PopFont();

						if (selected)
						{
							ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, ImColor(0.14f, 0.42f, 0.82f, 1.0f));

							if (ImGui::IsWindowHovered() && ImGui::IsMouseReleased(1))
								ImGui::OpenPopup("CommitPopup", ImGuiPopupFlags_NoOpenOverExistingPopup);
							if (ImGui::BeginPopup("CommitPopup"))
							{
								if (repoData->BranchHeads.find(data->ID) != repoData->BranchHeads.end())
								{
									std::vector<BranchData>& branches = repoData->BranchHeads.at(data->ID);
									for (BranchData& branch : branches)
									{
										if (ImGui::MenuItem(repoData->Branches.at(branch.Branch).Name.c_str()))
										{
											Stopwatch sw("Branch Checkout");

											action = Action::CheckoutBranch;
											int err = git_checkout_tree(repoData->Repository, reinterpret_cast<git_object*>(data->Commit), &safeCheckoutOp);
											if (err >= 0)
												err = git_repository_set_head(repoData->Repository, repoData->Branches.at(branch.Branch).Name.c_str());

											if (err < 0)
												error = git_error_last()->message;
										}
									}
									ImGui::Separator();
								}
								if (ImGui::MenuItem("New Branch"))
								{
									action = Action::CreateBranch;
								}

								if (repoData->HeadBranch && !isHead)
								{
									char resetString[512];
									snprintf(resetString, 512, "Reset \"%s\" to here...", repoData->Branches.at(repoData->HeadBranch).Name.c_str());
									ImGui::Separator();
									if (ImGui::BeginMenu(resetString))
									{
										if (ImGui::MenuItem("Soft (Move the head to the given commit)"))
										{
											Stopwatch sw("Soft Reset");

											if (git_reset(repoData->Repository, reinterpret_cast<const git_object*>(data->Commit), GIT_RESET_SOFT, &safeCheckoutOp) < 0)
												error = git_error_last()->message;

											action = Action::Reset;
										}
										if (ImGui::MenuItem("Mixed (Soft + reset index to the commit)"))
										{
											Stopwatch sw("Mixed Reset");

											if (git_reset(repoData->Repository, reinterpret_cast<const git_object*>(data->Commit), GIT_RESET_MIXED, &safeCheckoutOp) < 0)
												error = git_error_last()->message;

											action = Action::Reset;
										}
										if (ImGui::MenuItem("Hard (Mixed + changes in working tree discarded)"))
										{
											Stopwatch sw("Hard Reset");

											if (git_reset(repoData->Repository, reinterpret_cast<const git_object*>(data->Commit), GIT_RESET_HARD, &forceCheckoutOp) < 0)
												error = git_error_last()->message;

											action = Action::Reset;
										}

										ImGui::EndMenu();
									}
								}

								ImGui::Separator();
								if (ImGui::MenuItem("Checkout Commit"))
								{
									action = Action::CheckoutCommit;
								}
								if (ImGui::MenuItem("Copy as Patch"))
								{
									git_buf buf = GIT_BUF_INIT;
									git_email_create_options op = GIT_EMAIL_CREATE_OPTIONS_INIT;
									if (git_email_create_from_commit(&buf, g_SelectedCommit->Commit, &op) == 0)
									{
										std::string diffStr = buf.ptr ? buf.ptr : "";
										const size_t patchEnd = diffStr.rfind("--");
										if (patchEnd != std::string::npos)
										{
											diffStr.resize(patchEnd + 2, '\0');
											diffStr += "\nQuickGit";
											diffStr += " 0.0.1";
											diffStr += "\n\n";
										}
										git_buf_free(&buf);
										ImGui::SetClipboardText(diffStr.c_str());
									}
								}

								ImGui::Separator();
								if (ImGui::MenuItem("Copy Commit SHA"))
								{
									ImGui::SetClipboardText(g_SelectedCommit->CommitID);
								}
								if (ImGui::MenuItem("Copy Commit Info"))
								{
									Commit c;
									GetCommit(g_SelectedCommit->Commit, &c);
									char shortSHA[8];
									strncpy_s(shortSHA, c.CommitID, SHORT_SHA_LENGTH);
									std::string info = "SHA: ";
									info += shortSHA;
									info += "\nAuthor: ";
									info += c.AuthorName;
									info += " (";
									info += c.AuthorEmail;
									info += ")\nDate: ";
									info += c.AuthorDateTime;
									info += "\nMessage: ";
									info += c.Message;
									info += "\n";
									info += c.Description;
									ImGui::SetClipboardText(info.c_str());
								}

								ImGui::EndPopup();
							}
						}
					}

					ImGui::EndTable();
				}

				ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 16.0f, 16.0f });

				if (g_SelectedCommit)
				{
					if (action == Action::CreateBranch)
					{
						ImGui::OpenPopup("Create Branch");
					}
					if (action == Action::CheckoutCommit)
					{
						ImGui::OpenPopup("Checkout Commit");
					}
					if (action == Action::CheckoutBranch)
					{
						UpdateHead(*repoData);
					}

					if (ShowModal("Create Branch"))
					{
						ImGui::TextDisabled("Use '/' as a path separator to create folders");

						if (ImGui::BeginTable("BranchTable", 2))
						{
							ImGui::TableSetupColumn("Col1", columnFlags | ImGuiTableColumnFlags_WidthFixed);
							ImGui::TableSetupColumn("Col2", columnFlags);

							ImGui::TableNextRow();
							ImGui::TableNextColumn();

							ImGui::TextUnformatted("Create Branch at:");
							ImGui::TableNextColumn();
							ImGui::Text("%s %.*s", ICON_MDI_SOURCE_COMMIT, SHORT_SHA_LENGTH, g_SelectedCommit->CommitID);
							ImGui::SameLine();
							ImGui::TextWrapped("%s", g_SelectedCommit->Message);

							ImGui::TableNextRow();
							ImGui::TableNextColumn();

							ImGui::TextUnformatted("Branch name:");
							ImGui::TableNextColumn();
							ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
							static char branchName[256] = "";
							ImGui::InputTextWithHint("##NewBranchName", "Enter branch name", branchName, 256);

							ImGui::TableNextRow();
							ImGui::TableNextColumn();
							ImGui::TableNextColumn();

							static bool checkoutAfterCreate = false;
							ImGui::Checkbox("Check out after create", &checkoutAfterCreate);

							ImGui::TableNextRow();
							ImGui::TableNextColumn();
							ImGui::TableNextColumn();

							if (ImGui::Button("Create"))
							{
								git_reference* outBranch = nullptr;
								int err = git_branch_create(&outBranch, repoData->Repository, branchName, g_SelectedCommit->Commit, 0);

								if (outBranch)
									branchCreated = true;

								if (err >= 0 && checkoutAfterCreate)
									err = git_checkout_tree(repoData->Repository, reinterpret_cast<git_object*>(g_SelectedCommit->Commit), &safeCheckoutOp);

								if (err >= 0 && checkoutAfterCreate)
								{
									char brName[512];
									snprintf(brName, 512, "refs/heads/%s", branchName);
									err = git_repository_set_head(repoData->Repository, brName);
									UpdateHead(*repoData);
								}

								if (err < 0)
									error = git_error_last()->message;

								memset(branchName, 0, 256);
								ImGui::CloseCurrentPopup();
							}

							ImGui::SameLine();

							if (ImGui::Button("Cancel"))
							{
								ImGui::CloseCurrentPopup();
							}

							ImGui::EndTable();
						}

						ImGui::EndPopup();
					}

					if (ShowModal("Checkout Commit"))
					{
						ImGui::TextUnformatted("Checkout a particular revision. Repository will be in detached HEAD state.");

						const float lineHeight = ImGui::GetTextLineHeight();
						ImGui::SetCursorPosY(ImGui::GetCursorPosY() + lineHeight);

						ImGui::TextUnformatted("Commit to checkout:");
						ImGui::SameLine();
						ImGui::Text("%s %.*s", ICON_MDI_SOURCE_COMMIT, SHORT_SHA_LENGTH, g_SelectedCommit->CommitID);
						ImGui::SameLine();
						ImGui::TextWrapped("%s", g_SelectedCommit->Message);

						ImGui::SetCursorPosY(ImGui::GetCursorPosY() + lineHeight);

						if (ImGui::Button("Checkout Commit"))
						{
							int err = git_checkout_tree(repoData->Repository, reinterpret_cast<git_object*>(g_SelectedCommit->Commit), &safeCheckoutOp);
							if (err >= 0)
								err = git_repository_set_head_detached(repoData->Repository, git_commit_id(g_SelectedCommit->Commit));

							if (err < 0)
								error = git_error_last()->message;

							UpdateHead(*repoData);

							ImGui::CloseCurrentPopup();
						}

						ImGui::SameLine();

						if (ImGui::Button("Cancel"))
						{
							ImGui::CloseCurrentPopup();
						}

						ImGui::EndPopup();
					}
				}

				// Error Window
				{
					static char gitErrorStr[2048] = "";
					if (error && gitErrorStr[0] == '\0')
					{
						strncpy_s(gitErrorStr, error, strlen(error));
						ImGui::OpenPopup("Error");
					}

					if (gitErrorStr[0] != '\0')
					{
						const ImVec2 center = ImGui::GetMainViewport()->GetCenter();
						ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
						if (ImGui::BeginPopupModal("Error"))
						{
							ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
							ImGui::InputText("##GitError", gitErrorStr, strlen(gitErrorStr), ImGuiInputTextFlags_ReadOnly);

							if (ImGui::Button("OK", ImVec2(120, 0)))
							{
								gitErrorStr[0] = '\0';
								error = nullptr;
								ImGui::CloseCurrentPopup();
							}

							ImGui::EndPopup();
						}
					}
				}

				ImGui::PopStyleVar();

				ImGui::EndTable();
			}

			ImGui::End();

			if (branchCreated || action == Action::Reset)
				Fill(repoData, repoData->Repository);
		}
	}

	void ImGuiRender()
	{
		ImGui::DockSpaceOverViewport(ImGui::GetMainViewport());

		ImGui::Begin("Add Repo");
		ImGui::InputText("Path", g_Path, 2048);
		if (ImGui::Button("Open"))
		{
			ClientInitRepo(g_Path);
		}
		ImGui::End();





		ImGui::Begin("Repositories");
		std::vector<git_repository*>& repos = ClientGetRepositories();
		for (git_repository* repo : repos)
		{
			const char* p = git_repository_workdir(repo);
			bool open = ImGui::TreeNodeEx(p, ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_Leaf);
			if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
			{
				std::unique_ptr<RepoData> data = std::make_unique<RepoData>();
				Fill(data.get(), repo);
				g_SelectedRepos.emplace_back(std::move(data));
			}

			if (open)
			{
				ImGui::TreePop();
			}
		}
		ImGui::End();



		for (std::vector<std::unique_ptr<RepoData>>::iterator it = g_SelectedRepos.begin(); it != g_SelectedRepos.end(); ++it)
		{
			RepoData* repoData = it->get();
			bool opened = repoData;
			ShowRepoWindow(repoData, &opened);
			if (!opened)
			{
				g_SelectedRepos.erase(it);
				break;
			}
		}





		ImGui::Begin("Commit");
		ImGui::Indent();
		static Commit cd;
		static std::vector<Diff> diffs;

		if (g_SelectedCommit && g_SelectedCommit->Commit != cd.CommitPtr)
		{
			GetCommit(g_SelectedCommit->Commit, &cd);
			diffs.clear();
			GenerateDiff(g_SelectedCommit->Commit, diffs);
		}

		if (cd.CommitPtr)
		{
			if (ImGui::BeginTable("CommitTopTable", 2))
			{
				ImGui::TableNextRow();
				ImGui::TableNextColumn();

				ImGui::TextUnformatted("AUTHOR");
				ImGui::Spacing();
				ImGui::PushFont(g_BoldFont);
				ImGui::TextUnformatted(cd.AuthorName.c_str());
				ImGui::PopFont();
				ImGui::SameLine();
				ImGui::TextUnformatted(cd.AuthorEmail.c_str());
				ImGui::TextUnformatted(cd.AuthorDateTime.c_str());
				ImGui::SameLine();
				ImGui::TextUnformatted(cd.AuthorTimezoneOffset.c_str());

				ImGui::TableNextColumn();

				ImGui::TextUnformatted("COMMITTER");
				ImGui::Spacing();
				ImGui::PushFont(g_BoldFont);
				ImGui::TextUnformatted(cd.CommitterName.c_str());
				ImGui::PopFont();
				ImGui::SameLine();
				ImGui::TextUnformatted(cd.CommitterEmail.c_str());
				ImGui::TextUnformatted(cd.CommitterDateTime.c_str());
				ImGui::SameLine();
				ImGui::TextUnformatted(cd.CommitterTimezoneOffset.c_str());

				ImGui::EndTable();
			}

			ImGui::Spacing();
			ImGui::Text("SHA %s", cd.CommitID);
			ImGui::Spacing();

			ImGui::Separator();

			ImGui::Spacing();
			ImGui::PushFont(g_HeadingFont);
			ImGui::TextWrapped(cd.Message.c_str());
			ImGui::PopFont();
			if (!cd.Description.empty())
			{
				ImGui::TextWrapped(cd.Description.c_str());
			}
			ImGui::Spacing();

			ImGui::Separator();

			ImGui::Spacing();
			for (auto& diff : diffs)
			{
				ImGui::PushStyleColor(ImGuiCol_Text, GetPatchStatusColor(diff.Status));
				bool open = ImGui::TreeNodeEx(diff.File.c_str(), ImGuiTreeNodeFlags_SpanAvailWidth);
				ImGui::PopStyleColor();
				if (open)
				{
					float frameHeight = ImGui::GetFrameHeightWithSpacing();
					ImGui::Indent(frameHeight);
					if (diff.Patch == "@@BinaryData")
					{
						ImGui::TextUnformatted(diff.Patch.c_str());
						if (diff.NewFileSize)
						{
							ImGui::Indent(frameHeight);
							if (diff.OldFileSize)
							{
								ImGui::PushStyleColor(ImGuiCol_Text, GetPatchStatusColor(GIT_DELTA_DELETED));
								ImGui::Text("Old: %u Bytes", diff.OldFileSize);
								ImGui::PopStyleColor();
								ImGui::SameLine(0, frameHeight);
							}

							ImGui::PushStyleColor(ImGuiCol_Text, GetPatchStatusColor(GIT_DELTA_ADDED));
							ImGui::Text("New: %u Bytes", diff.NewFileSize);
							ImGui::PopStyleColor();
							ImGui::Unindent(frameHeight);
						}
					}
					else
					{
						ImVec2 size = ImGui::CalcTextSize(diff.Patch.c_str());
						size.y += ImGui::GetFrameHeightWithSpacing();
						ImGui::InputTextMultiline(diff.File.c_str(), &diff.Patch[0], diff.Patch.size(), { ImGui::GetContentRegionAvail().x, size.y }, ImGuiInputTextFlags_ReadOnly);
					}

					ImGui::Unindent(frameHeight);
					ImGui::TreePop();
				}
			}
		}
		ImGui::Unindent();
		ImGui::End();
		
		static bool show_demo_window = true;
		if (show_demo_window)
			ImGui::ShowDemoWindow(&show_demo_window);
	}

	void ImGuiRun()
	{
		ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
		while (!glfwWindowShouldClose(g_Window))
		{
			glfwPollEvents();

			ImGui_ImplOpenGL3_NewFrame();
			ImGui_ImplGlfw_NewFrame();
			ImGui::NewFrame();

			ImGuiRender();

			ImGui::Render();
			int display_w, display_h;
			glfwGetFramebufferSize(g_Window, &display_w, &display_h);
			glViewport(0, 0, display_w, display_h);
			glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
			glClear(GL_COLOR_BUFFER_BIT);
			ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

			ImGuiIO& io = ImGui::GetIO(); (void)io;
			if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
			{
				GLFWwindow* backup_current_context = glfwGetCurrentContext();
				ImGui::UpdatePlatformWindows();
				ImGui::RenderPlatformWindowsDefault();
				glfwMakeContextCurrent(backup_current_context);
			}

			glfwSwapBuffers(g_Window);
		}
	}
}
