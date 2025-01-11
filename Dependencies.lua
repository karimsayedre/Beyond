include "./vendor/premake_customization/ordered_pairs.lua"

-- Utility function for converting the first character to uppercase
function firstToUpper(str)
	return (str:gsub("^%l", string.upper))
end

-- Grab Vulkan SDK path
VULKAN_SDK = os.getenv("VULKAN_SDK")

--[[
	If you're adding a new dependency all you have to do to get it linking
	and included is define it properly in the table below, here's some example usage:

	MyDepName = {
		LibName = "my_dep_name",
		LibDir = "some_path_to_dependency_lib_dir",
		IncludeDir = "my_include_dir",
		Windows = { DebugLibName = "my_dep_name_debug" },
		Configurations = "Debug,Release"
	}

	MyDepName - This is just for organizational purposes, it doesn't actually matter for the build process
	LibName - This is the name of the .lib file that you want e.g Editor to link against (you shouldn't include the .lib extension since Linux uses .a)
	LibDir - Indicates which directory the lib file is located in, this can include "%{cfg.buildcfg}" if you have a dedicated Debug / Release directory
	IncludeDir - Pretty self explanatory, the filepath that will be included in externalincludedirs
	Windows - This defines a platform-specific scope for this dependency, anything defined in that scope will only apply for Windows, you can also add one for Linux
	DebugLibName - Use this if the .lib file has a different name in Debug builds (e.g "shaderc_sharedd" vs "shaderc_shared")
	Configurations - Defines a list of configurations that this dependency should be used in, if no Configurations is specified all configs will link and include this dependency (which is what we want in most cases)

	Most of the properties I've listed can be used in either the "global" dependency scope OR in a specific platform scope,
	the only property that doesn't support that is "Configurations" which HAS to be defined in the global scope.

	Of course you can put only SOME properties in a platform scope, and others in the global scope if you want to.

	Naturally I suggest taking a look at existing dependency definitions when adding a new dependency.

	Remember that in most cases you only need to update this list, no need to manually add dependencies to the "links" list or "includedirs" list

	HEADER-ONLY LIBRARIES: If your dependency is header-only you shouldn't specify e.g LibName, just add IncludeDir and it'll be treated like a header-only library

]]--

Dependencies = {
	Vulkan = {
		Windows = {
			LibName = "vulkan-1",
			IncludeDir = "%{VULKAN_SDK}/Include/",
			LibDir = "%{VULKAN_SDK}/Lib/",
		},
		Linux =  {
			LibName = "vulkan",
			IncludeDir = "%{VULKAN_SDK}/include/",
			LibDir = "%{VULKAN_SDK}/lib/",
		},
	},
	CompressonatorFramework = {
		IncludeDir = "%{wks.location}/Beyond/vendor/Compressonator_4.4.19/include",
		Windows = { LibName = "CMP_Framework_MD", DebugLibName = "CMP_Framework_MDd", LibDir = "%{wks.location}/Beyond/vendor/Compressonator_4.4.19/lib/VS2019/x64"}
	},
	TinyDDS = {
		IncludeDir = "%{wks.location}/Beyond/vendor/tiny_dds/include"
	},
	rtxgi_sdk = {
		IncludeDir = "%{wks.location}/Beyond/vendor/rtxgi-sdk/include",
	},
	DirectXCompiler = {
		LibName = "dxcompiler",
	},
	TBB = {
		Linux = { LibName = "tbb" },
	},
	NvidiaAftermath = {
		LibName = "GFSDK_Aftermath_Lib.x64",
		IncludeDir = "%{wks.location}/Beyond/vendor/NvidiaAftermath/include",
		Windows = { LibDir = "%{wks.location}/Beyond/vendor/NvidiaAftermath/lib/x64/windows" },
		Linux = { LibDir = "%{wks.location}/Beyond/vendor/NvidiaAftermath/lib/x64/linux/" },
	},
	Assimp = {
		IncludeDir = "%{wks.location}/Beyond/vendor/assimp/include",
		Windows = { LibName = "assimp-vc143-mt", LibDir = "%{wks.location}/Beyond/vendor/assimp/bin/windows/" },
		Linux = { LibName = "assimp", LibDir = "%{wks.location}/Beyond/vendor/assimp/bin/linux/" },
		Configurations = "Debug,Release"
	},
	Mono = {
		IncludeDir = "%{wks.location}/Beyond/vendor/mono/include",
		Windows = { LibName = "mono-2.0-sgen", LibDir = "%{wks.location}/Beyond/vendor/mono/lib/windows/%{cfg.buildcfg}/" },
		Linux = { LibName = "monosgen-2.0", LibDir = "%{wks.location}/Beyond/vendor/mono/lib/linux/" },
	},
	ShaderC = {
		Windows = { LibName = "shaderc_combined", DebugLibName = "shaderc_combinedd", },
		IncludeDir = "%{wks.location}/Beyond/vendor/shaderc/include",
		Configurations = "Debug,Release"
	},
	ShaderCUtil = {
		LibName = "shaderc_util",
		Windows = { DebugLibName = "shaderc_utild", },
		IncludeDir = "%{wks.location}/Beyond/vendor/shaderc/libshaderc_util/include",
		Configurations = "Debug,Release"
	},
	SPIRVCrossCore = {
		IncludeDir = "%{wks.location}/Beyond/vendor/SPIRV-Cross/include",
		Windows = { LibName = "spirv-cross-core", DebugLibName = "spirv-cross-cored", 
		--LibDir = "%{wks.location}/Beyond/vendor/SPIRV-Cross/Lib/%{cfg.buildcfg}/" -- This is a workaround for SER because it's not supported.
		},
		Configurations = "Debug,Release"
	},
	GLFW = {
		-- No need to specify LibDir for GLFW since it's automatically handled by premake
		LibName = "GLFW",
		IncludeDir = "%{wks.location}/Beyond/vendor/GLFW/include",
	},
	GLM = {
		IncludeDir = "%{wks.location}/Beyond/vendor/glm",
	},
	EnTT = {
		IncludeDir = "%{wks.location}/Beyond/vendor/entt/include",
	},
	ImGui = {
		LibName = "ImGui",
		IncludeDir = "%{wks.location}/Beyond/vendor/imgui",
	},
	ImGuiNodeEditor = {
		IncludeDir = "%{wks.location}/Beyond/vendor/imgui-node-editor",
	},
	NFDExtended = {
		LibName = "NFD-Extended",
		IncludeDir = "%{wks.location}/Beyond/vendor/NFD-Extended/NFD-Extended/src/include"
	},
	FastNoise = {
		IncludeDir = "%{wks.location}/Beyond/vendor/FastNoise",
	},
	SIMDUTF = {
		IncludeDir = "%{wks.location}/Beyond/vendor/simdutf",
	},
	EABase = {
		IncludeDir = "%{wks.location}/Beyond/vendor/EASTL-3.21.23/EABase/include/Common", 
	},
	EASTL = {
		LibName = "EASTL",
		IncludeDir = "%{wks.location}/Beyond/vendor/EASTL-3.21.23/include",
	},
	DLSS = {
		Windows = { LibName = "nvsdk_ngx_d", DebugLibName = "nvsdk_ngx_d_dbg", LibDir = "%{wks.location}/Beyond/vendor/nvngx_dlss_sdk/lib/Windows_x86_64/x86_64" },
		IncludeDir = "%{wks.location}/Beyond/vendor/nvngx_dlss_sdk/include",
	},
	SPDLOG = {
		LibName = "spdlog",
		IncludeDir = "%{wks.location}/Beyond/vendor/spdlog/include",
	},
	MiniAudio = {
		IncludeDir = "%{wks.location}/Beyond/vendor/miniaudio/include",
	},
	Farbot = {
		IncludeDir = "%{wks.location}/Beyond/vendor/farbot/include",
	},
	Choc = {
		IncludeDir = "%{wks.location}/Beyond/vendor/choc",
	},
	MagicEnum = {
		IncludeDir = "%{wks.location}/Beyond/vendor/magic_enum/include",
	},
	Tracy = {
		LibName = "Tracy",
		IncludeDir = "%{wks.location}/Beyond/vendor/tracy/tracy/public",
	},
	JoltPhysics = {
		LibName = "JoltPhysics",
		IncludeDir = "%{wks.location}/Beyond/vendor/JoltPhysics/JoltPhysics",
	},
	MSDFAtlasGen = {
		LibName = "msdf-atlas-gen",
		IncludeDir = "%{wks.location}/Beyond/vendor/msdf-atlas-gen/msdf-atlas-gen",
	},
	MSDFGen = {
		LibName = "msdfgen",
		IncludeDir = "%{wks.location}/Beyond/vendor/msdf-atlas-gen/msdfgen",
	},
	Freetype = {
		LibName = "freetype"
	},
	STB = {
		IncludeDir = "%{wks.location}/Beyond/vendor/stb/include",
	},
	YAML_CPP = {
		IncludeDir = "%{wks.location}/Beyond/vendor/yaml-cpp/include",
	},
	rapidyaml = {
		IncludeDir = { "%{wks.location}/Beyond/vendor/rapidyaml/include" }
	},
	WS2 = {
		Windows = { LibName = "ws2_32", },
	},
	Dbghelp = {
		Windows = { LibName = "	Dbghelp" },
	},
	FileWatch = {
		IncludeDir = "%{wks.location}/Beyond/vendor/filewatch/include"
	},
	CDT = {
		IncludeDir = "%{wks.location}/Beyond/vendor/CDT"
	},
	RTM = {
		IncludeDir = "%{wks.location}/Beyond/vendor/rtm/include"
	},
	ACL = {
		IncludeDir = "%{wks.location}/Beyond/vendor/acl/include"
	},
	nvvk = {
		IncludeDir = "%{wks.location}/Beyond/vendor/nvvk",
	}
}

-- NOTE: Probably don't touch these functions unless you know what you're doing (or just ask me if you need help extending them)

function LinkDependency(table, is_debug, target)

	-- Setup library directory
	if table.LibDir ~= nil then
		libdirs { table.LibDir }
	end

	-- Try linking
	local libraryName = nil
	if table.LibName ~= nil then
		libraryName = table.LibName
	end

	if table.DebugLibName ~= nil and is_debug and target == "Windows" then
		libraryName = table.DebugLibName
	end

	-- if libraryName ~= nil then
		links { libraryName }
		return true
	-- end

	-- return false
end

function AddDependencyIncludes(table)
	-- if table.IncludeDir ~= nil then
		externalincludedirs { table.IncludeDir }
	-- end
end

function ProcessDependencies(config_name)
	local target = firstToUpper(os.target())

	for key, libraryData in orderedPairs(Dependencies) do

		-- Always match config_name if no Configurations list is specified
		local matchesConfiguration = true

		if config_name ~= nil and libraryData.Configurations ~= nil then
			matchesConfiguration = string.find(libraryData.Configurations, config_name)
		end

		local isDebug = config_name == "Debug"

		if matchesConfiguration then
			local continueLink = true

			-- Process Platform Scope
			if libraryData[target] ~= nil then
				continueLink = not LinkDependency(libraryData[target], isDebug, target)
				AddDependencyIncludes(libraryData[target])
			end

			-- Process Global Scope
			if true then
				LinkDependency(libraryData, isDebug, target)
			end

			AddDependencyIncludes(libraryData)
		end

	end
end

function IncludeDependencies(config_name)
	local target = firstToUpper(os.target())

	for key, libraryData in orderedPairs(Dependencies) do

		-- Always match config_name if no Configurations list is specified
		local matchesConfiguration = true

		if config_name ~= nil and libraryData.Configurations ~= nil then
			matchesConfiguration = string.find(libraryData.Configurations, config_name)
		end

		if matchesConfiguration then
			-- Process Global Scope
			AddDependencyIncludes(libraryData)

			-- Process Platform Scope
			if libraryData[target] ~= nil then
				AddDependencyIncludes(libraryData[target])
			end
		end

	end
end
