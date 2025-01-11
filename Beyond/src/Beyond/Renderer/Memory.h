#pragma once

namespace Beyond {
	enum class MemoryUsage
	{
		/** No intended memory usage specified.
		Use other members of VmaAllocationCreateInfo to specify your requirements.
		*/
		UNKNOWN = 0,
		/**
		\deprecated Obsolete, preserved for backward compatibility.
		Prefers `VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT`.
		*/
		GPU_ONLY = 1,
		/**
		\deprecated Obsolete, preserved for backward compatibility.
		Guarantees `VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT` and `VK_MEMORY_PROPERTY_HOST_COHERENT_BIT`.
		*/
		CPU_ONLY = 2,
		/**
		\deprecated Obsolete, preserved for backward compatibility.
		Guarantees `VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT`, prefers `VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT`.
		*/
		CPU_TO_GPU = 3,
		/**
		\deprecated Obsolete, preserved for backward compatibility.
		Guarantees `VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT`, prefers `VK_MEMORY_PROPERTY_HOST_CACHED_BIT`.
		*/
		GPU_TO_CPU = 4,
		/**
		\deprecated Obsolete, preserved for backward compatibility.
		Prefers not `VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT`.
		*/
		CPU_COPY = 5,
		/**
		Lazily allocated GPU memory having `VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT`.
		Exists mostly on mobile platforms. Using it on desktop PC or other GPUs with no such memory type present will fail the allocation.

		Usage: Memory for transient attachment images (color attachments, depth attachments etc.), created with `VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT`.

		Allocations with this usage are always created as dedicated - it implies #VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT.
		*/
		GPU_LAZILY_ALLOCATED = 6,
		/**
		Selects best memory type automatically.
		This flag is recommended for most common use cases.

		When using this flag, if you want to map the allocation (using vmaMapMemory() or #VMA_ALLOCATION_CREATE_MAPPED_BIT),
		you must pass one of the flags: #VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT or #VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT
		in VmaAllocationCreateInfo::flags.

		It can be used only with functions that let the library know `VkBufferCreateInfo` or `VkImageCreateInfo`, e.g.
		vmaCreateBuffer(), vmaCreateImage(), vmaFindMemoryTypeIndexForBufferInfo(), vmaFindMemoryTypeIndexForImageInfo()
		and not with generic memory allocation functions.
		*/
		AUTO = 7,
		/**
		Selects best memory type automatically with preference for GPU (device) memory.

		When using this flag, if you want to map the allocation (using vmaMapMemory() or #VMA_ALLOCATION_CREATE_MAPPED_BIT),
		you must pass one of the flags: #VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT or #VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT
		in VmaAllocationCreateInfo::flags.

		It can be used only with functions that let the library know `VkBufferCreateInfo` or `VkImageCreateInfo`, e.g.
		vmaCreateBuffer(), vmaCreateImage(), vmaFindMemoryTypeIndexForBufferInfo(), vmaFindMemoryTypeIndexForImageInfo()
		and not with generic memory allocation functions.
		*/
		AUTO_PREFER_DEVICE = 8,
		/**
		Selects best memory type automatically with preference for CPU (host) memory.

		When using this flag, if you want to map the allocation (using vmaMapMemory() or #VMA_ALLOCATION_CREATE_MAPPED_BIT),
		you must pass one of the flags: #VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT or #VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT
		in VmaAllocationCreateInfo::flags.

		It can be used only with functions that let the library know `VkBufferCreateInfo` or `VkImageCreateInfo`, e.g.
		vmaCreateBuffer(), vmaCreateImage(), vmaFindMemoryTypeIndexForBufferInfo(), vmaFindMemoryTypeIndexForImageInfo()
		and not with generic memory allocation functions.
		*/
		AUTO_PREFER_HOST = 9,
	};
}
