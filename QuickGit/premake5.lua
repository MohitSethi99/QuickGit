project "QuickGit"
	kind "ConsoleApp"
	language "C++"
	cppdialect "C++20"
	staticruntime "off"
	warnings "extra"
	externalwarnings "off"
	rtti "off"
	postbuildmessage ""

	flags { "FatalWarnings" }

	targetdir ("%{wks.location}/bin/" .. outputdir .. "/%{prj.name}")
	objdir ("%{wks.location}/bin-int/" .. outputdir .. "/%{prj.name}")

	pchheader "pch.h"
	pchsource "src/pch.cpp"

	files
	{
		"src/**.h",
		"src/**.cpp",
		"vendor/stb_image/**.h",
		"vendor/stb_image/**.cpp",

		"vendor/**.natvis",
		"vendor/**.natstepfilter",
	}

	defines
	{
		"GLFW_INCLUDE_NONE",
		"SPDLOG_USE_STD_FORMAT",
		"SPDLOG_WCHAR_TO_UTF8_SUPPORT"
	}

	includedirs
	{
		"src"
	}

	externalincludedirs
	{
		"vendor/spdlog/include",
		"%{IncludeDir.GLFW}",
		"%{IncludeDir.Glad}",
		"%{IncludeDir.ImGui}",
		"%{IncludeDir.stb_image}",
		"%{IncludeDir.icons}",
		"%{IncludeDir.magic_enum}",
		"%{IncludeDir.LibGit2}",
	}

	links
	{
		"GLFW",
		"Glad",
		"ImGui",
	}

	postbuildcommands
	{
		-- LibGit2
		'{ECHO} ====== Copying LibGit2 ======',
		'{COPYFILE} %{LibDir.LibGit2}/git2.dll "%{cfg.targetdir}"',
	}

	filter "system:windows"
		systemversion "latest"
		links
		{
			"opengl.dll",
			"%{LibDir.LibGit2}/git2.lib",
		}

	filter "system:linux"
		pic "On"
		systemversion "latest"
		buildoptions { "`pkg-config --cflags gtk+-3.0`" }
		linkoptions { "`pkg-config --libs gtk+-3.0`" }
		links
		{
			"GL:shared",
			"dl:shared",
			"%{LibDir.LibGit2}/git2.lib",
		}

	filter "configurations:Debug"
		defines "QG_DEBUG"
		runtime "Debug"
		symbols "on"

	filter "configurations:Release"
		defines "QG_RELEASE"
		runtime "Release"
		optimize "speed"

	filter "configurations:Dist"
		defines "QG_DIST"
		runtime "Release"
		optimize "speed"
		symbols "off"
