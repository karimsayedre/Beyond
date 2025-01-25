#pragma once

#include "Beyond/Renderer/ShaderUniform.h"

#include <filesystem>
#include <string>

#include "EASTL/string.h"

#define BEY_HAS_SHADER_COMPILER 1 //!BEY_DIST

namespace Beyond {
	namespace ShaderUtils {
		enum class SourceLang
		{
			NONE, GLSL, HLSL,
		};
	}

	enum class ShaderUniformType
	{
		None = 0, Bool, Int, UInt, Float, Vec2, Vec3, Vec4, Mat3, Mat4,
		IVec2, IVec3, IVec4, Struct
	};

	enum class ShaderStage : uint16_t
	{
		None = 0, Vertex = 0x00000001, Fragment = 0x00000010, Compute = 0x00000020, RayGen = 0x00000100, RayMiss = 0x00000800, RayAnyHit = 0x00000200, RayClosestHit = 0x00000400, RayIntersection = 0x00001000, RayCallable = 0x00002000,
	};

	// The fewer, the better
	enum class RootSignature
	{
		None, Draw, RaytracingHLSL, DDGIVis, DDGICompute, DDGIRaytrace, ComputeGLSL, ComputeHLSL
	};

	inline bool IsRootSignatureHLSL(RootSignature rootSignature)
	{
		switch (rootSignature)
		{
			case RootSignature::ComputeHLSL:
			case RootSignature::DDGIVis:
			case RootSignature::DDGIRaytrace:
			case RootSignature::DDGICompute:
			case RootSignature::RaytracingHLSL: return true;
			case RootSignature::Draw:
			case RootSignature::ComputeGLSL: return false;
			default: BEY_CORE_VERIFY(false, "Unknown Root Signature");
				return false;
		}
	}

	//VkShaderStageFlagBits ShaderStageToVkShaderStage(ShaderStage stage)
	//{
	//	switch (stage)
	//	{
	//		case ShaderStage::Vertex:   return VK_SHADER_STAGE_VERTEX_BIT;
	//		case ShaderStage::Fragment: return VK_SHADER_STAGE_FRAGMENT_BIT;
	//		case ShaderStage::Compute:  return VK_SHADER_STAGE_COMPUTE_BIT;
	//		case ShaderStage::RayGen:  return VK_SHADER_STAGE_RAYGEN_BIT_KHR;
	//		case ShaderStage::RayMiss:  return VK_SHADER_STAGE_MISS_BIT_KHR;
	//		case ShaderStage::RayAnyHit:  return VK_SHADER_STAGE_ANY_HIT_BIT_KHR;
	//		case ShaderStage::RayClosestHit:  return VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
	//		case ShaderStage::RayIntersection:  return VK_SHADER_STAGE_INTERSECTION_BIT_KHR;
	//		case ShaderStage::RayCallable:  return VK_SHADER_STAGE_CALLABLE_BIT_KHR;
	//	}

	//	BEY_CORE_VERIFY(false);
	//	return (VkShaderStageFlagBits)0;
	//}

	//ShaderStage ShaderStageFromVkShaderStage(VkShaderStageFlagBits stage)
	//{
	//	switch (stage)
	//	{
	//		case VK_SHADER_STAGE_VERTEX_BIT:   return ShaderStage::Vertex;
	//		case VK_SHADER_STAGE_FRAGMENT_BIT: return ShaderStage::Fragment;
	//		case VK_SHADER_STAGE_COMPUTE_BIT:  return ShaderStage::Compute;
	//		case VK_SHADER_STAGE_RAYGEN_BIT_KHR:  return ShaderStage::RayGen;
	//		case VK_SHADER_STAGE_MISS_BIT_KHR:  return ShaderStage::RayMiss;
	//		case VK_SHADER_STAGE_ANY_HIT_BIT_KHR:  return ShaderStage::RayAnyHit;
	//		case VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR:  return ShaderStage::RayClosestHit;
	//		case VK_SHADER_STAGE_INTERSECTION_BIT_KHR:  return ShaderStage::RayIntersection;
	//		case VK_SHADER_STAGE_CALLABLE_BIT_KHR:  return ShaderStage::RayCallable;
	//	}

	//	BEY_CORE_VERIFY(false);
	//	return (ShaderStage)0;
	//}

	class ShaderUniform
	{
	public:
		ShaderUniform() = default;
		ShaderUniform(eastl::string name, ShaderUniformType type, uint32_t size, uint32_t offset);

		const eastl::string& GetName() const { return m_Name; }
		ShaderUniformType GetType() const { return m_Type; }
		uint32_t GetSize() const { return m_Size; }
		uint32_t GetOffset() const { return m_Offset; }

		static constexpr eastl::string_view UniformTypeToString(ShaderUniformType type);

		static void Serialize(StreamWriter* serializer, const ShaderUniform& instance)
		{
			serializer->WriteString(instance.m_Name);
			serializer->WriteRaw(instance.m_Type);
			serializer->WriteRaw(instance.m_Size);
			serializer->WriteRaw(instance.m_Offset);
		}

		static void Deserialize(StreamReader* deserializer, ShaderUniform& instance)
		{
			deserializer->ReadString(instance.m_Name);
			deserializer->ReadRaw(instance.m_Type);
			deserializer->ReadRaw(instance.m_Size);
			deserializer->ReadRaw(instance.m_Offset);
		}
	private:
		eastl::string m_Name;
		ShaderUniformType m_Type = ShaderUniformType::None;
		uint32_t m_Size = 0;
		uint32_t m_Offset = 0;
	};

	struct ShaderUniformBuffer
	{
		eastl::string Name;
		uint32_t Index;
		uint32_t BindingPoint;
		uint32_t Size;
		uint32_t RendererID;
		std::vector<ShaderUniform> Uniforms;
	};

	struct ShaderStorageBuffer
	{
		eastl::string Name;
		uint32_t Index;
		uint32_t BindingPoint;
		uint32_t Size;
		uint32_t RendererID;
		//std::vector<ShaderUniform> Uniforms;
	};

	struct ShaderBuffer
	{
		eastl::string Name;
		uint32_t Size = 0;
		eastl::unordered_map<eastl::string, ShaderUniform> Uniforms;

		static void Serialize(StreamWriter* serializer, const ShaderBuffer& instance)
		{
			serializer->WriteString(instance.Name);
			serializer->WriteRaw(instance.Size);
			serializer->WriteMap(instance.Uniforms);
		}

		static void Deserialize(StreamReader* deserializer, ShaderBuffer& instance)
		{
			deserializer->ReadString(instance.Name);
			deserializer->ReadRaw(instance.Size);
			deserializer->ReadMap(instance.Uniforms);
		}
	};

	class Shader : public RefCounted
	{
	public:
		using ShaderReloadedCallback = std::function<void()>;

		virtual void Reload(bool forceCompile = false) = 0;
		virtual void RT_Reload(bool forceCompile) = 0;

		virtual uint32_t GetHash() const = 0;
		virtual RootSignature GetRootSignature() const = 0;

		virtual const std::string& GetName() const = 0;

		virtual void SetMacro(const std::string& name, const std::string& value) = 0;

		static Ref<Shader> Create(const std::string& filepath, bool forceCompile = false, bool disableOptimization = false);
		static Ref<Shader> LoadFromShaderPack(const std::string& filepath, bool forceCompile = false, bool disableOptimization = false);
		static Ref<Shader> CreateFromString(const std::string& source);

		virtual const eastl::unordered_map<eastl::string, ShaderBuffer>& GetShaderBuffers() const = 0;
		virtual const eastl::unordered_map<eastl::string, ShaderResourceDeclaration>& GetResources() const = 0;

		virtual void AddShaderReloadedCallback(const ShaderReloadedCallback& callback) = 0;

		static constexpr const char* GetShaderDirectoryPath()
		{
			return "Resources/Shaders/";
		}
	};

	class ShaderPack;

	// This should be eventually handled by the Asset Manager
	class ShaderLibrary : public RefCounted
	{
	public:
		ShaderLibrary();
		~ShaderLibrary() override;

		void Add(const Ref<Shader>& shader);
		void Load(const RootSignature rootSignature, std::string_view path, bool forceCompile, bool disableOptimization, bool external, const std::wstring& entryPoint, const std::wstring& targetProfile, const std::vector<std::pair<std::wstring, std::wstring>>& defines);
		void Load(const RootSignature rootSignature, std::string_view path, bool forceCompile = false, bool disableOptimization = false);
		void Load(std::string_view name, const std::string& path);
		void LoadShaderPack(const std::filesystem::path& path);

		const Ref<Shader>& Get(const std::string& name, const uint32_t index = 0) const;
		size_t GetSize() const { return m_Shaders.size(); }

		std::unordered_map<std::string, std::vector<Ref<Shader>>>& GetShaders() { return m_Shaders; }
		const std::unordered_map<std::string, std::vector<Ref<Shader>>>& GetShaders() const { return m_Shaders; }
	private:
		std::unordered_map<std::string, std::vector<Ref<Shader>>> m_Shaders;
		Ref<ShaderPack> m_ShaderPack;
	};

}
