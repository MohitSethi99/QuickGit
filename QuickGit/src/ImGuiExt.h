#pragma once

#include <imgui.h>

namespace QuickGit
{
	class ImGuiExt
	{
	public:
		static void HeadingText(const char* fmt, ...);
		static void HeadingTextUnformatted(const char* text, const char* textEnd = nullptr);
		static void TextEllipsis(const char* txt, const ImVec2& size = { 0, 0 });
		static bool BeginPopupModal(const char* name);
		inline static void EndPopupModal() { ImGui::EndPopup(); }
	};
}
