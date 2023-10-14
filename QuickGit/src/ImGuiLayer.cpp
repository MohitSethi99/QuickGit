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
	static RepoData* s_SelectedRepository = nullptr;

	ImFont* g_DefaultFont = nullptr;
	ImFont* g_SmallFont = nullptr;
	ImFont* g_HeadingFont = nullptr;
	ImFont* g_BoldFont = nullptr;
	ImGuiTextFilter BranchFilter;
	ImGuiTextFilter CommitsFilter;

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

	void SetTheme(bool dark)
	{
		ImVec4* colors = ImGui::GetStyle().Colors;
		if (dark)
		{
			colors[ImGuiCol_Text] = ImVec4(0.82f, 0.82f, 0.82f, 1.00f);
			colors[ImGuiCol_TextDisabled] = ImVec4(0.42f, 0.42f, 0.42f, 1.00f);
			colors[ImGuiCol_WindowBg] = ImVec4(0.17f, 0.17f, 0.18f, 1.00f);
			colors[ImGuiCol_ChildBg] = ImVec4(0.13f, 0.13f, 0.13f, 1.00f);
			colors[ImGuiCol_PopupBg] = ImVec4(0.169f, 0.169f, 0.180f, 1.000f);
			colors[ImGuiCol_Border] = ImVec4(0.275f, 0.275f, 0.275f, 1.000f);
			colors[ImGuiCol_BorderShadow] = ImVec4(0.275f, 0.275f, 0.275f, 0.00f);
			colors[ImGuiCol_FrameBg] = ImVec4(0.22f, 0.22f, 0.22f, 1.00f);
			colors[ImGuiCol_FrameBgHovered] = ImVec4(0.28f, 0.28f, 0.28f, 1.00f);
			colors[ImGuiCol_FrameBgActive] = ImVec4(0.26f, 0.26f, 0.26f, 1.00f);
			colors[ImGuiCol_TitleBg] = ImVec4(0.1f, 0.1f, 0.1f, 1.00f);
			colors[ImGuiCol_TitleBgActive] = ImVec4(0.1f, 0.1f, 0.1f, 1.00f);
			colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.1f, 0.1f, 0.1f, 1.00f);
			colors[ImGuiCol_MenuBarBg] = ImVec4(0.1f, 0.1f, 0.1f, 1.00f);
			colors[ImGuiCol_ScrollbarBg] = ImVec4(0.10f, 0.10f, 0.10f, 0.00f);
			colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.34f, 0.34f, 0.34f, 1.00f);
			colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.588f, 0.588f, 0.588f, 1.000f);
			colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.43f, 0.43f, 0.43f, 1.00f);
			colors[ImGuiCol_CheckMark] = ImVec4(0.00f, 0.44f, 0.88f, 1.00f);
			colors[ImGuiCol_SliderGrab] = ImVec4(0.000f, 0.434f, 0.878f, 1.000f);
			colors[ImGuiCol_SliderGrabActive] = ImVec4(0.000f, 0.434f, 0.878f, 1.000f);
			colors[ImGuiCol_Button] = ImVec4(0.22f, 0.22f, 0.22f, 1.00f);
			colors[ImGuiCol_ButtonHovered] = ImVec4(0.28f, 0.28f, 0.28f, 1.00f);
			colors[ImGuiCol_ButtonActive] = ImVec4(0.000f, 0.439f, 0.878f, 0.824f);
			colors[ImGuiCol_Header] = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
			colors[ImGuiCol_HeaderHovered] = ImVec4(0.27f, 0.27f, 0.27f, 1.00f);
			colors[ImGuiCol_HeaderActive] = ImVec4(0.27f, 0.27f, 0.27f, 1.00f);
			colors[ImGuiCol_Separator] = ImVec4(0.275f, 0.275f, 0.275f, 1.000f);
			colors[ImGuiCol_SeparatorHovered] = ImVec4(0.275f, 0.275f, 0.275f, 1.000f);
			colors[ImGuiCol_SeparatorActive] = ImVec4(0.275f, 0.275f, 0.275f, 1.000f);
			colors[ImGuiCol_ResizeGrip] = ImVec4(0.471f, 0.471f, 0.471f, 1.000f);
			colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.471f, 0.471f, 0.471f, 1.000f);
			colors[ImGuiCol_ResizeGripActive] = ImVec4(0.471f, 0.471f, 0.471f, 1.000f);
			colors[ImGuiCol_Tab] = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
			colors[ImGuiCol_TabHovered] = ImVec4(0.19f, 0.19f, 0.19f, 1.00f);
			colors[ImGuiCol_TabActive] = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
			colors[ImGuiCol_TabUnfocused] = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
			colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.169f, 0.169f, 0.180f, 1.000f);
			colors[ImGuiCol_DockingPreview] = ImVec4(0.19f, 0.53f, 0.78f, 0.22f);
			colors[ImGuiCol_DockingEmptyBg] = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
			colors[ImGuiCol_PlotLines] = ImVec4(0.00f, 0.44f, 0.88f, 1.00f);
			colors[ImGuiCol_PlotLinesHovered] = ImVec4(0.00f, 0.47f, 0.94f, 1.00f);
			colors[ImGuiCol_PlotHistogram] = ImVec4(0.00f, 0.44f, 0.88f, 1.00f);
			colors[ImGuiCol_PlotHistogramHovered] = ImVec4(0.00f, 0.47f, 0.94f, 1.00f);
			colors[ImGuiCol_TableHeaderBg] = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
			colors[ImGuiCol_TableBorderStrong] = ImVec4(0.197f, 0.197f, 0.197f, 1.00f);
			colors[ImGuiCol_TableBorderLight] = ImVec4(0.197f, 0.197f, 0.197f, 1.00f);
			colors[ImGuiCol_TableRowBg] = ImVec4(1.00f, 1.00f, 1.00f, 0.00f);
			colors[ImGuiCol_TableRowBgAlt] = ImVec4(1.00f, 1.00f, 1.00f, 0.10f);
			colors[ImGuiCol_TextSelectedBg] = ImVec4(0.188f, 0.529f, 0.780f, 1.000f);
			colors[ImGuiCol_DragDropTarget] = ImVec4(0.00f, 0.44f, 0.88f, 1.00f);
			colors[ImGuiCol_NavHighlight] = ImVec4(0.00f, 0.44f, 0.88f, 1.00f);
			colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
			colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
			colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.20f, 0.20f, 0.20f, 0.35f);
		}
		else
		{
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
		}
	}

	// Define a progress callback function
	void checkout_progress(const char* path, size_t completed_steps, size_t total_steps, [[maybe_unused]] void* payload)
	{
		printf("Checkout progress: %s - %zu/%zu\n", path, completed_steps, total_steps);
	}

	bool ImGuiInit(const char** args, int count)
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
		style.FramePadding = ImVec2(4.0f, 4.0f);
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
		style.PopupBorderSize = 2.0f;
		style.FrameBorderSize = 1.0f;
		style.TabBorderSize = 1.0f;
		style.WindowRounding = 6.0f;
		style.ChildRounding = 0.0f;
		style.FrameRounding = 4.0f;
		style.PopupRounding = 4.0f;
		style.GrabRounding = 2.0f;
		style.LogSliderDeadzone = 4.0f;
		style.TabRounding = 4.0f;
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

		SetTheme(true);
		SetFont();

		ImGui_ImplGlfw_InitForOpenGL(g_Window, true);
		ImGui_ImplOpenGL3_Init(glsl_version);

		Client::Init();

		memset(g_Path, 0, 2048);

		for (int i = 0; i < count; ++i)
			Client::InitRepo(args[i]);

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
			case GIT_DELTA_UNTRACKED	: return ImVec4(0.10f, 0.60f, 0.10f, 1.00f);
			case GIT_DELTA_TYPECHANGE	: return ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
			case GIT_DELTA_UNREADABLE	: return ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
			case GIT_DELTA_CONFLICTED	: return ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
			default						: return ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
		}
	}

	struct Commit
	{
		git_commit* CommitPtr = nullptr;
		char CommitID[41];
		UUID ID;

		eastl::string AuthorName;
		eastl::string AuthorEmail;
		eastl::string AuthorDateTime;
		eastl::string AuthorTimezoneOffset;

		eastl::string CommitterName;
		eastl::string CommitterEmail;
		eastl::string CommitterDateTime;
		eastl::string CommitterTimezoneOffset;

		eastl::string Message;
		eastl::string Description;

		eastl::vector<eastl::string> Refs;
	};

	void GetCommit(git_commit* commit, Commit* out)
	{
		out->CommitPtr = commit;

		const git_oid* oid = git_commit_id(commit);
		strncpy_s(out->CommitID, git_oid_tostr_s(oid), sizeof(out->CommitID) - 1);
		out->ID = Utils::GenUUID(commit);

		const git_signature* author = git_commit_author(commit);
		const git_signature* committer = git_commit_committer(commit);
		const char* commitSummary = git_commit_summary(commit);
		const char* commitDesc = git_commit_body(commit);

		out->AuthorName = author->name;
		out->AuthorEmail = author->email;
		char sign = committer->when.offset >= 0 ? '+' : '-';
		out->AuthorTimezoneOffset = std::format("{}{:02d}:{:02d}", sign, abs(author->when.offset) / 60, author->when.offset % 60).c_str();

		git_time_t timestamp = author->when.time;
		tm localTime;
		localtime_s(&localTime, &timestamp);
		char dateTime[COMMIT_DATE_LEN];
		strftime(dateTime, sizeof(dateTime), "%d %b %Y %H:%M:%S", &localTime);
		out->AuthorDateTime = dateTime;

		out->CommitterName = committer->name;
		out->CommitterEmail = committer->email;
		sign = committer->when.offset >= 0 ? '+' : '-';
		out->CommitterTimezoneOffset = std::format("{}{:02d}:{:02d}", sign, abs(committer->when.offset) / 60, committer->when.offset % 60).c_str();

		timestamp = committer->when.time;
		localtime_s(&localTime, &timestamp);
		strftime(dateTime, sizeof(dateTime), "%d %b %Y %H:%M:%S", &localTime);
		out->CommitterDateTime = dateTime;

		out->Message = commitSummary ? commitSummary : "";
		out->Description = commitDesc ? commitDesc : "";

		out->Refs.clear();
		git_reference_iterator* iter;
		git_reference* ref;
		git_reference_iterator_new(&iter, git_commit_owner(commit));
		while (!git_reference_next(&ref, iter))
		{
			const git_oid* refOid = git_reference_target(ref);
			if (refOid && git_oid_equal(refOid, oid))
			{
				const char* name = git_reference_name(ref) + (git_reference_is_remote(ref) == 1 ? sizeof(REMOTE_BRANCH_PREFIX) : sizeof(LOCAL_BRANCH_PREFIX)) - 1;
				out->Refs.emplace_back(name);
			}
		}
		git_reference_iterator_free(iter);
	}

	int BranchNameFilterTextCallback(ImGuiInputTextCallbackData* data)
	{
		switch (data->EventChar)
		{
			case ' ': data->EventChar = '-'; return 0;
			case '~': return 1;
			case '@': return 1;
			case '^': return 1;
			case '*': return 1;
			case '?': return 1;
			case ':': return 1;
			case '[': return 1;
			case '\\': return 1;
			case '<': return 1;
			case '>': return 1;
			default: return 0;
		}
	};

	void ShowRepoBranches(RepoData* repoData)
	{
		const float cursorPosX = ImGui::GetCursorPosX();
		BranchFilter.Draw("##BranchFilter", ImGui::GetContentRegionAvail().x);
		if (!BranchFilter.IsActive())
		{
			ImGui::SameLine();
			ImGui::SetCursorPosX(cursorPosX + ImGui::GetFontSize() * 0.75f);
			ImGui::BeginDisabled();
			ImGui::TextUnformatted(reinterpret_cast<const char*>(ICON_MDI_MAGNIFY " Filter..."));
			ImGui::EndDisabled();
		}

		constexpr ImGuiTreeNodeFlags treeFlags = ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_DefaultOpen;
		ImGui::BeginChild("Branches");
		if (ImGui::TreeNodeEx("Locals", treeFlags))
		{
			for (const auto& [branchRef, branchData] : repoData->Branches)
			{
				if (BranchFilter.IsActive() && !(BranchFilter.PassFilter(branchData.Name.c_str())))
					continue;

				if (branchData.Type == BranchType::Local)
				{
					const char8_t* icon = (repoData->HeadBranch == branchRef ? ICON_MDI_CHECK_ALL : ICON_MDI_SOURCE_BRANCH);
					bool open = ImGui::TreeNodeEx(branchData.Name.c_str(), treeFlags | ImGuiTreeNodeFlags_Leaf, "%s %s", icon, branchData.ShortName());
					if (open)
						ImGui::TreePop();
				}
			}

			ImGui::TreePop();
		}

		if (ImGui::TreeNodeEx("Remotes", treeFlags))
		{
			for (const auto& [branchRef, branchData] : repoData->Branches)
			{
				if (BranchFilter.IsActive() && !(BranchFilter.PassFilter(branchData.Name.c_str())))
					continue;

				if (branchData.Type == BranchType::Remote)
				{
					bool open = ImGui::TreeNodeEx(branchData.Name.c_str(), treeFlags | ImGuiTreeNodeFlags_Leaf, "%s %s", ICON_MDI_SOURCE_BRANCH, branchData.ShortName());
					if (open)
						ImGui::TreePop();
				}
			}

			ImGui::TreePop();
		}
		ImGui::EndChild();
	}

	void ShowRepoWindow(RepoData* repoData, bool* opened)
	{
		constexpr ImGuiTableColumnFlags columnFlags = ImGuiTableColumnFlags_NoHide | ImGuiTableColumnFlags_NoHeaderLabel;
		constexpr ImGuiTableFlags tooltipTableFlags = ImGuiTableFlags_SizingStretchProp;

		if (!repoData)
			return;

		enum class Action
		{
			None = 0,
			BranchCreate,
			BranchCheckout,
			BranchRename,
			BranchReset,
			BranchDelete,
			CommitCheckout,
		};

		Action action = Action::None;
		static git_reference* selectedBranch = nullptr;
		CommitData* selectedCommit = nullptr;

		ImGuiExt::Begin(repoData->Name.c_str(), opened);

		if (ImGui::IsWindowFocused())
			s_SelectedRepository = repoData;

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
				if (repoData->HeadBranch)
				{
					ImGui::TextUnformatted(repoData->Branches.at(repoData->HeadBranch).ShortName());
				}
				else
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

			ShowRepoBranches(repoData);

			ImGui::TableNextColumn();

			ImGui::Indent();
			if (repoData->HeadBranch)
				ImGui::Text("%s %s", ICON_MDI_SOURCE_BRANCH, repoData->Branches.at(repoData->HeadBranch).ShortName());
			else
				ImGui::TextUnformatted("Detached HEAD");

			ImGui::SameLine();
			if (ImGui::Button(reinterpret_cast<const char*>(ICON_MDI_REFRESH)))
			{
				Client::Fill(repoData, repoData->Repository);
			}

			const float cursorPosX = ImGui::GetCursorPosX();
			CommitsFilter.Draw("##CommitsFilter", ImGui::GetContentRegionAvail().x);
			if (!CommitsFilter.IsActive())
			{
				ImGui::SameLine();
				ImGui::SetCursorPosX(cursorPosX + ImGui::GetFontSize() * 0.75f);
				ImGui::BeginDisabled();
				ImGui::TextUnformatted(reinterpret_cast<const char*>(ICON_MDI_MAGNIFY " Search SHA, author or message..."));
				ImGui::EndDisabled();
			}

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

			ImVec2 cellPadding = ImGui::GetStyle().CellPadding;
			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, { cellPadding.x, cellPadding.y * 2.0f });
			ImGui::PushStyleColor(ImGuiCol_Header, { 0.000f, 0.439f, 0.878f, 0.824f });
			ImGui::PushStyleColor(ImGuiCol_HeaderActive, { 0.000f, 0.439f, 0.878f, 0.824f });
			if (ImGui::BeginTable(repoData->Name.c_str(), 4, tableFlags))
			{
				ImGui::TableSetupColumn("Message", columnFlags);
				ImGui::TableSetupColumn("AuthorName", columnFlags | ImGuiTableColumnFlags_WidthFixed);
				ImGui::TableSetupColumn("CommitID", columnFlags | ImGuiTableColumnFlags_WidthFixed);
				ImGui::TableSetupColumn("AuthorDate", columnFlags | ImGuiTableColumnFlags_WidthFixed);

				bool disabled = true;
				uint32_t start = maxRows * currentPage;
				uint32_t end = start + (currentPage == totalPages ? static_cast<uint32_t>(repoData->Commits.size()) % maxRows : maxRows);
				for (uint32_t i = start; i < end; ++i)
				{
					CommitData& data = repoData->Commits[i];

					if (CommitsFilter.IsActive() && !(CommitsFilter.PassFilter(data.CommitID) || CommitsFilter.PassFilter(data.Message) || CommitsFilter.PassFilter(data.AuthorName)))
						continue;

					ImGui::TableNextRow();
					ImGui::TableNextColumn();

					if (repoData->BranchHeads.find(data.ID) != repoData->BranchHeads.end())
					{
						eastl::vector<git_reference*>& branchHeads = repoData->BranchHeads.at(data.ID);
						for (git_reference* branch : branchHeads)
						{
							const bool isHeadBranch = branch == repoData->HeadBranch;
							BranchData& branchData = repoData->Branches.at(branch);
							const char* branchName = branchData.ShortName();

							if (isHeadBranch)
							{
								ImGui::PushFont(g_BoldFont);
								ImVec2 size = ImGui::CalcTextSize(branchName);
								size.x += lineHeightWithSpacing;
								ImGuiExt::FramedText(size, branchData.Color, true, 2.0f, "%s%s", ICON_MDI_CHECK_ALL, branchName);
								ImGui::PopFont();
								ImGui::SameLine(0, ImGui::GetTextLineHeight());
							}
							else
							{
								ImGuiExt::FramedTextUnformatted({ 0, 0 }, branchData.Color, true, 2.0f, branchName);
								ImGui::SameLine(0, lineHeightWithSpacing * 0.5f);
							}
						}
					}

					const bool isHead = data.ID == repoData->Head;
					if (repoData->SelectedCommit == 0 && isHead)
						repoData->SelectedCommit = data.ID;
					bool selected = repoData->SelectedCommit == data.ID;
					ImGui::PushID(&data.ID);
					if (ImGui::Selectable("##CommitSelectable", &selected, ImGuiSelectableFlags_SpanAllColumns))
					{
						s_SelectedRepository = repoData;
						repoData->SelectedCommit = data.ID;
					}

					if (!selected && ImGui::IsItemHovered() && ImGui::IsMouseReleased(1))
					{
						selected = true;
						s_SelectedRepository = repoData;
						repoData->SelectedCommit = data.ID;
					}

					if (!selected && ImGui::IsItemFocused())
					{
						selected = true;
						s_SelectedRepository = repoData;
						repoData->SelectedCommit = data.ID;
					}
					ImGui::PopID();

					if (disabled && isHead)
						disabled = false;

					if (isHead)	ImGui::PushFont(g_BoldFont);
					if (selected) ImGui::PushStyleColor(ImGuiCol_Text, { 0.9f, 0.9f, 0.9f, 1.0f });
					ImGui::BeginDisabled(disabled);
					
					ImGui::SameLine();
					float textSize = ImGui::CalcTextSize(data.Message).x;
					textSize -= data.MessageSize == COMMIT_MSG_LEN - 1 ? 5.0f : 0.0f;
					ImGuiExt::TextEllipsis(data.Message, { textSize, 0 });
					ImGui::TableNextColumn();
					ImGui::TextUnformatted(data.AuthorName);
					ImGui::TableNextColumn();
					ImGui::TextUnformatted(data.CommitID, data.CommitID + COMMIT_SHORT_ID_LEN);
					ImGui::TableNextColumn();
					ImGui::TextUnformatted(data.AuthorDate, data.AuthorDate + strlen(data.AuthorDate) - 3);

					ImGui::EndDisabled();
					if (selected) ImGui::PopStyleColor();
					if (isHead)	ImGui::PopFont();

					if (selected)
					{
						if (ImGui::IsWindowHovered() && ImGui::IsMouseReleased(1))
							ImGui::OpenPopup("CommitPopup", ImGuiPopupFlags_NoOpenOverExistingPopup);
						if (ImGui::BeginPopup("CommitPopup"))
						{
							if (repoData->BranchHeads.find(data.ID) != repoData->BranchHeads.end())
							{
								eastl::vector<git_reference*>& branchHeads = repoData->BranchHeads.at(data.ID);
								for (git_reference* branch : branchHeads)
								{
									BranchData& branchData = repoData->Branches.at(branch);
									if (ImGui::BeginMenu(branchData.ShortName()))
									{
										if (branchData.Type == BranchType::Local)
										{
											if (ImGui::MenuItem("Checkout"))
											{
												action = Action::BranchCheckout;
												if (!Client::BranchCheckout(branch))
													error = git_error_last()->message;
											}
											ImGui::Separator();
											if (ImGui::MenuItem("Rename"))
											{
												action = Action::BranchRename;
												selectedBranch = branch;
											}
											ImGui::BeginDisabled(repoData->HeadBranch == branch);
											if (ImGui::MenuItem("Delete"))
											{
												action = Action::BranchDelete;
												selectedBranch = branch;
											}
											ImGui::EndDisabled();
										}
										if (ImGui::MenuItem("Copy Branch Name"))
										{
											ImGui::SetClipboardText(branchData.ShortName());
										}

										ImGui::EndMenu();
									}
								}
								ImGui::Separator();
							}
							if (ImGui::MenuItem("New Branch"))
							{
								action = Action::BranchCreate;
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
										if (!Client::BranchReset(repoData, data.Commit, GIT_RESET_SOFT))
											error = git_error_last()->message;

										action = Action::BranchReset;
									}
									if (ImGui::MenuItem("Mixed (Soft + reset index to the commit)"))
									{
										if (!Client::BranchReset(repoData, data.Commit, GIT_RESET_MIXED))
											error = git_error_last()->message;

										action = Action::BranchReset;
									}
									if (ImGui::MenuItem("Hard (Mixed + changes in working tree discarded)"))
									{
										if (!Client::BranchReset(repoData, data.Commit, GIT_RESET_HARD))
											error = git_error_last()->message;

										action = Action::BranchReset;
									}

									ImGui::EndMenu();
								}
							}

							ImGui::Separator();
							if (ImGui::MenuItem("Checkout Commit"))
							{
								action = Action::CommitCheckout;
							}
							if (ImGui::MenuItem("Copy as Patch"))
							{
								eastl::string patch;
								if (Client::CreatePatch(data.Commit, patch))
									ImGui::SetClipboardText(patch.c_str());
								else
									error = git_error_last()->message;
							}

							ImGui::Separator();
							if (ImGui::MenuItem("Copy Commit SHA"))
							{
								ImGui::SetClipboardText(data.CommitID);
							}
							if (ImGui::MenuItem("Copy Commit Info"))
							{
								Commit c;
								GetCommit(data.Commit, &c);
								char shortSHA[8];
								strncpy_s(shortSHA, c.CommitID, COMMIT_SHORT_ID_LEN);
								eastl::string info = "SHA: ";
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
			ImGui::PopStyleColor(2);
			ImGui::PopStyleVar();

			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 16.0f, 16.0f });

			char invalidNameError[] = "Branch name is invalid!";
			auto it = repoData->CommitsIndexMap.find(repoData->SelectedCommit);
			if (it != repoData->CommitsIndexMap.end())
				selectedCommit = &(repoData->Commits[it->second]);
			if (selectedCommit)
			{
				if (action == Action::BranchCreate)
				{
					ImGui::OpenPopup("Create Branch");
				}
				if (action == Action::BranchRename)
				{
					ImGui::OpenPopup("Rename Branch");
				}
				if (action == Action::BranchCheckout)
				{
					Client::UpdateHead(*repoData);
				}
				if (action == Action::BranchDelete)
				{
					ImGui::OpenPopup("Delete Branch");
				}
				if (action == Action::CommitCheckout)
				{
					ImGui::OpenPopup("Checkout Commit");
				}

				float commitMessageTextSize = ImGui::CalcTextSize(selectedCommit->Message).x;
				if (selectedCommit->MessageSize == COMMIT_MSG_LEN - 1)
					commitMessageTextSize -= 20.0f;
				commitMessageTextSize = eastl::max(commitMessageTextSize, 700.0f);

				if (ImGuiExt::BeginPopupModal("Create Branch", { 270.0f + commitMessageTextSize, 0.0f }))
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
						ImGui::Text("%s %.*s", ICON_MDI_SOURCE_COMMIT, COMMIT_SHORT_ID_LEN, selectedCommit->CommitID);
						ImGui::SameLine();
						ImGuiExt::TextEllipsis(selectedCommit->Message);

						ImGui::TableNextRow();
						ImGui::TableNextColumn();

						ImGui::TextUnformatted("Branch name:");
						ImGui::TableNextColumn();
						ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
						static char branchName[256] = "";
						ImGui::InputTextWithHint("##NewBranchName", "Enter branch name", branchName, 256, ImGuiInputTextFlags_CallbackCharFilter, BranchNameFilterTextCallback);

						ImGui::TableNextRow();
						ImGui::TableNextColumn();
						ImGui::TableNextColumn();

						static bool checkoutAfterCreate = false;
						ImGui::Checkbox("Check out after create", &checkoutAfterCreate);

						ImGui::TableNextRow();
						ImGui::TableNextColumn();
						ImGui::TableNextColumn();

						ImGui::Spacing();
						ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize("Create Cancel").x - lineHeightWithSpacing);
						ImGui::BeginDisabled(branchName[0] == 0);
						if (ImGui::Button("Create"))
						{
							bool validName = false;
							git_reference* branch = Client::BranchCreate(repoData, branchName, selectedCommit->Commit, validName);
							bool success = branch;
							if (branch && checkoutAfterCreate)
								success = Client::BranchCheckout(branch);

							if (!validName)
								error = invalidNameError;
							else if (!success)
								error = git_error_last()->message;

							Client::UpdateHead(*repoData);

							memset(branchName, 0, 256);
							ImGui::CloseCurrentPopup();
						}
						ImGui::EndDisabled();

						ImGui::SameLine();

						if (ImGui::Button("Cancel"))
						{
							ImGui::CloseCurrentPopup();
						}

						ImGui::EndTable();
					}

					ImGuiExt::EndPopupModal();
				}

				if (ImGuiExt::BeginPopupModal("Rename Branch"))
				{
					ImGui::TextDisabled("Use '/' as a path separator to create folders");

					ImGui::Spacing();

					static char branchName[256] = "";
					ImGui::Text("%s %s %s", ICON_MDI_SOURCE_BRANCH, repoData->Branches.at(selectedBranch).ShortName(), ICON_MDI_ARROW_RIGHT);
					ImGui::SameLine();
					ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
					ImGui::InputTextWithHint("##NewBranchName", "Enter new branch name", branchName, 256, ImGuiInputTextFlags_CallbackCharFilter, BranchNameFilterTextCallback);

					ImGui::Spacing();
					ImGui::SetCursorPosX(ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize("Rename Cancel").x);
					ImGui::BeginDisabled(branchName[0] == 0);
					if (ImGui::Button("Rename"))
					{
						bool validName = false;
						if (!Client::BranchRename(repoData, selectedBranch, branchName, validName))
						{
							if (validName)
								error = git_error_last()->message;
							else
								error = invalidNameError;
						}

						selectedBranch = nullptr;
						memset(branchName, 0, 256);
						ImGui::CloseCurrentPopup();
					}
					ImGui::EndDisabled();

					ImGui::SameLine();

					if (ImGui::Button("Cancel"))
					{
						ImGui::CloseCurrentPopup();
					}

					ImGuiExt::EndPopupModal();
				}

				if (ImGuiExt::BeginPopupModal("Delete Branch"))
				{
					ImGui::TextUnformatted("Delete branch from your repository");
					ImGui::Spacing();

					ImGui::Text("Branch: %s %s", ICON_MDI_SOURCE_BRANCH, repoData->Branches.at(selectedBranch).ShortName());

					ImGui::Spacing();
					ImGui::SetCursorPosX(ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize("Delete Cancel").x);
					if (ImGui::Button("Delete"))
					{
						if (!Client::BranchDelete(repoData, selectedBranch))
							error = git_error_last()->message;

						selectedBranch = nullptr;
						ImGui::CloseCurrentPopup();
					}
					ImGui::SameLine();
					if (ImGui::Button("Cancel"))
					{
						ImGui::CloseCurrentPopup();
					}

					ImGuiExt::EndPopupModal();
				}

				if (ImGuiExt::BeginPopupModal("Checkout Commit", { 270.0f + commitMessageTextSize, 0.0f }))
				{
					ImGui::TextUnformatted("Checkout a particular revision. Repository will be in detached HEAD state.");
					ImGui::Spacing();

					ImGui::TextUnformatted("Commit to checkout:");
					ImGui::SameLine();
					ImGui::Text("%s %.*s", ICON_MDI_SOURCE_COMMIT, COMMIT_SHORT_ID_LEN, selectedCommit->CommitID);
					ImGui::SameLine();
					ImGuiExt::TextEllipsis(selectedCommit->Message);

					ImGui::Spacing();
					ImGui::SetCursorPosX(ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize("Checkout Commit Cancel").x);
					if (ImGui::Button("Checkout Commit"))
					{
						if (!Client::CommitCheckout(selectedCommit->Commit, false))
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
				if (error && gitErrorStr[0] == 0)
				{
					strncpy_s(gitErrorStr, error, strlen(error));
					ImGui::OpenPopup("Error");
				}

				if (gitErrorStr[0] != 0)
				{
					const ImVec2 center = ImGui::GetMainViewport()->GetCenter();
					ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
					if (ImGui::BeginPopupModal("Error"))
					{
						ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
						ImGui::InputText("##GitError", gitErrorStr, strlen(gitErrorStr), ImGuiInputTextFlags_ReadOnly);

						if (ImGui::Button("OK", ImVec2(120, 0)))
						{
							gitErrorStr[0] = 0;
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

		ImGuiExt::End();
	}

	void ImGuiRender()
	{
		constexpr ImGuiWindowFlags sideBarFlags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoNavFocus;
		
		const float frameHeight = ImGui::GetFrameHeight();
		const float frameHeightWithSpacing = ImGui::GetFrameHeightWithSpacing();
		const ImVec2 windowPadding = ImGui::GetStyle().WindowPadding;

		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 0.0f, 0.0f });
		if (ImGui::BeginViewportSideBar("##PrimaryMenuBar", ImGui::GetMainViewport(), ImGuiDir_Up, frameHeight, sideBarFlags))
		{
			if (ImGui::BeginMenuBar())
			{
				ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, windowPadding);

				if (ImGui::BeginMenu("File"))
				{
					if (ImGui::MenuItem("Open Repository", "Ctrl+O"))
					{
					}
					if (ImGui::MenuItem("Preferences", "Ctrl+,"))
					{
					}
					if (ImGui::MenuItem("Exit"))
					{
					}

					ImGui::EndMenu();
				}
				if (ImGui::BeginMenu("Edit"))
				{
					ImGui::EndMenu();
				}

				ImGui::PopStyleVar();
				ImGui::EndMenuBar();
			}
			ImGui::End();
		}
		if (ImGui::BeginViewportSideBar("##StatusBar", ImGui::GetMainViewport(), ImGuiDir_Down, frameHeight, sideBarFlags))
		{
			if (ImGui::BeginMenuBar())
			{
				constexpr float updateTime = 0.1f;
				static float accum = 0.0f;
				static float dt = 0.0f;
				static double mem = 0.0f;
				if (accum >= 0.0f)
				{
					dt = ImGui::GetIO().DeltaTime;
					mem = static_cast<double>(QuickGit::Allocation::GetSize()) / (1024.0 * 1024.0);
					accum = -updateTime;
				}

				accum += dt;
				ImGui::Text("FPS: %.2lf (%.3lfms)  MEM: %.2lfMB", 1.0f / dt, dt, mem);

				ImGui::EndMenuBar();
			}
			ImGui::End();
		}
		ImGui::PopStyleVar(2);

		ImGui::DockSpaceOverViewport(ImGui::GetMainViewport());

		ImGuiExt::Begin("Add Repo\t\t");
		ImGui::InputText("Path", g_Path, 2048);
		if (ImGui::Button("Open"))
		{
			Client::InitRepo(g_Path);
			memset(g_Path, 0, sizeof(g_Path));
		}
		ImGuiExt::End();


		eastl::vector<eastl::unique_ptr<RepoData>>& repos = Client::GetRepositories();
		for (eastl::vector<eastl::unique_ptr<RepoData>>::iterator it = repos.begin(); it != repos.end(); ++it)
		{
			RepoData* repoData = it->get();
			bool opened = repoData;
			ShowRepoWindow(repoData, &opened);
			if (!opened)
			{
				if (s_SelectedRepository == repoData)
					s_SelectedRepository = nullptr;

				repos.erase(it);
				break;
			}
		}

		CommitData* selectedCommit = nullptr;
		if (s_SelectedRepository && s_SelectedRepository->SelectedCommit)
		{
			if (s_SelectedRepository->CommitsIndexMap.find(s_SelectedRepository->SelectedCommit) != s_SelectedRepository->CommitsIndexMap.end())
			{
				size_t index = s_SelectedRepository->CommitsIndexMap.at(s_SelectedRepository->SelectedCommit);
				selectedCommit = &s_SelectedRepository->Commits[index];
			}
		}

		ImGuiExt::Begin("Commit\t\t");
		{
			ImGui::Indent();
			static Commit cd;
			static Diff diffs;

			if (selectedCommit && selectedCommit->Commit != cd.CommitPtr)
			{
				GetCommit(selectedCommit->Commit, &cd);
				diffs.Patches.clear();
				Client::GenerateDiff(selectedCommit->Commit, diffs);
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

				if (ImGui::BeginTable("CommitMidTable", 2, ImGuiTableFlags_SizingFixedFit))
				{
					ImGui::TableNextRow();
					ImGui::TableNextColumn();

					if (!cd.Refs.empty())
						ImGui::TextUnformatted("REFS");
					ImGui::TextUnformatted("SHA");
					ImGui::TextUnformatted("Internal ID");

					ImGui::TableNextColumn();

					for (size_t i = 0, sz = cd.Refs.size(); i < sz; ++i)
					{
						ImGuiExt::FramedTextUnformatted({ 0, 0 }, ImGui::GetColorU32(ImGuiCol_FrameBg), true, 2.0f, cd.Refs[i].c_str());
						if (i < sz - 1)
							ImGui::SameLine(0, ImGui::GetTextLineHeight());
					}
					ImGui::TextUnformatted(cd.CommitID);
					ImGui::Text("%llu", cd.ID);

					ImGui::EndTable();
				}

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
				for (auto& diff : diffs.Patches)
				{
					ImGui::PushStyleColor(ImGuiCol_Text, GetPatchStatusColor(diff.Status));
					bool open = ImGui::TreeNodeEx(diff.File.c_str(), ImGuiTreeNodeFlags_SpanAvailWidth);
					ImGui::PopStyleColor();
					if (open)
					{
						ImGui::Indent(frameHeightWithSpacing);
						if (diff.Patch == "@@BinaryData")
						{
							ImGui::TextUnformatted(diff.Patch.c_str());
							if (diff.NewFileSize)
							{
								ImGui::Indent(frameHeightWithSpacing);
								if (diff.OldFileSize)
								{
									ImGui::PushStyleColor(ImGuiCol_Text, GetPatchStatusColor(GIT_DELTA_DELETED));
									ImGui::Text("Old: %u Bytes", diff.OldFileSize);
									ImGui::PopStyleColor();
									ImGui::SameLine(0, frameHeightWithSpacing);
								}

								ImGui::PushStyleColor(ImGuiCol_Text, GetPatchStatusColor(GIT_DELTA_ADDED));
								ImGui::Text("New: %u Bytes", diff.NewFileSize);
								ImGui::PopStyleColor();
								ImGui::Unindent(frameHeightWithSpacing);
							}
						}
						else
						{
							ImVec2 size = ImGui::CalcTextSize(diff.Patch.c_str());
							size.y += ImGui::GetFrameHeightWithSpacing();
							ImGui::InputTextMultiline(diff.File.c_str(), &diff.Patch[0], diff.Patch.size(), { ImGui::GetContentRegionAvail().x, size.y }, ImGuiInputTextFlags_ReadOnly);
						}

						ImGui::Unindent(frameHeightWithSpacing);
						ImGui::TreePop();
					}
				}
			}
			ImGui::Unindent();
		}
		ImGuiExt::End();

		ImGuiExt::Begin("Local Changes\t\t");
		{
			ImGui::Indent();

			static git_commit* head = nullptr;
			static Diff unstaged;
			static Diff staged;
			static uint32_t contextLines = 3;
			static bool showFullContent = false;

			if (ImGui::Button(reinterpret_cast<const char*>(ICON_MDI_REFRESH)))
				head = nullptr;
			ImGui::SameLine();
			if (ImGui::Button(reinterpret_cast<const char*>(ICON_MDI_COGS)))
				ImGui::OpenPopup("Changes Prefs");

			if (ImGui::BeginPopup("Changes Prefs"))
			{
				if (ImGui::Checkbox("Full Content", &showFullContent))
					head = nullptr;
				ImGui::BeginDisabled(showFullContent);
				static const uint32_t step = 1;
				static const uint32_t fastStep = 3;
				if (ImGui::InputScalar("Context Lines", ImGuiDataType_U32, &contextLines, &step, &fastStep))
					head = nullptr;
				ImGui::EndDisabled();
				ImGui::EndPopup();
			}
			
			if (selectedCommit && selectedCommit->Commit != head)
			{
				head = selectedCommit->Commit;
				unstaged.Patches.clear();
				staged.Patches.clear();
				Client::GenerateDiffWithWorkDir(git_commit_owner(head), unstaged, staged, showFullContent ? INT_MAX : contextLines);
			}
			
			if (head)
			{
				for (int i = 0; i < 2; ++i)
				{
					bool stageArea = i != 0;
					auto& diffs = stageArea ? staged : unstaged;
					ImGui::TextUnformatted(i == 0 ? "Unstaged" : "Staged");
					for (auto& diff : diffs.Patches)
					{
						ImGui::PushStyleColor(ImGuiCol_Text, GetPatchStatusColor(diff.Status));
						bool open = ImGui::TreeNodeEx(diff.File.c_str(), ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_OpenOnArrow);
						if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0))
						{
							bool success = false;
							if (stageArea)
								success = Client::RemoveFromIndex(git_commit_owner(head), diff.File.c_str());
							else
								success = Client::AddToIndex(git_commit_owner(head), diff.File.c_str());

							if (!success)
							{
								if (const git_error* er = git_error_last())
									printf("%s\n", er->message);
							}

							if (success)
								head = nullptr;
						}
						ImGui::PopStyleColor();
						if (open)
						{
							ImGui::Indent(frameHeightWithSpacing);
							if (diff.Patch == "@@BinaryData")
							{
								ImGui::TextUnformatted(diff.Patch.c_str());
								if (diff.NewFileSize)
								{
									ImGui::Indent(frameHeightWithSpacing);
									if (diff.OldFileSize)
									{
										ImGui::PushStyleColor(ImGuiCol_Text, GetPatchStatusColor(GIT_DELTA_DELETED));
										ImGui::Text("Old: %u Bytes", diff.OldFileSize);
										ImGui::PopStyleColor();
										ImGui::SameLine(0, frameHeightWithSpacing);
									}

									ImGui::PushStyleColor(ImGuiCol_Text, GetPatchStatusColor(GIT_DELTA_ADDED));
									ImGui::Text("New: %u Bytes", diff.NewFileSize);
									ImGui::PopStyleColor();
									ImGui::Unindent(frameHeightWithSpacing);
								}
							}
							else
							{
								ImVec2 size = ImGui::CalcTextSize(diff.Patch.c_str());
								size.y += ImGui::GetFrameHeightWithSpacing();
								ImGui::InputTextMultiline(diff.File.c_str(), &diff.Patch[0], diff.Patch.size(), { ImGui::GetContentRegionAvail().x, size.y }, ImGuiInputTextFlags_ReadOnly);
							}

							ImGui::Unindent(frameHeightWithSpacing);
							ImGui::TreePop();
						}
					}
				}
				static char subject[512];
				static char desc[2048];

				ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
				if (ImGui::InputTextWithHint("##CommitSubject", "Commit subject", subject, 512, ImGuiInputTextFlags_EnterReturnsTrue))
					ImGui::SetKeyboardFocusHere();

				ImVec2 descriptionInputBoxSize = ImGui::GetContentRegionAvail();
				descriptionInputBoxSize.y -= frameHeightWithSpacing;
				descriptionInputBoxSize.y = eastl::max(descriptionInputBoxSize.y, frameHeightWithSpacing);
				ImGui::InputTextMultiline("##CommitDescription", desc, 2048, descriptionInputBoxSize);
				ImGui::BeginDisabled(staged.Patches.empty() || subject[0] == 0);
				eastl::string commitLabel = (staged.Patches.empty() ? "Commit" : std::format("Commit {} File(s)", staged.Patches.size()).c_str());
				if (ImGui::Button(commitLabel.c_str()))
				{
					bool success = Client::Commit(s_SelectedRepository, subject, desc);

					if (success)
					{
						memset(subject, 0, sizeof(subject));
						memset(desc, 0, sizeof(desc));
						head = nullptr;
					}
					if (!success)
					{
						if (const git_error* er = git_error_last())
							printf("%s\n", er->message);
					}
				}
				ImGui::EndDisabled();
			}
			
			ImGui::Unindent();
		}
		ImGuiExt::End();

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
