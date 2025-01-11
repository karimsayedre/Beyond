FileVersion = 1.2

BeyondRootDirectory = os.getenv("BEYOND_DIR")
include (path.join(BeyondRootDirectory, "Editor", "Resources", "LUA", "Beyond.lua"))

workspace "Sandbox"
	startproject "Sandbox"
	configurations { "Debug", "Release", "Dist" }

group "Beyond"
project "ScriptCore"
	location "%{BeyondRootDirectory}/ScriptCore"
	kind "SharedLib"
	language "C#"
	dotnetframework "4.7.2"

	targetdir ("%{BeyondRootDirectory}/Editor/Resources/Scripts")
	objdir ("%{BeyondRootDirectory}/Editor/Resources/Scripts/Intermediates")

	files {
		"%{BeyondRootDirectory}/ScriptCore/Source/**.cs",
		"%{BeyondRootDirectory}/ScriptCore/Properties/**.cs"
	}

	linkAppReferences()

	filter "configurations:Debug"
		optimize "Off"
		symbols "Default"

	filter "configurations:Release"
		optimize "On"
		symbols "Default"

	filter "configurations:Dist"
		optimize "Full"
		symbols "Off"

group ""

project "Sandbox"
	location "Assets/Scripts"
	kind "SharedLib"
	language "C#"
	dotnetframework "4.7.2"

	targetname "Sandbox"
	targetdir ("%{prj.location}/Binaries")
	objdir ("%{prj.location}/Intermediates")

	files  {
		"Assets/Scripts/Source/**.cs", 
	}

	linkAppReferences()

	filter "configurations:Debug"
		optimize "Off"
		symbols "Default"

	filter "configurations:Release"
		optimize "On"
		symbols "Default"

	filter "configurations:Dist"
		optimize "Full"
		symbols "Off"
