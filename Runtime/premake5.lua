project "Runtime"
	kind "ConsoleApp"
	targetname "Beyond Runtime"

	targetdir ("../bin/" .. outputdir .. "/%{prj.name}")
	objdir ("../bin-int/" .. outputdir .. "/%{prj.name}")

	links  { "Beyond" }

	defines  { "GLM_FORCE_DEPTH_ZERO_TO_ONE", }

	files {
		"src/**.h", 
		"src/**.c", 
		"src/**.hpp", 
		"src/**.cpp" 
	}

	includedirs {
		"src/",
		"../Beyond/src",
		"../Beyond/vendor",
	}

	filter "system:windows"
		systemversion "latest"
		defines { "BEY_PLATFORM_WINDOWS", "RTXGI_COORDINATE_SYSTEM=2", "RTXGI_DDGI_RESOURCE_MANAGEMENT=1", "RTXGI_GFX_NAME_OBJECTS=1", }
		postbuildcommands {
			'{COPY} "../Shared/vendor/Compressonator_4.4.19/bin/CLI" "Tools/CompressonatorCLI"',
			'{COPY} "../Beyond/vendor/assimp/bin/windows/assimp-vc143-mt.dll" "%{cfg.targetdir}"',

		}

	filter { "system:windows", "configurations:Debug" }
		postbuildcommands {
			'{COPY} "../Beyond/vendor/mono/bin/Debug/mono-2.0-sgen.dll" "%{cfg.targetdir}"',
			'{COPY} "../Beyond/vendor/NvidiaAftermath/lib/x64/windows/GFSDK_Aftermath_Lib.x64.dll" "%{cfg.targetdir}"'
		}

	filter { "system:windows", "configurations:Debug-AS" }
		postbuildcommands {
			'{COPY} "../Beyond/vendor/mono/bin/Debug/mono-2.0-sgen.dll" "%{cfg.targetdir}"',
			'{COPY} "../Beyond/vendor/NvidiaAftermath/lib/x64/windows/GFSDK_Aftermath_Lib.x64.dll" "%{cfg.targetdir}"'
		}

	filter { "system:windows", "configurations:Release" }
		postbuildcommands {
			'{COPY} "../Beyond/vendor/mono/bin/Release/mono-2.0-sgen.dll" "%{cfg.targetdir}"',
			'{COPY} "../Beyond/vendor/NvidiaAftermath/lib/x64/windows/GFSDK_Aftermath_Lib.x64.dll" "%{cfg.targetdir}"'
		}

	filter { "system:windows", "configurations:Dist" }
		postbuildcommands {
			'{COPY} "../Beyond/vendor/mono/bin/Release/mono-2.0-sgen.dll" "%{cfg.targetdir}"',
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
		symbols "On"
		defines { "BEY_DEBUG", }

		ProcessDependencies("Debug")

	filter { "system:windows", "configurations:Debug-AS" }
		sanitize { "Address" }
		flags { "NoRuntimeChecks", "NoIncrementalLink" }

	filter "configurations:Release"
		optimize "On"
        vectorextensions "AVX2"
        isaextensions { "BMI", "POPCNT", "LZCNT", "F16C" }
		defines { "BEY_RELEASE", "BEY_TRACK_MEMORY", }

		ProcessDependencies("Release")

	filter "configurations:Debug or configurations:Release"
		defines {
			"BEY_TRACK_MEMORY",
			
            "JPH_DEBUG_RENDERER",
            "JPH_FLOATING_POINT_EXCEPTIONS_ENABLED",
            "JPH_EXTERNAL_PROFILE"
		}

	filter "configurations:Dist"
		kind "WindowedApp"
		optimize "On"
		symbols "Off"
        vectorextensions "AVX2"
        isaextensions { "BMI", "POPCNT", "LZCNT", "F16C" }
		defines { "BEY_DIST" }

		ProcessDependencies("Dist")

        