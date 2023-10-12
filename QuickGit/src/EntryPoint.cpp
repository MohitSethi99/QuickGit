#include "pch.h"

#include "ImGuiLayer.h"

int main(int argc, const char** argv)
{
	QuickGit::ImGuiInit(argv, argc);
	QuickGit::ImGuiRun();
	QuickGit::ImGuiShutdown();

	return 0;
}
