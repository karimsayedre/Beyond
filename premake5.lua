include "./vendor/premake_customization/solution_items.lua"
include "Dependencies.lua"
include "./Editor/Resources/LUA/Beyond.lua"

workspace "Beyond"
	configurations { "Debug", "Debug-AS", "Release", "Dist" }
	targetdir "build"
	startproject "Editor"
    conformancemode "On"
	
	language "C++"
	cppdialect "C++latest"
	staticruntime "Off"

	solution_items { ".editorconfig" }

	externalwarnings "Off"

	flags { "MultiProcessorCompile" }

	-- NOTE: Don't remove this. Please never use Annex K functions ("secure", e.g _s) functions.
	defines {
		"_CRT_SECURE_NO_WARNINGS",
		"_SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING",
		"TRACY_ENABLE",
		"TRACY_ON_DEMAND",
		"TRACY_CALLSTACK=10",
		"EASTL_EASTDC_VSNPRINTF=0",
		"GLM_FORCE_XYZW_ONLY"
	}

    filter "action:vs*"
        linkoptions { "/ignore:4099" } -- NOTE: Disable no PDB found warning
        disablewarnings { "4068" } -- Disable "Unknown #pragma mark warning"

	filter "language:C++ or language:C"
		architecture "x86_64"

	filter "configurations:Debug or configurations:Debug-AS"
		optimize "Off"
		symbols "On"
		defines {
				"EASTL_DEV_DEBUG=1",
			}

	filter { "system:windows", "configurations:Debug-AS" }	
		sanitize { "Address" }
		editandcontinue "Off"
		flags { "NoRuntimeChecks", "NoIncrementalLink" }

	filter "configurations:Release"
		optimize "On"
		symbols "Default"

	filter "configurations:Dist"
		optimize "Full"
		symbols "Off"

	filter "system:windows"
		buildoptions { "/EHsc", "/Zc:preprocessor", "/Zc:__cplusplus" }

outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

group "Dependencies"
include "Beyond/vendor/GLFW"
include "Beyond/vendor/imgui"
include "Beyond/vendor/EASTL-3.21.23"
include "Beyond/vendor/spdlog"
include "Beyond/vendor/tracy"
include "Beyond/vendor/JoltPhysics/JoltPhysicsPremake.lua"
include "Beyond/vendor/JoltPhysics/JoltViewerPremake.lua"
include "Beyond/vendor/NFD-Extended"
group "Dependencies/msdf"
include "Beyond/vendor/msdf-atlas-gen"
group ""

group "Core"
include "Beyond"
include "ScriptCore"
group ""

group "Tools"
include "Editor"
group ""

group "Runtime"
include "Runtime"
group ""
