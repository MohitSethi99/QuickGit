include "./vendor/premake/premake_customization/solution_items.lua"

workspace "QuickGit"
	architecture "x64"
	startproject "QuickGit"

	configurations
	{
		"Debug",
		"Release",
		"Dist"
	}

	solution_items
	{
		".editorconfig"
	}

	flags
	{
		"MultiProcessorCompile"
	}

	filter { "action:vs2022" }
		linkoptions { "/ignore:4006" }
		buildoptions { "/bigobj" }
	filter { "action:vs2022", "toolset:clang" }
		buildoptions { "/showFilenames" }
	filter "system:linux"
		toolset "clang"

-- Library directories relavtive to root folder (solution directory)
LibDir = {}
LibDir["LibGit2"] = "%{wks.location}/QuickGit/vendor/libgit2/build/Release"

-- Bin directories relavtive to root folder (solution directory)
BinDir = {}

outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

-- Include directories relavtive to root folder (solution directory)
IncludeDir = {}
IncludeDir["GLFW"] = "%{wks.location}/QuickGit/vendor/GLFW/include"
IncludeDir["Glad"] = "%{wks.location}/QuickGit/vendor/Glad/include"
IncludeDir["ImGui"] = "%{wks.location}/QuickGit/vendor/imgui"
IncludeDir["stb_image"] = "%{wks.location}/QuickGit/vendor/stb_image"
IncludeDir["icons"] = "%{wks.location}/QuickGit/vendor/icons/include"
IncludeDir["magic_enum"] = "%{wks.location}/QuickGit/vendor/magic_enum"
IncludeDir["LibGit2"] = "%{wks.location}/QuickGit/vendor/libgit2/include"
IncludeDir["EABase"] = "%{wks.location}/QuickGit/vendor/EABase/include/Common"
IncludeDir["EASTL"] = "%{wks.location}/QuickGit/vendor/EASTL/include"

group "Dependencies"
	include "QuickGit/vendor/GLFW"
	include "QuickGit/vendor/Glad"
	include "QuickGit/vendor/imgui"
	include "QuickGit/vendor/EASTL"

group ""

include "QuickGit"
