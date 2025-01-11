/*
 * Copyright (c) 2019-2023, NVIDIA CORPORATION.  All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-FileCopyrightText: Copyright (c) 2019-2021 NVIDIA CORPORATION
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

//#include <vulkan/vulkan_core.h>

#include <memory>
#include <vector>

//#include <memallocator_vk.hpp>
#include "memallocator_vk.hpp"
#include "VulkanMemoryAllocator/vk_mem_alloc.h"


 /**
  \class nvvk::ResourceAllocator

  The goal of nvvk::ResourceAllocator is to aid creation of typical Vulkan
  resources (VkBuffer, VkImage and VkAccelerationStructure).
  All memory is allocated using the provided [nvvk::MemAllocator](#class-nvvkmemallocator)
  and bound to the appropriate resources. The allocator contains a
  [nvvk::StagingMemoryManager](#class-nvvkstagingmemorymanager) and
  [nvvk::SamplerPool](#class-nvvksamplerpool) to aid this process.

  ResourceAllocator separates object creation and memory allocation by delegating allocation
  of memory to an object of interface type 'nvvk::MemAllocator'.
  This way the ResourceAllocator can be used with different memory allocation strategies, depending on needs.
  nvvk provides three implementations of MemAllocator:
  * nvvk::DedicatedMemoryAllocator is using a very simple allocation scheme, one VkDeviceMemory object per allocation.
	This strategy is only useful for very simple applications due to the overhead of vkAllocateMemory and
	an implementation dependent bounded number of vkDeviceMemory allocations possible.
  * nvvk::DMAMemoryAllocator delegates memory requests to a 'nvvk:DeviceMemoryAllocator',
	as an example implemention of a suballocator
  * nvvk::VMAMemoryAllocator delegates memory requests to a [Vulkan Memory Allocator](https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator)

  Utility wrapper structs contain the appropriate Vulkan resource and the
  appropriate nvvk::MemHandle :

  - nvvk::Buffer
  - nvvk::Image
  - nvvk::Texture  contains VkImage and VkImageView as well as an
	optional VkSampler stored witin VkDescriptorImageInfo
  - nvvk::AccelNV
  - nvvk::AccelKHR

  nvvk::Buffer, nvvk::Image, nvvk::Texture and nvvk::AccelKHR nvvk::AccelNV objects can be copied
  by value. They do not track lifetime of the underlying Vulkan objects and memory allocations.
  The corresponding destroy() functions of nvvk::ResourceAllocator destroy created objects and
  free up their memory. ResourceAllocator does not track usage of objects either. Thus, one has to
  make sure that objects are no longer in use by the GPU when they get destroyed.

  > Note: These classes are foremost to showcase principle components that
  > a Vulkan engine would most likely have.
  > They are geared towards ease of use in this sample framework, and
  > not optimized nor meant for production code.

  \code{.cpp}
  nvvk::DeviceMemoryAllocator memAllocator;
  nvvk::ResourceAllocator     resAllocator;

  memAllocator.init(device, physicalDevice);
  resAllocator.init(device, physicalDevice, &memAllocator);

  ...

  VkCommandBuffer cmd = ... transfer queue command buffer

  // creates new resources and
  // implicitly triggers staging transfer copy operations into cmd
  nvvk::Buffer vbo = resAllocator.createBuffer(cmd, vboSize, vboData, vboUsage);
  nvvk::Buffer ibo = resAllocator.createBuffer(cmd, iboSize, iboData, iboUsage);

  // use functions from staging memory manager
  // here we associate the temporary staging resources with a fence
  resAllocator.finalizeStaging( fence );

  // submit cmd buffer with staging copy operations
  vkQueueSubmit(... cmd ... fence ...)

  ...

  // if you do async uploads you would
  // trigger garbage collection somewhere per frame
  resAllocator.releaseStaging();

  \endcode

  Separation of memory allocation and resource creation is very flexible, but it
  can be tedious to set up for simple usecases. nvvk offers three helper ResourceAllocator
  derived classes which internally contain the MemAllocator object and manage its lifetime:
  * [ResourceAllocatorDedicated](#class nvvk::ResourceAllocatorDedicated)
  * [ResourceAllocatorDma](#class nvvk::ResourceAllocatorDma)
  * [ResourceAllocatorVma](#cass nvvk::ResourceAllocatorVma)

  In these cases, only one object needs to be created and initialized.

  ResourceAllocator can also be subclassed to specialize some of its functionality.
  Examples are [ExportResourceAllocator](#class ExportResourceAllocator) and [ExplicitDeviceMaskResourceAllocator](#class ExplicitDeviceMaskResourceAllocator).
  ExportResourceAllocator injects itself into the object allocation process such that
  the resulting allocations can be exported or created objects may be bound to exported
  memory
  ExplicitDeviceMaskResourceAllocator overrides the devicemask of allocations such that
  objects can be created on a specific device in a device group.
  */

namespace nvvk {

	// Objects
	struct Buffer
	{
		VkBuffer  buffer = VK_NULL_HANDLE;
		VmaAllocation memHandle{ nullptr };
	};

	struct Image
	{
		VkImage   image = VK_NULL_HANDLE;
		MemHandle memHandle{ nullptr };
	};

	struct Texture
	{
		VkImage               image = VK_NULL_HANDLE;
		MemHandle             memHandle{ nullptr };
		VkDescriptorImageInfo descriptor{};
	};

	struct AccelNV
	{
		VkAccelerationStructureNV accel = VK_NULL_HANDLE;
		MemHandle                 memHandle{ nullptr };
	};

	struct AccelKHR
	{
		VkAccelerationStructureKHR accel = VK_NULL_HANDLE;
		nvvk::Buffer               buffer;
	};




}  // namespace nvvk
