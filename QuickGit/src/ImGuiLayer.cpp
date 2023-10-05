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

#include "Timer.h"
#include "Client.h"

#include "ImGuiExt.h"

namespace QuickGit
{
	GLFWwindow* g_Window;
	char g_Path[2048];
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

		Client::Init();

		memset(g_Path, 0, 2048);

	#if 1
		Client::InitRepo("G:/Projects/ArcEngine");
		Client::InitRepo("G:/Projects/MohitSethi99.github.io");
	#endif

		return true;
	}
	
	void ImGuiShutdown()
	{
		Client::Shutdown();

		ImGui_ImplOpenGL3_Shutdown();
		ImGui_ImplGlfw_Shutdown();
		ImGui::DestroyContext();

		glfwDestroyWindow(g_Window);
		glfwTerminate();
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
								ImGui::TextUnformatted(branch.ShortName());
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
					ImGui::Text("%s %s", ICON_MDI_SOURCE_BRANCH, repoData->Branches.at(repoData->HeadBranch).ShortName());
				else
					ImGui::TextUnformatted("Detached HEAD");

				static ImGuiTextFilter filter;
				filter.Draw();



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
								const char* branchName = branch.ShortName();
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
						
						ImGuiExt::TextEllipsis(data->Message, {885, 0});
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
										if (ImGui::MenuItem(branch.ShortName()))
										{
											action = Action::CheckoutBranch;
											if (!Client::CheckoutBranch(branch.Branch))
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
									snprintf(resetString, 512, "Reset \"%s\" to here...", repoData->Branches.at(repoData->HeadBranch).ShortName());
									ImGui::Separator();
									if (ImGui::BeginMenu(resetString))
									{
										if (ImGui::MenuItem("Soft (Move the head to the given commit)"))
										{
											if (!Client::Reset(data->Commit, GIT_RESET_SOFT))
												error = git_error_last()->message;

											action = Action::Reset;
										}
										if (ImGui::MenuItem("Mixed (Soft + reset index to the commit)"))
										{
											if (!Client::Reset(data->Commit, GIT_RESET_MIXED))
												error = git_error_last()->message;

											action = Action::Reset;
										}
										if (ImGui::MenuItem("Hard (Mixed + changes in working tree discarded)"))
										{
											if (!Client::Reset(data->Commit, GIT_RESET_HARD))
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
									std::string patch;
									if (Client::CreatePatch(g_SelectedCommit->Commit, patch))
										ImGui::SetClipboardText(patch.c_str());
									else
										error = git_error_last()->message;
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
						Client::UpdateHead(*repoData);
					}

					if (ImGuiExt::BeginPopupModal("Create Branch"))
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
							ImGuiExt::TextEllipsis(g_SelectedCommit->Message);

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
								git_reference* branch = Client::CreateBranch(branchName, g_SelectedCommit->Commit);
								branchCreated = branch;
								bool success = branch;
								if (branch && checkoutAfterCreate)
									success = Client::CheckoutBranch(branch);

								if (!success)
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

						ImGuiExt::EndPopupModal();
					}

					if (ImGuiExt::BeginPopupModal("Checkout Commit"))
					{
						ImGui::TextUnformatted("Checkout a particular revision. Repository will be in detached HEAD state.");

						const float lineHeight = ImGui::GetTextLineHeight();
						ImGui::SetCursorPosY(ImGui::GetCursorPosY() + lineHeight);

						ImGui::TextUnformatted("Commit to checkout:");
						ImGui::SameLine();
						ImGui::Text("%s %.*s", ICON_MDI_SOURCE_COMMIT, SHORT_SHA_LENGTH, g_SelectedCommit->CommitID);
						ImGui::SameLine();
						ImGuiExt::TextEllipsis(g_SelectedCommit->Message);

						ImGui::SetCursorPosY(ImGui::GetCursorPosY() + lineHeight);

						if (ImGui::Button("Checkout Commit"))
						{
							if (!Client::CheckoutCommit(g_SelectedCommit->Commit, false))
								error = git_error_last()->message;

							Client::UpdateHead(*repoData);

							ImGui::CloseCurrentPopup();
						}

						ImGui::SameLine();

						if (ImGui::Button("Cancel"))
						{
							ImGui::CloseCurrentPopup();
						}

						ImGuiExt::EndPopupModal();
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
				Client::Fill(repoData, repoData->Repository);
		}
	}

	void ImGuiRender()
	{
		ImGui::DockSpaceOverViewport(ImGui::GetMainViewport());

		ImGui::Begin("Add Repo");
		ImGui::InputText("Path", g_Path, 2048);
		if (ImGui::Button("Open"))
		{
			Client::InitRepo(g_Path);
		}
		ImGui::End();





		ImGui::Begin("Repositories");
		std::vector<std::unique_ptr<RepoData>>& repos = Client::GetRepositories();
		for (const std::unique_ptr<RepoData>& repo : repos)
		{
			bool open = ImGui::TreeNodeEx(repo->Filepath.c_str(), ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_Leaf);
			if (open) ImGui::TreePop();
		}
		ImGui::End();

		for (std::vector<std::unique_ptr<RepoData>>::iterator it = repos.begin(); it != repos.end(); ++it)
		{
			RepoData* repoData = it->get();
			bool opened = repoData;
			ShowRepoWindow(repoData, &opened);
			if (!opened)
			{
				repos.erase(it);
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
			Client::GenerateDiff(g_SelectedCommit->Commit, diffs);
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
