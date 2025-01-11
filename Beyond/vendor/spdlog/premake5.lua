project "spdlog"
	kind "StaticLib"
	language "C++"
    staticruntime "off"
	cppdialect "C++20"
	systemversion "latest"

	targetdir ("bin/" .. outputdir .. "/%{prj.name}")
	objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

	includedirs {
		"include/",
	}

	defines {
		"WIN32",
		"_WINDOWS",
		"SPDLOG_COMPILED_LIB",
	}

	files
	{
		"include/**.h",
		"src/**.cpp",
	}

	filter "system:linux"
		pic "On"

	filter "configurations:Debug"
		runtime "Debug"
		symbols "on"

	filter "configurations:Release"
		runtime "Release"
		optimize "on"

    filter "configurations:Dist"
		runtime "Release"
		optimize "on"
        symbols "off"
