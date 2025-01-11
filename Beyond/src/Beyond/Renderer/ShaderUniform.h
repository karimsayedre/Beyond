#pragma once

#include "Beyond/Core/Base.h"
#include "Beyond/Core/Log.h"

#include "Beyond/Serialization/StreamReader.h"
#include "Beyond/Serialization/StreamWriter.h"

#include <string>
#include <vector>

#include "Beyond/Platform/Vulkan/RenderPassInput.h"

namespace Beyond {

	class ShaderResourceDeclaration
	{
	public:
		ShaderResourceDeclaration() = default;
		ShaderResourceDeclaration(eastl::string name, RenderPassInputType type, uint32_t set, uint32_t resourceRegister, uint32_t count)
			: m_Name(std::move(name)), m_Type(type), m_Set(set), m_Register(resourceRegister), m_Count(count) { }

		virtual const eastl::string& GetName() const { return m_Name; }
		virtual RenderPassInputType GetType() const { return m_Type; }
		virtual uint32_t GetSet() const { return m_Set; }
		virtual uint32_t GetRegister() const { return m_Register; }
		virtual uint32_t GetCount() const { return m_Count; }

		static void Serialize(StreamWriter* serializer, const ShaderResourceDeclaration& instance)
		{
			serializer->WriteString(instance.m_Name);
			serializer->WriteRaw(instance.m_Set);
			serializer->WriteRaw(instance.m_Register);
			serializer->WriteRaw(instance.m_Count);
		}

		static void Deserialize(StreamReader* deserializer, ShaderResourceDeclaration& instance)
		{
			deserializer->ReadString(instance.m_Name);
			deserializer->ReadRaw(instance.m_Set);
			deserializer->ReadRaw(instance.m_Register);
			deserializer->ReadRaw(instance.m_Count);
		}
	private:
		eastl::string m_Name;
		RenderPassInputType m_Type = RenderPassInputType::None;
		uint32_t m_Set = 0;
		uint32_t m_Register = 0;
		uint32_t m_Count = 0;
	};

	typedef std::vector<ShaderResourceDeclaration*> ShaderResourceList;

}
