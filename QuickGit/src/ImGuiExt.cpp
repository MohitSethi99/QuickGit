#include "pch.h"
#include "ImGuiExt.h"

#include <imgui_internal.h>

namespace QuickGit
{
	extern ImFont* g_HeadingFont;

	void ImGuiExt::HeadingTextUnformatted(const char* text, const char* textEnd /* = nullptr*/)
	{
		ImGui::PushFont(g_HeadingFont);
		ImGui::TextUnformatted(text, textEnd);
		ImGui::PopFont();
	}

	void ImGuiExt::HeadingText(const char* fmt, ...)
	{
		ImGui::PushFont(g_HeadingFont);
		va_list args;
		va_start(args, fmt);
		ImGui::TextV(fmt, args);
		va_end(args);
		ImGui::PopFont();
	}

	void ImGuiExt::TextEllipsis(const char* txt, const ImVec2& size /*= {0, 0}*/)
	{
		ImGuiWindow* window = ImGui::GetCurrentWindow();
		ImVec2 minRect = window->DC.CursorPos;
		ImVec2 sz = { size.x == 0 ? ImGui::GetContentRegionAvail().x : size.x, size.y == 0 ? ImGui::GetTextLineHeightWithSpacing() : size.y };
		ImVec2 maxRect = { minRect.x + sz.x, minRect.y + sz.y };
		ImGui::RenderTextEllipsis(window->DrawList, minRect, maxRect, maxRect.x, maxRect.x, txt, nullptr, nullptr);
		ImGui::Spacing();
	}

	bool ImGuiExt::BeginPopupModal(const char* name)
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
			ImGuiExt::HeadingTextUnformatted(name);

		return result;
	}
}
