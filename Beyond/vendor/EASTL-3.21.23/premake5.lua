project "EASTL"
	kind "StaticLib"
	language "C++"
    staticruntime "off"

	targetdir ("bin/" .. outputdir .. "/%{prj.name}")
	objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

	includedirs {
		"include/",
		"EABase/include/Common",  -- Created using cmake
		"../../src/" -- Beyond/src
		-- "../EABase-2.09.05/include/Common"
	}

	defines {
		"WIN32",
		"_WINDOWS",
		"_CHAR16T",
		"_CRT_SECURE_NO_WARNINGS",
		"_SCL_SECURE_NO_WARNINGS",
		"EASTL_OPENSOURCE=1",
		"EASTL_EASTDC_VSNPRINTF=0"
	}

	files
	{
		"include/**.h",
		"source/**.cpp",

		"doc/EASTL.natvis"
	}

	filter "system:windows"
		systemversion "latest"
		cppdialect "C++20"

	filter "system:linux"
		pic "On"
		systemversion "latest"
		cppdialect "C++20"

	filter "configurations:Debug"
		runtime "Debug"
		symbols "on"
		defines { "EASTL_DEV_DEBUG=1"}

	filter "configurations:Release"
		runtime "Release"
		optimize "on"

    filter "configurations:Dist"
		runtime "Release"
		optimize "on"
        symbols "off"
