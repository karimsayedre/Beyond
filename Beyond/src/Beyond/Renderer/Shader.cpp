#include "pch.h"
#include "Shader.h"

#include <utility>

#include "Beyond/Renderer/Renderer.h"
#include "Beyond/Platform/Vulkan/VulkanShader.h"

#if BEY_HAS_SHADER_COMPILER
#include "Beyond/Platform/Vulkan/ShaderCompiler/VulkanShaderCompiler.h"
#endif

#include "Beyond/Renderer/RendererAPI.h"
#include "Beyond/Renderer/ShaderPack.h"

namespace Beyond {

	Ref<Shader> Shader::Create(const std::string& filepath, bool forceCompile, bool disableOptimization)
	{
		Ref<Shader> result = nullptr;

		switch (RendererAPI::Current())
		{
			case RendererAPIType::None: return nullptr;
			case RendererAPIType::Vulkan:
				result = Ref<VulkanShader>::Create(filepath, forceCompile, disableOptimization);
				break;
		}
		return result;
	}

	Ref<Shader> Shader::CreateFromString(const std::string& source)
	{
		Ref<Shader> result = nullptr;

		switch (RendererAPI::Current())
		{
			case RendererAPIType::None: return nullptr;
		}
		return result;
	}

	ShaderLibrary::ShaderLibrary()
	{
	}

	ShaderLibrary::~ShaderLibrary()
	{
	}

	void ShaderLibrary::Add(const Beyond::Ref<Shader>& shader)
	{
		auto& name = shader->GetName();
		BEY_CORE_ASSERT(m_Shaders.find(name) == m_Shaders.end());
		m_Shaders[name].emplace_back(shader);
	}

	void ShaderLibrary::Load(const RootSignature rootSignature, std::string_view path, bool forceCompile, bool disableOptimization, bool external, const std::wstring& entryPoint,
		const std::wstring& targetProfile, const std::vector<std::pair<std::wstring, std::wstring>>& defines)
	{
		Ref<Shader> shader;
		if (!forceCompile && m_ShaderPack)
		{
			if (m_ShaderPack->Contains(path))
				shader = m_ShaderPack->LoadShader(path);
		}
		else
		{
			// Try compile from source
			// Unavailable at runtime
#if BEY_HAS_SHADER_COMPILER
			shader = VulkanShaderCompiler::Compile(rootSignature, path, forceCompile, disableOptimization, external, entryPoint, targetProfile, defines);
#endif
		}

		auto& name = shader->GetName();
		//BEY_CORE_ASSERT(m_Shaders.find(name) == m_Shaders.end());
		m_Shaders[name].emplace_back(shader);
	}

	void ShaderLibrary::Load(const RootSignature rootSignature, std::string_view path, bool forceCompile, bool disableOptimization)
	{
		Ref<Shader> shader;
		if (!forceCompile && m_ShaderPack)
		{
			if (m_ShaderPack->Contains(path))
				shader = m_ShaderPack->LoadShader(path);
		}
		else
		{
			// Try compile from source
			// Unavailable at runtime
#if BEY_HAS_SHADER_COMPILER
			shader = VulkanShaderCompiler::Compile(rootSignature, path, forceCompile, disableOptimization); 
#endif
		}

		auto& name = shader->GetName();
		BEY_CORE_ASSERT(m_Shaders.find(name) == m_Shaders.end());
		m_Shaders[name].emplace_back(shader);
	}

	void ShaderLibrary::Load(std::string_view name, const std::string& path)
	{
		BEY_CORE_ASSERT(m_Shaders.contains(std::string(name)));
		m_Shaders[std::string(name)].emplace_back(Shader::Create(path));
	}

	void ShaderLibrary::LoadShaderPack(const std::filesystem::path& path)
	{
		m_ShaderPack = Ref<ShaderPack>::Create(path);
		if (!m_ShaderPack->IsLoaded())
		{
			m_ShaderPack = nullptr;
			BEY_CORE_ERROR("Could not load shader pack: {}", path.string());
		}
	}

	const Ref<Shader>& ShaderLibrary::Get(const std::string& name, const uint32_t index) const
	{
		BEY_CORE_ASSERT(m_Shaders.contains(name));
		BEY_CORE_ASSERT(m_Shaders.at(name).size() > index);
		return m_Shaders.at(name)[index];
	}

	ShaderUniform::ShaderUniform(eastl::string name, const ShaderUniformType type, const uint32_t size, const uint32_t offset)
		: m_Name(std::move(name)), m_Type(type), m_Size(size), m_Offset(offset)
	{
	}

	constexpr eastl::string_view ShaderUniform::UniformTypeToString(const ShaderUniformType type)
	{
		if (type == ShaderUniformType::Bool)
		{
			return eastl::string("Boolean");
		}
		else if (type == ShaderUniformType::Int)
		{
			return eastl::string("Int");
		}
		else if (type == ShaderUniformType::Float)
		{
			return eastl::string("Float");
		}

		return eastl::string("None");
	}

}
