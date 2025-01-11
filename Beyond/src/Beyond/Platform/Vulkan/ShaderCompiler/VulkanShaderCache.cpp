#include "pch.h"
#include "VulkanShaderCache.h"

#include <ranges>

#include "Beyond/Core/Hash.h"
#include "Beyond/Platform/Vulkan/VulkanShaderUtils.h"
#include "Beyond/Utilities/SerializationMacros.h"

#include "ryml.hpp"
#include "ShaderPreprocessing/ShaderPreprocessor.h"

namespace Beyond {

	static const char* s_ShaderRegistryPath = "Resources/Cache/Shader/ShaderRegistry.cache";


	VkShaderStageFlagBits VulkanShaderCache::HasChanged(Ref<VulkanShaderCompiler> shader)
	{
		std::map<std::string, std::map<VkShaderStageFlagBits, StageData>> shaderCache;

		Deserialize(shaderCache);

		VkShaderStageFlagBits changedStages = {};
		const bool shaderNotCached = !shaderCache.contains(shader->m_ShaderSourcePath.string());

		for (const auto& stage : shader->m_ShaderSource | std::views::keys)
		{
			// Keep in mind that we're using the [] operator.
			// Which means that we add the stage if it's not already there.
			if (shaderNotCached || shader->m_StagesMetadata.at(stage) != shaderCache[shader->m_ShaderSourcePath.string()][stage])
			{
				shaderCache[shader->m_ShaderSourcePath.string()][stage] = shader->m_StagesMetadata.at(stage);
				*(int*)&changedStages |= stage;
			}
		}

		// Update cache in case we added a stage but didn't remove the deleted(in file) stages
		shaderCache.at(shader->m_ShaderSourcePath.string()) = shader->m_StagesMetadata;

		if (changedStages)
		{
			Serialize(shaderCache);
		}

		return changedStages;
	}


	void VulkanShaderCache::Serialize(const std::map<std::string, std::map<VkShaderStageFlagBits, StageData>>& shaderCache)
	{
		// create an empty tree
		ryml::Tree tree;
		tree.rootref() |= ryml::MAP;

		// add a root node
		auto shaderRegistry = tree.rootref().append_child();
		shaderRegistry << ryml::key("ShaderRegistry");
		shaderRegistry |= ryml::SEQ;

		for (auto& [filepath, shader] : shaderCache)
		{
			auto shaders = shaderRegistry.append_child();
			shaders |= ryml::MAP;

			auto shaderNode = shaders.append_child();
			shaderNode << ryml::key("ShaderPath") << filepath;

			auto stages = shaders.append_child();
			stages << ryml::key("Stages");
			stages |= ryml::SEQ;


			for (auto& [stage, stageData] : shader)
			{
				auto stageNodes = stages.append_child();
				stageNodes |= ryml::MAP;

				stageNodes.append_child() << ryml::key("Stage") << ShaderUtils::ShaderStageToString(stage);
				stageNodes.append_child() << ryml::key("StageHash") << stageData.HashValue;

				auto headers = stageNodes.append_child();
				headers << ryml::key("Headers");
				headers |= ryml::SEQ;

				for (auto& header : stageData.Headers)
				{
					auto headerNodes = headers.append_child();
					headerNodes |= ryml::MAP;

					headerNodes.append_child() << ryml::key("HeaderPath") << header.IncludedFilePath.string();
					headerNodes.append_child() << ryml::key("IsRelative") << header.IsRelative;
					headerNodes.append_child() << ryml::key("IsGuarded") << header.IsGuarded;
					headerNodes.append_child() << ryml::key("IncludeDepth") << header.IncludeDepth;
					headerNodes.append_child() << ryml::key("HashValue") << header.HashValue;
				}
			}
		}


		//YAML::Emitter out;
		FILE* f = fopen(s_ShaderRegistryPath, "wb"); // Open the file for writing
		ryml::EmitterFile out(f); // Create an emitter file object
		out.emit_as(ryml::EMIT_YAML, tree); // Emit the node as YAML
		fclose(f); // Close the file
	}

	void VulkanShaderCache::Deserialize(std::map<std::string, std::map<VkShaderStageFlagBits, StageData>>& shaderCache)
	{
		std::string str = Utils::ReadFileAndSkipBOM(s_ShaderRegistryPath);
		if (str.empty())
			return;

		auto tree = ryml::parse_in_place(ryml::to_substr(str));

		ryml::NodeRef handles = tree["ShaderRegistry"];
		if (handles.key_is_null())
		{
			BEY_CORE_ERROR("[ShaderCache] Shader Registry is invalid.");
			return;
		}

		// Old format
		if (handles.is_map())
		{
			BEY_CORE_ERROR("[ShaderCache] Old Shader Registry format.");
			return;
		}

		for (auto const& shader : handles.children())
		{
			std::string path;
			BEY_DESERIALIZE_PROPERTY_RYML(ShaderPath, path, shader, std::string());

			for (auto stage : shader["Stages"]) //Stages
			{
				std::string stageType;
				uint32_t stageHash;
				stage["Stage"] >> stageType;
				stage["StageHash"] >> stageHash;

				auto& stageCache = shaderCache[path][ShaderUtils::ShaderTypeFromString(stageType)];
				stageCache.HashValue = stageHash;

				for (auto header : stage["Headers"])
				{
					IncludeData headerData{};
					std::string headerPath;

					BEY_DESERIALIZE_PROPERTY_RYML(HeaderPath, headerPath, header, std::string());
					BEY_DESERIALIZE_PROPERTY_RYML(IncludeDepth, headerData.IncludeDepth, header, 0);
					BEY_DESERIALIZE_PROPERTY_RYML(IsRelative, headerData.IsRelative, header, false);
					BEY_DESERIALIZE_PROPERTY_RYML(IsGuarded, headerData.IsGuarded, header, false);
					BEY_DESERIALIZE_PROPERTY_RYML(HashValue, headerData.HashValue, header, 0);

					headerData.IncludedFilePath = headerPath;
					stageCache.Headers.emplace(headerData);
				}
			}
		}
	}
}

