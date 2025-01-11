#pragma once
#include <vulkan/vulkan_core.h>

#include "RendererResource.h"
#include "Beyond/Core/Ref.h"

#include "RendererTypes.h"

namespace Beyond {

	class IndexBuffer : public RendererResource
	{
	public:
		virtual ~IndexBuffer() {}

		virtual void SetData(void* buffer, uint64_t size, uint64_t offset = 0) = 0;
		virtual void Bind() const = 0;
		virtual bool IsReady() const = 0;

		virtual uint64_t GetCount() const = 0;

		virtual uint64_t GetSize() const = 0;
		virtual RendererID GetRendererID() const = 0;

		ResourceDescriptorInfo GetDescriptorInfo() const override = 0;
		uint32_t GetBindlessIndex() const override = 0;
		virtual uint64_t GetBufferDeviceAddress(VkDevice device) = 0;

		static Ref<IndexBuffer> Create(void* data, const std::string& name, uint64_t size = 0);
	};
}

