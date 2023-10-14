#include "pch.h"

#include "Log.h"
#include "ImGuiLayer.h"

int main(int argc, const char** argv)
{
	QuickGit::Log::Init();

	QuickGit::ImGuiInit(argv, argc);
	QuickGit::ImGuiRun();
	QuickGit::ImGuiShutdown();

	return 0;
}
