project "Editor"
	kind "ConsoleApp"
	editandcontinue "Off"

	targetdir ("../bin/" .. outputdir .. "/%{prj.name}")
	objdir ("../bin-int/" .. outputdir .. "/%{prj.name}")

	links { "Beyond" }

	defines { "GLM_FORCE_DEPTH_ZERO_TO_ONE", }

	files  { 
		"src/**.h",
		"src/**.c",
		"src/**.hpp",
		"src/**.cpp",
		
		-- Shaders
		"Resources/Shaders/**.glsl",
		"Resources/Shaders/**.glslh",
		"Resources/Shaders/**.hlsl",
		"Resources/Shaders/**.hlslh",
		"Resources/Shaders/**.slh",
	}

	includedirs  {
		"src/",

		"../Beyond/src/",
		"../Beyond/vendor/"
	}

	filter "system:windows"
		systemversion "latest"

		defines { "BEY_PLATFORM_WINDOWS", "RTXGI_COORDINATE_SYSTEM=2", "RTXGI_DDGI_RESOURCE_MANAGEMENT=1", "RTXGI_GFX_NAME_OBJECTS=1",  }

		postbuildcommands {
			'{COPY} "../Beyond/vendor/NvidiaAftermath/lib/x64/windows/GFSDK_Aftermath_Lib.x64.dll" "%{cfg.targetdir}"',
			'{COPY} "../Beyond/vendor/assimp/bin/windows/assimp-vc143-mt.dll" "%{cfg.targetdir}"',
		}

	filter { "system:windows", "configurations:Debug or configurations:Debug-AS" }
		postbuildcommands {
			'{COPY} "../Beyond/vendor/mono/bin/Debug/mono-2.0-sgen.dll" "%{cfg.targetdir}"',
			'{COPY} "../Beyond/vendor/nvngx_dlss_sdk/lib/Windows_x86_64/dev/nvngx_dlss.dll" "%{cfg.targetdir}"',
		}
		linkoptions { "/FUNCTIONPADMIN" }


	filter { "system:windows", "configurations:Release or configurations:Dist" }
		postbuildcommands {
			'{COPY} "../Beyond/vendor/mono/bin/Release/mono-2.0-sgen.dll" "%{cfg.targetdir}"',
			'{COPY} "../Beyond/vendor/nvngx_dlss_sdk/lib/Windows_x86_64/rel/nvngx_dlss.dll" "%{cfg.targetdir}"',
		}

	filter "system:linux"
		defines { "BEY_PLATFORM_LINUX", "__EMULATE_UUID" }

		links {
			"pthread"
		}

		result, err = os.outputof("pkg-config --libs gtk+-3.0")
		linkoptions {
			result,
			"-pthread",
			"-ldl"
		}

	filter "configurations:Debug or configurations:Debug-AS"
		symbols "Full"
		defines { "BEY_DEBUG" }

		ProcessDependencies("Debug")

	filter { "system:windows", "configurations:Debug-AS" }
		sanitize { "Address" }
		flags { "NoRuntimeChecks", "NoIncrementalLink" }

	filter "configurations:Release"
		optimize "On"
		vectorextensions "AVX2"
		isaextensions { "BMI", "POPCNT", "LZCNT", "F16C" }
		defines { "BEY_RELEASE", }

		ProcessDependencies("Release")

	filter "configurations:Debug or configurations:Debug-AS or configurations:Release"
		defines {
			"BEY_TRACK_MEMORY",
			"TRACY_VK_USE_SYMBOL_TABLE",
			
			"JPH_DEBUG_RENDERER",
			"JPH_FLOATING_POINT_EXCEPTIONS_ENABLED",
			"JPH_EXTERNAL_PROFILE"
		}

	filter "configurations:Dist"
		kind "WindowedApp"
		optimize "On"
		symbols "On"
		vectorextensions "AVX2"
		isaextensions { "BMI", "POPCNT", "LZCNT", "F16C" }
		defines { "BEY_DIST" }

		ProcessDependencies("Dist")

	filter "files:**.hlsl"
		flags {"ExcludeFromBuild"}
