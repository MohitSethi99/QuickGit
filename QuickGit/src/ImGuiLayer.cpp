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
#define COMMIT_ID_LEN 64
#define COMMIT_MSG_LEN 256
#define COMMIT_DESC_LEN 2048
#define COMMIT_NAME_EMAIL_LEN 128
#define COMMIT_DATE_LEN 32
#define COMMIT_TIMEZONE_LEN 16

struct DescendingComparator
{
	bool operator()(const git_time_t& a, const git_time_t& b) const
	{
		return a > b;
	}
};

namespace QuickGit
{
	struct CommitData
	{
		git_commit* Commit;

		char CommitID[COMMIT_ID_LEN];
		char Message[COMMIT_MSG_LEN];
		char Description[COMMIT_DESC_LEN];
		char AuthorName[COMMIT_NAME_EMAIL_LEN];
		char AuthorEmail[COMMIT_NAME_EMAIL_LEN];
		char AuthorDate[COMMIT_DATE_LEN];
		char AuthorTimezoneOffset[COMMIT_TIMEZONE_LEN];
		char CommitterName[COMMIT_NAME_EMAIL_LEN];
		char CommitterEmail[COMMIT_NAME_EMAIL_LEN];
		char CommitterDate[COMMIT_DATE_LEN];
		char CommitterTimezoneOffset[COMMIT_TIMEZONE_LEN];
	};

	enum class BranchType { Remote, Local };

	struct BranchData
	{
		git_reference* Branch;

		BranchType Type;
		uint32_t Color;
	};

	struct RepoData
	{
		git_repository* Repository = nullptr;

		std::string Name{};
		std::string Filepath{};
		size_t UncommittedFiles = 0;

		char Head[COMMIT_ID_LEN]{};
		git_reference* HeadBranch = nullptr;
		std::unordered_map<git_reference*, std::string> Branches;
		std::unordered_map<std::string, std::vector<BranchData>> BranchHeads;
		std::map<git_time_t, std::unique_ptr<CommitData>, DescendingComparator> Commits;

		~RepoData()
		{
			for (auto& [_, commitData] : Commits)
				git_commit_free(commitData->Commit);

			for (auto& [branchRef, _] : Branches)
				git_reference_free(branchRef);
		}
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

		constexpr const char* regularFontPath = "res/fonts/jetbrains-mono/JetBrainsMono-Light.ttf";
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
		git_oid_tostr(repoData.Head, GIT_OID_HEXSZ + 1, id);
		git_reference_free(ref);

		repoData.HeadBranch = nullptr;
		for (const auto& [branchRef, name] : repoData.Branches)
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

		for (auto& [_, commitData] : data->Commits)
			git_commit_free(commitData->Commit);

		for (auto& [branchRef, _] : data->Branches)
			git_reference_free(branchRef);

		data->Commits.clear();
		data->Branches.clear();
		data->BranchHeads.clear();

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
					data->Branches[ref] = refName;
					BranchType type = data->Branches[ref].starts_with("refs/remotes") ? BranchType::Remote : BranchType::Local;
					data->BranchHeads[git_oid_tostr_s(targetId)].emplace_back(ref, type, GenerateColor(refName));
				}

				git_revwalk* walker;
				git_revwalk_new(&walker, repo);
				git_revwalk_push(walker, git_commit_id(targetCommit));

				[[maybe_unused]] const char* targetCommitMessage = git_commit_message(targetCommit);

				git_oid oid;
				while (git_revwalk_next(&oid, walker) == 0)
				{
					git_commit* commit = nullptr;
					if (git_commit_lookup(&commit, repo, &oid) == 0)
					{
						const git_signature* author = git_commit_author(commit);
						const git_signature* committer = git_commit_committer(commit);
						const char* commitSummary = git_commit_summary(commit);
						const char* commitDesc = git_commit_body(commit);

						std::unique_ptr<CommitData> cd = std::make_unique<CommitData>();
						cd->Commit = commit;

						strcpy_s(cd->CommitID, git_oid_tostr_s(&oid));

						if (commitSummary)
							strncpy_s(cd->Message, commitSummary, sizeof(cd->Message) - 1);

						if (commitDesc)
							strncpy_s(cd->Description, commitDesc, sizeof(cd->Description) - 1);

						strcpy_s(cd->AuthorName, author->name);
						strcpy_s(cd->AuthorEmail, author->email);
						sprintf_s(cd->AuthorTimezoneOffset, "%c%d:%d", author->when.sign, author->when.offset / 60, author->when.offset % 60);
						strcpy_s(cd->CommitterName, committer->name);
						strcpy_s(cd->CommitterEmail, committer->email);
						sprintf_s(cd->CommitterTimezoneOffset, "%c%d:%d", committer->when.sign, committer->when.offset / 60, committer->when.offset % 60);

						git_time_t timestamp = author->when.time;
						tm localTime;
						localtime_s(&localTime, &timestamp);
						strftime(cd->AuthorDate, sizeof(cd->AuthorDate), "%d %b %Y %H:%M:%S", &localTime);

						timestamp = committer->when.time;
						localtime_s(&localTime, &timestamp);
						strftime(cd->CommitterDate, sizeof(cd->CommitterDate), "%d %b %Y %H:%M:%S", &localTime);

						data->Commits[author->when.time] = std::move(cd);
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

	void ShowRepoWindow(RepoData* repoData, bool* opened)
	{
		if (repoData)
		{
			ImGui::Begin(repoData->Name.c_str(), opened);

			if (repoData->HeadBranch)
				ImGui::Text("%s %s", ICON_MDI_SOURCE_BRANCH, repoData->Branches.at(repoData->HeadBranch).c_str());
			else
				ImGui::TextUnformatted("Detached HEAD");

			/*
			static git_reference* selectedBranch = nullptr;
			ImGui::Text(repoData->Name.c_str(), "");
			ImGui::Text("%s", repoData->Filepath.c_str());
			ImGui::Text("Uncommitted Files: %d", repoData->UncommittedFiles);
			ImGui::Text("Commits: %d", repoData->Commits.size());

			ImGui::BeginChild("Branches", { 500, ImGui::GetContentRegionAvail().y }, true, ImGuiWindowFlags_AlwaysAutoResize);
			for (auto [branchRef, branchName] : repoData->Branches)
			{
				static size_t refsLen = strlen("refs/");

				//const char* name = &branchName[branchName.find_last_of('/') + 1];
				//const std::string_view head = &branchName[refsLen];
				//const size_t firstSlash = head.find_first_of('/', 0);
				//branchName[refsLen + firstSlash - 1] = '\0';
				//const std::string_view headName = head.substr(0, firstSlash);

				//if (ImGui::TreeNodeEx(headName.data(), ImGuiTreeNodeFlags_Framed, "%s", headName.data()))
				{
					bool open = ImGui::TreeNodeEx(branchName.c_str(), ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_Framed | ((selectedBranch == branchRef) ? ImGuiTreeNodeFlags_Selected : 0));

					if (ImGui::BeginPopupContextItem("#BranchPopup"))
					{
						if (ImGui::Button("Checkout"))
						{
							git_commit* commit = nullptr;
							[[maybe_unused]] int e = git_commit_lookup(&commit, repoData->Repository, git_reference_target(branchRef));

							git_tree* tree = nullptr;
							e = git_commit_tree(&tree, commit);

							e = git_checkout_tree(repoData->Repository, reinterpret_cast<git_object*>(tree), nullptr);

							git_commit_free(commit);
							git_tree_free(tree);
						}

						ImGui::EndPopup();
					}

					if (ImGui::IsItemActive())
					{
						selectedBranch = branchRef;
					}

					if (open)
						ImGui::TreePop();

					//ImGui::TreePop();
				}
			}
			ImGui::EndChild();
			*/
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
				ImGuiTableFlags_ScrollY;

			constexpr ImGuiTableColumnFlags columnFlags =
				ImGuiTableColumnFlags_NoHide |
				ImGuiTableColumnFlags_NoHeaderLabel;

			enum class Action
			{
				None = 0,
				CreateBranch,
				CheckoutBranch,
				CheckoutCommit,
				Reset
			};

			Action action = Action::None;
			static char* error = nullptr;
			const float lineHeightWithSpacing = ImGui::GetTextLineHeightWithSpacing();

			if (ImGui::BeginTable(repoData->Name.c_str(), 4, tableFlags))
			{
				ImGuiStyle& style = ImGui::GetStyle();

				ImGui::TableSetupColumn("Message", columnFlags);
				ImGui::TableSetupColumn("AuthorName", columnFlags | ImGuiTableColumnFlags_WidthFixed);
				ImGui::TableSetupColumn("CommitID", columnFlags | ImGuiTableColumnFlags_WidthFixed);
				ImGui::TableSetupColumn("AuthorDate", columnFlags | ImGuiTableColumnFlags_WidthFixed);

				bool disabled = true;
				int row = 0;
				for (const auto& [_, data] : repoData->Commits)
				{
					if (filter.IsActive() && !(filter.PassFilter(data->CommitID) || filter.PassFilter(data->Message) || filter.PassFilter(data->Description) || filter.PassFilter(data->AuthorName) || filter.PassFilter(data->AuthorEmail)))
						continue;

					if (ImGui::IsWindowFocused() && ImGui::IsAnyMouseDown() && row == ImGui::TableGetHoveredRow())
						g_SelectedCommit = data.get();

					++row;
					ImGui::TableNextRow();
					ImGui::TableNextColumn();
					if (repoData->BranchHeads.find(data->CommitID) != repoData->BranchHeads.end())
					{
						std::vector<BranchData>& branches = repoData->BranchHeads.at(data->CommitID);
						for (BranchData& branch : branches)
						{
							const bool isHeadBranch = branch.Branch == repoData->HeadBranch;
							const char* branchName = repoData->Branches.at(branch.Branch).c_str();
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
					
					bool isHead = strcmp(data->CommitID, repoData->Head) == 0;
					if (disabled && isHead)
						disabled = false;
					
					if (isHead)
						ImGui::PushFont(g_BoldFont);

					ImGui::BeginDisabled(disabled);
					ImGui::TextUnformatted(data->Message);
					ImGui::TableNextColumn();
					ImGui::TextUnformatted(data->AuthorName);
					ImGui::TableNextColumn();
					ImGui::Text("%.*s", SHORT_SHA_LENGTH, data->CommitID);
					ImGui::TableNextColumn();
					ImGui::TextUnformatted(data->AuthorDate, data->AuthorDate + strlen(data->AuthorDate) - 3);
					ImGui::EndDisabled();

					if (isHead)
						ImGui::PopFont();

					if (g_SelectedCommit == data.get())
					{
						ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, ImColor(0.14f, 0.42f, 0.82f, 1.0f));

						if (ImGui::IsMouseReleased(1))
							ImGui::OpenPopup("CommitPopup", ImGuiPopupFlags_NoOpenOverExistingPopup);
						if (ImGui::BeginPopup("CommitPopup"))
						{
							if (repoData->BranchHeads.find(data->CommitID) != repoData->BranchHeads.end())
							{
								std::vector<BranchData>& branches = repoData->BranchHeads.at(data->CommitID);
								for (BranchData& branch : branches)
								{
									if (ImGui::MenuItem(repoData->Branches.at(branch.Branch).c_str()))
									{
										Stopwatch sw("Branch Checkout");

										action = Action::CheckoutBranch;
										int err = git_checkout_tree(repoData->Repository, reinterpret_cast<git_object*>(data->Commit), &safeCheckoutOp);
										if (err >= 0)
											err = git_repository_set_head(repoData->Repository, repoData->Branches.at(branch.Branch).c_str());

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
								snprintf(resetString, 512, "Reset \"%s\" to here...", repoData->Branches.at(repoData->HeadBranch).c_str());
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
							
							ImGui::Separator();
							if (ImGui::MenuItem("Copy Commit SHA"))
							{
								ImGui::SetClipboardText(g_SelectedCommit->CommitID);
							}
							if (ImGui::MenuItem("Copy Commit Info"))
							{
								char shortSHA[8];
								strncpy_s(shortSHA, g_SelectedCommit->CommitID, SHORT_SHA_LENGTH);
								std::string info = "SHA: ";
								info += shortSHA;
								info += "\nAuthor: ";
								info += g_SelectedCommit->AuthorName;
								info += " (";
								info += g_SelectedCommit->AuthorEmail;
								info += ")\nDate: ";
								info += g_SelectedCommit->AuthorDate;
								info += "\nMessage: ";
								info += g_SelectedCommit->Message;
								info += "\n";
								info += g_SelectedCommit->Description;
								ImGui::SetClipboardText(info.c_str());
							}

							ImGui::EndPopup();
						}
					}
				}

				ImGui::EndTable();
			}

			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 16.0f, 16.0f });

			bool branchCreated = false;
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




		ImGui::Begin("No Repository Selected");
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
