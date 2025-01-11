BeyondRootDirectory = os.getenv("Beyond_DIR")
include (path.join(BeyondRootDirectory, "Editor", "Resources", "LUA", "Beyond.lua"))

workspace "$PROJECT_NAME$"
	targetdir "build"
	startproject "$PROJECT_NAME$"
	
	configurations 
	{ 
		"Debug", 
		"Release",
		"Dist"
	}

group "Beyond"
project "ScriptCore"
	location "%{BeyondRootDirectory}/ScriptCore"
	kind "SharedLib"
	language "C#"
	dotnetframework "4.7.2"

	targetdir ("%{BeyondRootDirectory}/Editor/Resources/Scripts")
	objdir ("%{BeyondRootDirectory}/Editor/Resources/Scripts/Intermediates")

	files
	{
		"%{BeyondRootDirectory}/ScriptCore/Source/**.cs",
		"%{BeyondRootDirectory}/ScriptCore/Properties/**.cs"
	}

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

project "$PROJECT_NAME$"
	kind "SharedLib"
	language "C#"
	dotnetframework "4.7.2"

	targetname "$PROJECT_NAME$"
	targetdir ("%{prj.location}/Assets/Scripts/Binaries")
	objdir ("%{prj.location}/Intermediates")

	files 
	{
		"Assets/**.cs", 
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
