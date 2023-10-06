#pragma once

#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui.h>

namespace QuickGit
{
	class ImGuiExt
	{
	public:
		static void FramedText(const ImVec2& frameSize, uint32_t frameColor, bool frameBorder, float frameRounding, const char* fmt, ...);
		static void FramedTextUnformatted(const ImVec2& frameSize, uint32_t frameColor, bool frameBorder, float frameRounding, const char* text, const char* textEnd = nullptr);
		static void HeadingText(const char* fmt, ...);
		static void HeadingTextUnformatted(const char* text, const char* textEnd = nullptr);
		static void TextEllipsis(const char* txt, const ImVec2& size = { 0, 0 });
		static bool BeginPopupModal(const char* name);
		inline static void EndPopupModal() { ImGui::EndPopup(); }
	};
}
