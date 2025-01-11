project "Beyond"
	kind "StaticLib"
	editandcontinue "Off"

	targetdir ("../bin/" .. outputdir .. "/%{prj.name}")
	objdir ("../bin-int/" .. outputdir .. "/%{prj.name}")

	pchheader "pch.h"
	pchsource "src/pch.cpp"
		
	files {
		"src/**.h",
		"src/**.c",
		"src/**.hpp",
		"src/**.cpp",

		"Material.natvis",

		"Platform/" .. firstToUpper(os.target()) .. "/**.hpp",
		"Platform/" .. firstToUpper(os.target()) .. "/**.cpp",

		"vendor/FastNoise/**.cpp",
		"vendor/simdutf/**.cpp",

		"vendor/rtxgi-sdk/include/**.h",
		"vendor/rtxgi-sdk/src/**.cpp",

		"vendor/yaml-cpp/src/**.cpp",
		"vendor/yaml-cpp/src/**.h",
		"vendor/yaml-cpp/include/**.h",
		"vendor/VulkanMemoryAllocator/**.h",
		"vendor/VulkanMemoryAllocator/**.cpp",

		"vendor/gli/gli/**.hpp",
		"vendor/gli/gli/**.inl",
		
		"vendor/nvvk/resourceallocator_vk.hpp",
		"vendor/nvvk/resourceallocator_vk.cpp",
		"vendor/nvvk/sbtwrapper_vk.hpp",
		"vendor/nvvk/sbtwrapper_vk.cpp",
		
		"vendor/tinygltf/tiny_gltf.h",
		-- "vendor/tinygltf/tiny_gltf.cc",

		"vendor/imgui/misc/cpp/imgui_stdlib.cpp",
		"vendor/imgui/misc/cpp/imgui_stdlib.h"
	}

	includedirs { "src/", "vendor/", }

	IncludeDependencies()
	
	defines { "GLM_FORCE_DEPTH_ZERO_TO_ONE", "VK_NO_PROTOTYPES", "RTXGI_COORDINATE_SYSTEM=2", "RTXGI_DDGI_RESOURCE_MANAGEMENT=1", "RTXGI_GFX_NAME_OBJECTS=1" }

	excludes { 
		"vendor/rtxgi-sdk/include/VulkanExtensions.h",
		"vendor/rtxgi-sdk/src/VulkanExtensions.cpp",
	}

	filter "files:vendor/FastNoise/**.cpp or files:vendor/simdutf/**.cpp or files:vendor/yaml-cpp/src/**.cpp or files:vendor/imgui/misc/cpp/imgui_stdlib.cpp or files:src/Beyond/Tiering/TieringSerializer.cpp or files:src/Beyond/Core/ApplicationSettings.cpp or files:vendor/rtxgi-sdk/src/**.cpp"
		flags { "NoPCH" }


	filter "system:windows"
		systemversion "latest"
		defines { "BEY_PLATFORM_WINDOWS", }

	filter "system:linux"
		defines { "BEY_PLATFORM_LINUX", "__EMULATE_UUID" }

	filter "configurations:Debug or configurations:Debug-AS"
		symbols "On"
		defines { "BEY_DEBUG", "_DEBUG", "ACL_ON_ASSERT_ABORT", }

	filter { "system:windows", "configurations:Debug-AS" }	
		sanitize { "Address" }
		flags { "NoRuntimeChecks", "NoIncrementalLink" }

	filter "configurations:Release"
		optimize "On"
		vectorextensions "AVX2"
		isaextensions { "BMI", "POPCNT", "LZCNT", "F16C" }
		defines { "BEY_RELEASE", "NDEBUG", }

	filter { "configurations:Debug or configurations:Debug-AS or configurations:Release" }
		defines {
			"BEY_TRACK_MEMORY",
			"JPH_DEBUG_RENDERER",
			"JPH_FLOATING_POINT_EXCEPTIONS_ENABLED",
			"JPH_EXTERNAL_PROFILE"
		}

	filter "configurations:Dist"
		optimize "On"
		symbols "on"
		vectorextensions "AVX2"
		isaextensions { "BMI", "POPCNT", "LZCNT", "F16C" }
		defines { "BEY_DIST" }

		removefiles {
			"src/Beyond/Platform/Vulkan/ShaderCompiler/**.cpp",
			"src/Beyond/Platform/Vulkan/Debug/**.cpp",

			"src/Beyond/Asset/AssimpAnimationImporter.cpp",
			"src/Beyond/Asset/AssimpMeshImporter.cpp",
		}
	