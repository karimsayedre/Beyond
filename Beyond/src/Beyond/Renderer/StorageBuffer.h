#pragma once

#include "Beyond/Core/Ref.h"

namespace Beyond {

	struct StorageBufferSpecification
	{
		bool GPUOnly = true;
		eastl::string DebugName;
	};

	class StorageBuffer : public RefCounted
	{
	public:
		virtual ~StorageBuffer() = default;
		virtual void SetData(const void* data, uint32_t size, uint32_t offset = 0) = 0;
		virtual void RT_SetData(const void* data, uint32_t size, uint32_t offset = 0) = 0;
		virtual void Resize(uint32_t newSize) = 0;
		virtual void RT_Resize(uint32_t newSize) = 0;
		virtual uint64_t GetSize() const = 0;

		static Ref<StorageBuffer> Create(uint32_t size, const StorageBufferSpecification& specification);
	};

}
