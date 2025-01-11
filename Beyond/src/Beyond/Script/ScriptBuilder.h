#pragma once

#include "Beyond/Project/Project.h"

namespace Beyond {

	class ScriptBuilder
	{
	public:
		static void BuildCSProject(const std::filesystem::path& filepath);
		static void BuildScriptAssembly(Ref<Project> project);
	};

}
