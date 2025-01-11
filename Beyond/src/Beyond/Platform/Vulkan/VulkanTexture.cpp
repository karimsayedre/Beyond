#include "pch.h"
#include "VulkanTexture.h"

#include "VulkanContext.h"

#include "VulkanImage.h"
#include "VulkanRenderer.h"
#include "VulkanAPI.h"

#include "Beyond/Asset/TextureImporter.h"
#include "Beyond/Core/Thread.h"

namespace Beyond {

	namespace Utils {



		static size_t GetMemorySize(ImageFormat format, uint32_t width, uint32_t height)
		{
			switch (format)
			{
				case ImageFormat::RED16UI: return width * height * sizeof(uint16_t);
				case ImageFormat::RG16F: return width * height * 2 * sizeof(uint16_t);
				case ImageFormat::RG32F: return width * height * 2 * sizeof(float);
				case ImageFormat::RED32F: return width * height * sizeof(float);
				case ImageFormat::RED16F: return width * height * sizeof(short);
				case ImageFormat::RED8UN: return width * height;
				case ImageFormat::RED8UI: return width * height;
				case ImageFormat::RGBA: return width * height * 4;
				case ImageFormat::SRGBA: return width * height * 4;
				case ImageFormat::RGBA32F: return width * height * 4 * sizeof(float);
				case ImageFormat::RGB32F: return width * height * 3 * sizeof(float);
				case ImageFormat::A2B10R11G11UNorm: return width * height * sizeof(float);
				case ImageFormat::B10G11R11UFLOAT: return width * height * sizeof(float);
				case ImageFormat::RGBA16F: return width * height * 4 * sizeof(uint16_t);
			}
			BEY_CORE_ASSERT(false);
			return 0;
		}

		static bool ValidateSpecification(const TextureSpecification& specification)
		{
			bool result = true;

			result = specification.Width > 0 && specification.Height > 0 && specification.Width < 65536 && specification.Height < 65536;
			BEY_CORE_VERIFY(result);

			return result;
		}

	}

	//////////////////////////////////////////////////////////////////////////////////
	// Texture2D
	//////////////////////////////////////////////////////////////////////////////////



	VulkanTexture2D::VulkanTexture2D(const TextureSpecification& specification, const std::filesystem::path& filepath)
		: m_Path(filepath), m_Specification(specification), m_Thread(specification.DebugName)
	{

		Utils::ValidateSpecification(specification);
		if (specification.DebugName.empty())
		{
			const auto& name = filepath.filename().string();
			m_Specification.DebugName = eastl::string(name.c_str(), name.size());
		}
		BEY_CORE_ASSERT(!m_Specification.DebugName.empty(), "Name all the textures.");

		m_Thread.Dispatch([instance = Ref(this), filepath]() mutable
		{
			instance->m_ImageData = TextureImporter::ToBufferFromFile(instance->m_Path, instance->m_FoundTexture, instance->m_Specification);
			if (instance->m_ImageData.empty() || !instance->m_ImageData[0].Data)
			{
				// TODO: move this to asset manager
				std::filesystem::path path = "Resources/Textures/ErrorTexture.png";
				instance->m_ImageData = TextureImporter::ToBufferFromFile(path, instance->m_FoundTexture, instance->m_Specification);
			}

			ImageSpecification imageSpec;
			imageSpec.Format = instance->m_Specification.Format;
			imageSpec.Width = instance->m_Specification.Width;
			imageSpec.Height = instance->m_Specification.Height;
			imageSpec.Mips = instance->m_Specification.GenerateMips ? (instance->m_Specification.Compress ? (uint32_t)instance->m_ImageData.size() : instance->GetMipLevelCount()) : 1;
			imageSpec.DebugName = instance->m_Specification.DebugName;
			imageSpec.CreateSampler = false;
			imageSpec.CreateBindlessDescriptor = instance->m_Specification.CreateBindlessDescriptor;
			imageSpec.HasTransparency = instance->m_Specification.HasTransparency;
			instance->m_Image = Image2D::Create(imageSpec);

			BEY_CORE_ASSERT(instance->m_Specification.Format != ImageFormat::None);

			Renderer::Submit([instance]() mutable
			{
				instance->Invalidate();
				instance->m_IsReady = true;
			});

			instance->m_ThreadDone = true;
		});
		m_Thread.Join();
	}

	VulkanTexture2D::VulkanTexture2D(const TextureSpecification& specification, const std::vector<Buffer>& imageData)
		: m_Specification(specification), m_Thread(specification.DebugName)
	{
		m_FoundTexture = true;
		BEY_CORE_ASSERT(!m_Specification.DebugName.empty(), "Name all the textures.");

		if (m_Specification.Height == 0)
		{

			if (imageData.empty())
			{
				// TODO: move this to asset manager
				std::filesystem::path path = "Resources/Textures/ErrorTexture.png";
				m_ImageData = TextureImporter::ToBufferFromFile(path, m_FoundTexture, m_Specification);
			}
			else
			{
				for (const auto& data : imageData)
					m_ImageData.emplace_back(TextureImporter::ToBufferFromMemory(Buffer(data.Data, m_Specification.Width), m_Specification));
			}

			Utils::ValidateSpecification(m_Specification);
		}
		else if (!imageData.empty() && imageData[0])
		{
			Utils::ValidateSpecification(m_Specification);
			for (auto buffer : imageData)
				m_ImageData.emplace_back(Buffer::Copy(buffer.Data, specification.Compress ? buffer.Size : (uint32_t)Utils::GetMemorySize(m_Specification.Format, m_Specification.Width, m_Specification.Height)));
		}
		else
		{
			Utils::ValidateSpecification(m_Specification);
			auto size = (uint32_t)Utils::GetMemorySize(m_Specification.Format, m_Specification.Width, m_Specification.Height);
			m_ImageData.emplace_back();
			m_ImageData[0].Allocate(size);
			m_ImageData[0].ZeroInitialize();
		}

		BEY_CORE_ASSERT(m_Specification.Layers >= 1 || !m_Specification.GenerateMips, "No support for both in the same texture!");

		ImageSpecification imageSpec;
		imageSpec.Format = m_Specification.Format;
		imageSpec.Width = m_Specification.Width;
		imageSpec.Height = m_Specification.Height;
		imageSpec.Mips = m_Specification.GenerateMips ? (m_Specification.Compress ? (uint32_t)m_ImageData.size() : VulkanTexture2D::GetMipLevelCount()) : 1;
		imageSpec.Layers = (m_Specification.Layers > 1 ? (uint32_t)m_ImageData.size() : 1);
		imageSpec.DebugName = m_Specification.DebugName;
		imageSpec.CreateSampler = false;
		imageSpec.CreateBindlessDescriptor = m_Specification.CreateBindlessDescriptor;
		imageSpec.HasTransparency = m_Specification.HasTransparency;
		if (specification.Storage)
			imageSpec.Usage |= ImageUsage::Storage;
		m_Image = Image2D::Create(imageSpec);

		Ref<VulkanTexture2D> instance = this;
		Renderer::Submit([instance]() mutable
		{
			instance->Invalidate();
		});
		m_ThreadDone = true;
		m_IsReady = true;
	}

	VulkanTexture2D::~VulkanTexture2D()
	{
		if (m_Image)
			m_Image->Release();

		for (auto& buffer : m_ImageData)
			buffer.Release();
		//m_Thread.Stop();
	}

	void VulkanTexture2D::Resize(const glm::uvec2& size)
	{
		Resize(size.x, size.y);
	}

	void VulkanTexture2D::Resize(const uint32_t width, const uint32_t height)
	{
		m_Specification.Width = width;
		m_Specification.Height = height;

		//Invalidate();

		Ref<VulkanTexture2D> instance = this;
		Renderer::Submit([instance]() mutable
		{
			instance->Invalidate();
		});
	}

	void VulkanTexture2D::Invalidate()
	{
		auto device = VulkanContext::GetCurrentDevice();
		auto vulkanDevice = device->GetVulkanDevice();

		m_Image->Release();

		uint32_t mipCount = m_Specification.GenerateMips ? (m_Specification.Compress ? (uint32_t)m_ImageData.size() : VulkanTexture2D::GetMipLevelCount()) : 1;

		ImageSpecification& imageSpec = m_Image->GetSpecification();
		imageSpec.Format = m_Specification.Format;
		imageSpec.Width = m_Specification.Width;
		imageSpec.Height = m_Specification.Height;
		imageSpec.Mips = mipCount;
		imageSpec.CreateSampler = false;
		imageSpec.CreateBindlessDescriptor = true;
		imageSpec.DebugName = m_Path.empty() ? m_Specification.DebugName : m_Path.string().c_str();
		imageSpec.Layers = m_Specification.Layers;

		if (m_ImageData.empty() || !m_ImageData[0].Data) // TODO: better management for this, probably from texture spec
			imageSpec.Usage |= ImageUsage::Storage;

		Ref<VulkanImage2D> image = m_Image.As<VulkanImage2D>();
		image->RT_Invalidate();

		auto& info = image->GetImageInfo();

		if (!m_ImageData.empty() && m_ImageData[0].Data)
		{
			VulkanAllocator allocator("Texture2D");
			VkMemoryAllocateInfo memAllocInfo{};
			memAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
			uint32_t mip = 0;
			uint32_t layer = 0;
			for (const Buffer& buffer : m_ImageData)
			{
				if (buffer.Size == 0)
					break;
				VkDeviceSize size = buffer.Size;



				// Create staging buffer
				VkBufferCreateInfo bufferCreateInfo{};
				bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
				bufferCreateInfo.size = size;
				bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
				bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
				VkBuffer stagingBuffer;
				VmaAllocation stagingBufferAllocation = allocator.AllocateBuffer(bufferCreateInfo, VMA_MEMORY_USAGE_CPU_TO_GPU, stagingBuffer);

				// Copy data to staging buffer
				uint8_t* destData = allocator.MapMemory<uint8_t>(stagingBufferAllocation);
				BEY_CORE_ASSERT(buffer.Data);
				memcpy(destData, buffer.Data, size);
				allocator.UnmapMemory(stagingBufferAllocation);

				VkCommandBuffer copyCmd = device->CreateCommandBuffer(fmt::eastl_format("copying texture2D from host buffer named: {}", m_Specification.DebugName), true);

				// The sub resource range describes the regions of the image that will be transitioned using the memory barriers below
				VkImageSubresourceRange subresourceRange = {};
				// Image only contains color data
				subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				// Start at first mip level
				subresourceRange.baseMipLevel = mip;
				subresourceRange.levelCount = 1;
				subresourceRange.layerCount = 1;
				subresourceRange.baseArrayLayer = layer;

				// Transition the texture image layout to transfer target, so we can safely copy our buffer data to it.
				VkImageMemoryBarrier imageMemoryBarrier{};
				imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
				imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				imageMemoryBarrier.image = info.Image;
				imageMemoryBarrier.subresourceRange = subresourceRange;
				imageMemoryBarrier.srcAccessMask = 0;
				imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
				imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
				imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

				// Insert a memory dependency at the proper pipeline stages that will execute the image layout transition 
				// Source pipeline stage is host write/read exection (VK_PIPELINE_STAGE_HOST_BIT)
				// Destination pipeline stage is copy command exection (VK_PIPELINE_STAGE_TRANSFER_BIT)
				vkCmdPipelineBarrier(
					copyCmd,
					VK_PIPELINE_STAGE_HOST_BIT,
					VK_PIPELINE_STAGE_TRANSFER_BIT,
					0,
					0, nullptr,
					0, nullptr,
					1, &imageMemoryBarrier);

				VkBufferImageCopy bufferCopyRegion = {};
				bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				bufferCopyRegion.imageSubresource.mipLevel = mip;
				bufferCopyRegion.imageSubresource.baseArrayLayer = layer;
				bufferCopyRegion.imageSubresource.layerCount = 1;
				bufferCopyRegion.imageExtent.width =  glm::max(uint32_t(m_Specification.Width >> (mip)), 1u);
				bufferCopyRegion.imageExtent.height = glm::max(uint32_t(m_Specification.Height >> (mip)), 1u);
				bufferCopyRegion.imageExtent.depth = 1;
				bufferCopyRegion.bufferOffset = 0;

				// Copy mip levels from staging buffer
				vkCmdCopyBufferToImage(
					copyCmd,
					stagingBuffer,
					info.Image,
					VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
					1,
					&bufferCopyRegion);

#if 0
				// Once the data has been uploaded we transfer to the texture image to the shader read layout, so it can be sampled from
				imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
				imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
				imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
				imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

				// Insert a memory dependency at the proper pipeline stages that will execute the image layout transition 
				// Source pipeline stage stage is copy command exection (VK_PIPELINE_STAGE_TRANSFER_BIT)
				// Destination pipeline stage fragment shader access (VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT)
				vkCmdPipelineBarrier(
					copyCmd,
					VK_PIPELINE_STAGE_TRANSFER_BIT,
					VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
					0,
					0, nullptr,
					0, nullptr,
					1, &imageMemoryBarrier);

#endif

				if (m_Specification.Compress)
				{
					Utils::InsertImageMemoryBarrier(copyCmd, info.Image,
						VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
						VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
						VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
						subresourceRange);
				}
				else if (mipCount > 1) // Mips to generate
				{
					Utils::InsertImageMemoryBarrier(copyCmd, info.Image,
						VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
						VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
						VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
						subresourceRange);
				}
				else
				{
					Utils::InsertImageMemoryBarrier(copyCmd, info.Image,
						VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
						VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, image->GetVulkanDescriptorInfo().imageLayout,
						VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
						subresourceRange);
				}


				device->FlushCommandBuffer(copyCmd);

				// Clean up staging resources
				allocator.DestroyBuffer(stagingBuffer, stagingBufferAllocation);

				if (m_Specification.GenerateMips)
					mip++;

				if (m_Specification.Layers > 1)
					layer++;
				
			}
		}
		else
		{
			VkCommandBuffer transitionCommandBuffer = device->CreateCommandBuffer(fmt::eastl_format("transitioning texture named: {}", m_Specification.DebugName), true);
			VkImageSubresourceRange subresourceRange = {};
			subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			subresourceRange.layerCount = 1;
			subresourceRange.levelCount = m_Specification.GenerateMips ? (m_Specification.Compress ? (uint32_t)m_ImageData.size() : VulkanTexture2D::GetMipLevelCount()) : 1;

			Utils::SetImageLayout(transitionCommandBuffer, info.Image, VK_IMAGE_LAYOUT_UNDEFINED, image->GetVulkanDescriptorInfo().imageLayout, subresourceRange,
						(!m_ImageData.empty() && m_ImageData[0].Data) ? VK_PIPELINE_STAGE_TRANSFER_BIT : VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
			device->FlushCommandBuffer(transitionCommandBuffer);
		}

		////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		// CREATE TEXTURE SAMPLER (owned by Image)
		////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

		// Create a texture sampler
		VkSamplerCreateInfo samplerInfo{};
		samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		samplerInfo.maxAnisotropy = 1.0f;
		samplerInfo.magFilter = VulkanSamplerFilter(m_Specification.SamplerFilter);
		samplerInfo.minFilter = VulkanSamplerFilter(m_Specification.SamplerFilter);
		samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerInfo.addressModeU = VulkanSamplerWrap(m_Specification.SamplerWrap);
		samplerInfo.addressModeV = VulkanSamplerWrap(m_Specification.SamplerWrap);
		samplerInfo.addressModeW = VulkanSamplerWrap(m_Specification.SamplerWrap);
		samplerInfo.mipLodBias = 0.0f;
		samplerInfo.compareOp = VK_COMPARE_OP_NEVER;
		samplerInfo.minLod = 0.0f;
		samplerInfo.maxLod = (float)mipCount;
		// Enable anisotropic filtering
		// This feature is optional, so we must check if it's supported on the device

		// TODO:
		/*if (vulkanDevice->features.samplerAnisotropy) {
				// Use max. level of anisotropy for this example
				sampler.maxAnisotropy = 1.0f;// vulkanDevice->properties.limits.maxSamplerAnisotropy;
				sampler.anisotropyEnable = VK_TRUE;
		}
		else {
				// The device does not support anisotropic filtering
				sampler.maxAnisotropy = 1.0;
				sampler.anisotropyEnable = VK_FALSE;
		}*/
		samplerInfo.maxAnisotropy = 1.0;
		samplerInfo.anisotropyEnable = VK_FALSE;
		samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;

		info.Sampler = Vulkan::CreateSampler(samplerInfo);
		VKUtils::SetDebugUtilsObjectName(device->GetVulkanDevice(), VK_OBJECT_TYPE_SAMPLER, fmt::eastl_format("{} - texture default sampler", m_Specification.DebugName), info.Sampler);

		image->UpdateDescriptor();

		if (!m_Specification.Storage)
		{
			VkImageViewCreateInfo view{};
			view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			view.viewType = VK_IMAGE_VIEW_TYPE_2D;
			view.format = Utils::VulkanImageFormat(m_Specification.Format);
			view.components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
			// The subresource range describes the set of mip levels (and array layers) that can be accessed through this image view
			// It's possible to create multiple image views for a single image referring to different (and/or overlapping) ranges of the image
			view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			view.subresourceRange.baseMipLevel = 0;
			view.subresourceRange.baseArrayLayer = 0;
			view.subresourceRange.layerCount = m_Specification.Layers;
			view.subresourceRange.levelCount = mipCount;
			view.image = info.Image;
			VK_CHECK_RESULT(vkCreateImageView(vulkanDevice, &view, nullptr, &info.ImageView));

			VKUtils::SetDebugUtilsObjectName(vulkanDevice, VK_OBJECT_TYPE_IMAGE_VIEW, fmt::eastl_format("Texture view: {}", m_Specification.DebugName), info.ImageView);

			image->UpdateDescriptor();
		}


		if (m_Specification.CreateBindlessDescriptor)
		{
			RenderPassInput input;
			input.Type = RenderPassResourceType::Texture2D;
			input.Input[GetBindlessIndex()] = this;
			input.Name = "bls_MaterialTextures";
			Renderer::AddBindlessDescriptor(std::move(input));
		}

		if (!m_ImageData.empty() && m_ImageData[0].Data && m_Specification.GenerateMips && !m_Specification.Compress && mipCount > 1)
			GenerateMips();

		m_IsReady = true;

		// TODO: option for local storage
		for (auto& buffer : m_ImageData)
		{
			buffer.Release();
			buffer = Buffer();
		}
	}

	void VulkanTexture2D::Bind(uint32_t slot) const
	{
	}

	void VulkanTexture2D::Lock()
	{
	}

	void VulkanTexture2D::Unlock()
	{
	}

	Buffer VulkanTexture2D::GetWriteableBuffer()
	{
		if (m_ImageData.empty())
			m_ImageData.emplace_back();
		return m_ImageData[0];
	}

	const std::filesystem::path& VulkanTexture2D::GetPath() const
	{
		return m_Path;
	}

	uint32_t VulkanTexture2D::GetMipLevelCount() const
	{
		return Utils::CalculateMipCount(m_Specification.Width, m_Specification.Height);
	}

	std::pair<uint32_t, uint32_t> VulkanTexture2D::GetMipSize(uint32_t mip) const
	{
		uint32_t width = m_Specification.Width;
		uint32_t height = m_Specification.Height;
		while (mip != 0)
		{
			width /= 2;
			height /= 2;
			mip--;
		}

		return { width, height };
	}

	void VulkanTexture2D::GenerateMips()
	{
		auto device = VulkanContext::GetCurrentDevice();
		auto vulkanDevice = device->GetVulkanDevice();

		Ref<VulkanImage2D> image = m_Image.As<VulkanImage2D>();
		const auto& info = image->GetImageInfo();

		const VkCommandBuffer blitCmd = VulkanContext::GetCurrentDevice()->CreateCommandBuffer("VulkanTexture2D::GenerateMips() blitCmd", true);

		const auto mipLevels = GetMipLevelCount();
		for (uint32_t i = 1; i < mipLevels; i++)
		{
			VkImageBlit imageBlit{};

			// Source
			imageBlit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			imageBlit.srcSubresource.layerCount = 1;
			imageBlit.srcSubresource.mipLevel = i - 1;
			imageBlit.srcOffsets[1].x = int32_t(m_Specification.Width >> (i - 1));
			imageBlit.srcOffsets[1].y = int32_t(m_Specification.Height >> (i - 1));
			imageBlit.srcOffsets[1].z = 1;

			// Destination
			imageBlit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			imageBlit.dstSubresource.layerCount = 1;
			imageBlit.dstSubresource.mipLevel = i;
			imageBlit.dstOffsets[1].x = int32_t(m_Specification.Width >> i);
			imageBlit.dstOffsets[1].y = int32_t(m_Specification.Height >> i);
			imageBlit.dstOffsets[1].z = 1;

			VkImageSubresourceRange mipSubRange = {};
			mipSubRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			mipSubRange.baseMipLevel = i;
			mipSubRange.levelCount = 1;
			mipSubRange.layerCount = 1;

			// Prepare current mip level as image blit destination
			Utils::InsertImageMemoryBarrier(blitCmd, info.Image,
											0, VK_ACCESS_TRANSFER_WRITE_BIT,
											VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
											VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
											mipSubRange);

			// Blit from previous level
			vkCmdBlitImage(
				blitCmd,
				info.Image,
				VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				info.Image,
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				1,
				&imageBlit,
				VulkanSamplerFilter(m_Specification.SamplerFilter));

			// Prepare current mip level as image blit source for next level
			Utils::InsertImageMemoryBarrier(blitCmd, info.Image,
											VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
											VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
											VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
											mipSubRange);
		}

		// After the loop, all mip layers are in TRANSFER_SRC layout, so transition all to SHADER_READ
		VkImageSubresourceRange subresourceRange = {};
		subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		subresourceRange.layerCount = 1;
		subresourceRange.levelCount = mipLevels;

		Utils::InsertImageMemoryBarrier(blitCmd, info.Image,
										VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_SHADER_READ_BIT,
										VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
										VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
										subresourceRange);

		VulkanContext::GetCurrentDevice()->FlushCommandBuffer(blitCmd);

#if 0
		VkImageMemoryBarrier barrier = {};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.image = m_Image;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

		VkImageSubresourceRange subresourceRange = {};
		subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		subresourceRange.levelCount = 1;
		subresourceRange.layerCount = 1;
		barrier.subresourceRange = subresourceRange;

		int32_t mipWidth = m_Specification.Width;
		int32_t mipHeight = m_Specification.Height;

		VkCommandBuffer commandBuffer = VulkanContext::GetCurrentDevice()->CreateCommandBuffer(true);

		barrier.subresourceRange.baseMipLevel = 0;
		barrier.subresourceRange.baseArrayLayer = 0;
		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

		vkCmdPipelineBarrier(commandBuffer,
							 VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
							 0, nullptr,
							 0, nullptr,
							 1, &barrier);

		auto mipLevels = GetMipLevelCount();
		for (uint32_t i = 1; i < mipLevels; i++)
		{
			VkImageBlit blit = {};
			blit.srcOffsets[0] = { 0, 0, 0 };
			blit.srcOffsets[1] = { mipWidth, mipHeight, 1 };
			blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			blit.srcSubresource.mipLevel = i - 1;
			blit.srcSubresource.baseArrayLayer = 0;
			blit.srcSubresource.layerCount = 1;

			blit.dstOffsets[0] = { 0, 0, 0 };
			blit.dstOffsets[1] = { mipWidth / 2, mipHeight / 2, 1 };
			blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			blit.dstSubresource.mipLevel = i;
			blit.dstSubresource.baseArrayLayer = 0;
			blit.dstSubresource.layerCount = 1;

			vkCmdBlitImage(commandBuffer,
						   m_Image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
						   m_Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
						   1, &blit,
						   VK_FILTER_LINEAR);

			barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
			barrier.subresourceRange.baseMipLevel = i;

			vkCmdPipelineBarrier(commandBuffer,
								 VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
								 0, nullptr,
								 0, nullptr,
								 1, &barrier);

			if (mipWidth > 1)
				mipWidth /= 2;
			if (mipHeight > 1)
				mipHeight /= 2;
		}

		// Transition all mips from transfer to shader read
		barrier.subresourceRange.baseMipLevel = 0;
		barrier.subresourceRange.levelCount = mipLevels;
		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		vkCmdPipelineBarrier(commandBuffer,
							 VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
							 0, nullptr,
							 0, nullptr,
							 1, &barrier);

		VulkanContext::GetCurrentDevice()->FlushCommandBuffer(commandBuffer);
#endif
	}

	void VulkanTexture2D::CopyToHostBuffer(Buffer& buffer)
	{
		if (m_Image)
			m_Image.As<VulkanImage2D>()->CopyToHostBuffer(buffer);
	}

	//////////////////////////////////////////////////////////////////////////////////
	// TextureCube
	//////////////////////////////////////////////////////////////////////////////////

	VulkanTextureCube::VulkanTextureCube(const TextureSpecification& specification, Buffer data)
		: m_Specification(specification)
	{
		if (data)
		{
			uint32_t size = m_Specification.Width * m_Specification.Height * 4 * 6; // six layers
			m_LocalStorage = Buffer::Copy(data.Data, size);
		}

		VulkanTextureCube::Invalidate();
		//Ref<VulkanTextureCube> instance = this;
		//Renderer::Submit([instance]() mutable
		//{
		//	instance->Invalidate();
		//});
	}

	void VulkanTextureCube::Release()
	{
		if (m_Image == nullptr)
			return;

		Renderer::SubmitResourceFree([image = m_Image, allocation = m_MemoryAlloc, texInfo = m_DescriptorImageInfo]() mutable
		{
			BEY_CORE_TRACE_TAG("Renderer", "Destroying VulkanTextureCube");
			auto vulkanDevice = VulkanContext::GetCurrentDevice()->GetVulkanDevice();
			vkDestroyImageView(vulkanDevice, texInfo.imageView, nullptr);
			Vulkan::DestroySampler(texInfo.sampler);

			VulkanAllocator allocator("TextureCube");
			allocator.DestroyImage(image, allocation);
		});
		m_Image = nullptr;
		m_MemoryAlloc = nullptr;
		m_DescriptorImageInfo.imageView = nullptr;
		m_DescriptorImageInfo.sampler = nullptr;
	}

	VulkanTextureCube::~VulkanTextureCube()
	{
		Release();
	}

	void VulkanTextureCube::Invalidate()
	{
		auto device = VulkanContext::GetCurrentDevice();
		auto vulkanDevice = device->GetVulkanDevice();

		Release();

		VkFormat format = Utils::VulkanImageFormat(m_Specification.Format);
		uint32_t mipCount = VulkanTextureCube::GetMipLevelCount();

		VkMemoryAllocateInfo memAllocInfo{};
		memAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;

		VulkanAllocator allocator("TextureCube");

		// Create optimal tiled target image on the device
		VkImageCreateInfo imageCreateInfo{};
		imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
		imageCreateInfo.format = format;
		imageCreateInfo.mipLevels = mipCount;
		imageCreateInfo.arrayLayers = 6;
		imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imageCreateInfo.extent = { m_Specification.Width, m_Specification.Height, 1 };
		imageCreateInfo.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
		imageCreateInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
		m_MemoryAlloc = allocator.AllocateImage(imageCreateInfo, VMA_MEMORY_USAGE_GPU_ONLY, m_Image, &m_GPUAllocationSize);
		VKUtils::SetDebugUtilsObjectName(vulkanDevice, VK_OBJECT_TYPE_IMAGE, m_Specification.DebugName.c_str(), m_Image);

		m_DescriptorImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

		// Copy data if present
		if (m_LocalStorage)
		{
			// Create staging buffer
			VkBufferCreateInfo bufferCreateInfo{};
			bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
			bufferCreateInfo.size = m_LocalStorage.Size;
			bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
			bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			VkBuffer stagingBuffer;
			VmaAllocation stagingBufferAllocation = allocator.AllocateBuffer(bufferCreateInfo, VMA_MEMORY_USAGE_CPU_TO_GPU, stagingBuffer);

			// Copy data to staging buffer
			uint8_t* destData = allocator.MapMemory<uint8_t>(stagingBufferAllocation);
			memcpy(destData, m_LocalStorage.Data, m_LocalStorage.Size);
			allocator.UnmapMemory(stagingBufferAllocation);

			VkCommandBuffer copyCmd = device->CreateCommandBuffer(fmt::eastl_format("Copying TextureCube from host buffer, named: {}", m_Specification.DebugName), true);

			// Image memory barriers for the texture image

			// The sub resource range describes the regions of the image that will be transitioned using the memory barriers below
			VkImageSubresourceRange subresourceRange = {};
			// Image only contains color data
			subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			// Start at first mip level
			subresourceRange.baseMipLevel = 0;
			subresourceRange.levelCount = 1;
			subresourceRange.layerCount = 6;

			// Transition the texture image layout to transfer target, so we can safely copy our buffer data to it.
			VkImageMemoryBarrier imageMemoryBarrier{};
			imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			imageMemoryBarrier.image = m_Image;
			imageMemoryBarrier.subresourceRange = subresourceRange;
			imageMemoryBarrier.srcAccessMask = 0;
			imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

			// Insert a memory dependency at the proper pipeline stages that will execute the image layout transition 
			// Source pipeline stage is host write/read exection (VK_PIPELINE_STAGE_HOST_BIT)
			// Destination pipeline stage is copy command exection (VK_PIPELINE_STAGE_TRANSFER_BIT)
			vkCmdPipelineBarrier(
				copyCmd,
				VK_PIPELINE_STAGE_HOST_BIT,
				VK_PIPELINE_STAGE_TRANSFER_BIT,
				0,
				0, nullptr,
				0, nullptr,
				1, &imageMemoryBarrier);

			VkBufferImageCopy bufferCopyRegion = {};
			bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			bufferCopyRegion.imageSubresource.mipLevel = 0;
			bufferCopyRegion.imageSubresource.baseArrayLayer = 0;
			bufferCopyRegion.imageSubresource.layerCount = 6;
			bufferCopyRegion.imageExtent.width = m_Specification.Width;
			bufferCopyRegion.imageExtent.height = m_Specification.Height;
			bufferCopyRegion.imageExtent.depth = 1;
			bufferCopyRegion.bufferOffset = 0;

			// Copy mip levels from staging buffer
			vkCmdCopyBufferToImage(
				copyCmd,
				stagingBuffer,
				m_Image,
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				1,
				&bufferCopyRegion);

			Utils::InsertImageMemoryBarrier(copyCmd, m_Image,
											VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
											VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
											VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
											subresourceRange);

			device->FlushCommandBuffer(copyCmd);

			allocator.DestroyBuffer(stagingBuffer, stagingBufferAllocation);
		}

		VkCommandBuffer layoutCmd = device->CreateCommandBuffer(fmt::eastl_format("Transitioning TextureCube, named: {}", m_Specification.DebugName), true);

		VkImageSubresourceRange subresourceRange = {};
		subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		subresourceRange.baseMipLevel = 0;
		subresourceRange.levelCount = mipCount;
		subresourceRange.layerCount = 6;

		Utils::SetImageLayout(
			layoutCmd, m_Image,
			VK_IMAGE_LAYOUT_UNDEFINED,
			m_DescriptorImageInfo.imageLayout,
			subresourceRange, 
			m_LocalStorage ? VK_PIPELINE_STAGE_TRANSFER_BIT : VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

		device->FlushCommandBuffer(layoutCmd);

		////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		// CREATE TEXTURE SAMPLER
		////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		// Create a texture sampler
		VkSamplerCreateInfo sampler{};
		sampler.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		sampler.magFilter = VulkanSamplerFilter(m_Specification.SamplerFilter);
		sampler.minFilter = VulkanSamplerFilter(m_Specification.SamplerFilter);
		sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		sampler.addressModeU = VulkanSamplerWrap(m_Specification.SamplerWrap);
		sampler.addressModeV = VulkanSamplerWrap(m_Specification.SamplerWrap);
		sampler.addressModeW = VulkanSamplerWrap(m_Specification.SamplerWrap);
		sampler.mipLodBias = 0.0f;
		sampler.compareOp = VK_COMPARE_OP_NEVER;
		sampler.minLod = 0.0f;
		// Set max level-of-detail to mip level count of the texture
		sampler.maxLod = (float)mipCount;
		// Enable anisotropic filtering
		// This feature is optional, so we must check if it's supported on the device

		sampler.maxAnisotropy = 1.0;
		sampler.anisotropyEnable = VK_FALSE;
		sampler.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
		m_DescriptorImageInfo.sampler = Vulkan::CreateSampler(sampler);
		VKUtils::SetDebugUtilsObjectName(device->GetVulkanDevice(), VK_OBJECT_TYPE_SAMPLER, fmt::eastl_format("{} - cube texture default sampler", m_Specification.DebugName), m_DescriptorImageInfo.sampler);

		// Create image view
		// Textures are not directly accessed by the shaders and
		// are abstracted by image views containing additional
		// information and sub resource ranges
		VkImageViewCreateInfo view{};
		view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		view.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
		view.format = format;
		view.components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
		// The subresource range describes the set of mip levels (and array layers) that can be accessed through this image view
		// It's possible to create multiple image views for a single image referring to different (and/or overlapping) ranges of the image
		view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		view.subresourceRange.baseMipLevel = 0;
		view.subresourceRange.baseArrayLayer = 0;
		view.subresourceRange.layerCount = 6;
		view.subresourceRange.levelCount = mipCount;
		view.image = m_Image;
		VK_CHECK_RESULT(vkCreateImageView(vulkanDevice, &view, nullptr, &m_DescriptorImageInfo.imageView));

		VKUtils::SetDebugUtilsObjectName(vulkanDevice, VK_OBJECT_TYPE_IMAGE_VIEW, fmt::eastl_format("Texture cube view: {}", m_Specification.DebugName), m_DescriptorImageInfo.imageView);
	}

	uint32_t VulkanTextureCube::GetMipLevelCount() const
	{
		return Utils::CalculateMipCount(m_Specification.Width, m_Specification.Height);
	}

	std::pair<uint32_t, uint32_t> VulkanTextureCube::GetMipSize(uint32_t mip) const
	{
		uint32_t width = m_Specification.Width;
		uint32_t height = m_Specification.Height;
		while (mip != 0)
		{
			width /= 2;
			height /= 2;
			mip--;
		}

		return { width, height };
	}

	VkImageView VulkanTextureCube::CreateImageViewSingleMip(uint32_t mip)
	{
		// TODO: assert to check mip count

		auto device = VulkanContext::GetCurrentDevice();
		auto vulkanDevice = device->GetVulkanDevice();

		VkFormat format = Utils::VulkanImageFormat(m_Specification.Format);

		VkImageViewCreateInfo view{};
		view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		view.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
		view.format = format;
		view.components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
		view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		view.subresourceRange.baseMipLevel = mip;
		view.subresourceRange.baseArrayLayer = 0;
		view.subresourceRange.layerCount = 6;
		view.subresourceRange.levelCount = 1;
		view.image = m_Image;

		VkImageView result;
		VK_CHECK_RESULT(vkCreateImageView(vulkanDevice, &view, nullptr, &result));
		VKUtils::SetDebugUtilsObjectName(vulkanDevice, VK_OBJECT_TYPE_IMAGE_VIEW, fmt::eastl_format("Texture cube mip: {}", mip), result);

		return result;
	}

	void VulkanTextureCube::GenerateMips(bool readonly)
	{
		auto device = VulkanContext::GetCurrentDevice();
		auto vulkanDevice = device->GetVulkanDevice();

		VkCommandBuffer blitCmd = VulkanContext::GetCurrentDevice()->CreateCommandBuffer(fmt::eastl_format("Generating Mips for TextureCube, named: {}", m_Specification.DebugName), true);

		uint32_t mipLevels = GetMipLevelCount();
		for (uint32_t face = 0; face < 6; face++)
		{
			VkImageSubresourceRange mipSubRange = {};
			mipSubRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			mipSubRange.baseMipLevel = 0;
			mipSubRange.baseArrayLayer = face;
			mipSubRange.levelCount = 1;
			mipSubRange.layerCount = 1;

			// Prepare current mip level as image blit destination
			Utils::InsertImageMemoryBarrier(blitCmd, m_Image,
											0, VK_ACCESS_TRANSFER_READ_BIT,
											VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
											VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
											mipSubRange);
		}

		for (uint32_t i = 1; i < mipLevels; i++)
		{
			for (uint32_t face = 0; face < 6; face++)
			{
				VkImageBlit imageBlit{};

				// Source
				imageBlit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				imageBlit.srcSubresource.layerCount = 1;
				imageBlit.srcSubresource.mipLevel = i - 1;
				imageBlit.srcSubresource.baseArrayLayer = face;
				imageBlit.srcOffsets[1].x = int32_t(m_Specification.Width >> (i - 1));
				imageBlit.srcOffsets[1].y = int32_t(m_Specification.Height >> (i - 1));
				imageBlit.srcOffsets[1].z = 1;

				// Destination
				imageBlit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				imageBlit.dstSubresource.layerCount = 1;
				imageBlit.dstSubresource.mipLevel = i;
				imageBlit.dstSubresource.baseArrayLayer = face;
				imageBlit.dstOffsets[1].x = int32_t(m_Specification.Width >> i);
				imageBlit.dstOffsets[1].y = int32_t(m_Specification.Height >> i);
				imageBlit.dstOffsets[1].z = 1;

				VkImageSubresourceRange mipSubRange = {};
				mipSubRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				mipSubRange.baseMipLevel = i - 1;
				mipSubRange.baseArrayLayer = face;
				mipSubRange.levelCount = 1;
				mipSubRange.layerCount = 1;

				// Prepare src mip level as image blit destination
				Utils::InsertImageMemoryBarrier(blitCmd, m_Image,
												VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
												VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
												VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
												mipSubRange);

				// dst
				mipSubRange.baseMipLevel = i;

				// Prepare current mip level as image blit destination
				Utils::InsertImageMemoryBarrier(blitCmd, m_Image,
												VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
												VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
												VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
												mipSubRange);

				// Blit from previous level
				vkCmdBlitImage(
					blitCmd,
					m_Image,
					VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
					m_Image,
					VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
					1,
					&imageBlit,
					VK_FILTER_LINEAR);

				// Prepare current mip level as image blit source for next level
				Utils::InsertImageMemoryBarrier(blitCmd, m_Image,
												VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
												VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
												VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
												mipSubRange);
			}
		}

		// After the loop, all mip layers are in TRANSFER_SRC layout, so transition all to SHADER_READ
		VkImageSubresourceRange subresourceRange = {};
		subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		subresourceRange.layerCount = 6;
		subresourceRange.levelCount = mipLevels;

		Utils::InsertImageMemoryBarrier(blitCmd, m_Image,
										VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_SHADER_READ_BIT,
										VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, readonly ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_GENERAL,
										VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
										subresourceRange);

		VulkanContext::GetCurrentDevice()->FlushCommandBuffer(blitCmd);

		m_MipsGenerated = true;

		m_DescriptorImageInfo.imageLayout = readonly ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_GENERAL;

	}

#if 0
	void VulkanTextureCube::CopyToHostBuffer(Buffer& buffer)
	{
		auto device = VulkanContext::GetCurrentDevice();
		auto vulkanDevice = device->GetVulkanDevice();
		VulkanAllocator allocator("TextureCube");

		// Create staging buffer
		VkBufferCreateInfo bufferCreateInfo{};
		bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferCreateInfo.size = m_GPUAllocationSize;
		bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
		bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		VkBuffer stagingBuffer;
		VmaAllocation stagingBufferAllocation = allocator.AllocateBuffer(bufferCreateInfo, VMA_MEMORY_USAGE_GPU_TO_CPU, stagingBuffer);

		VkCommandBuffer copyCmd = device->GetCommandBufferAssetThread(true);

		// Image memory barriers for the texture image
		// The sub resource range describes the regions of the image that will be transitioned using the memory barriers below
		VkImageSubresourceRange subresourceRange = {};
		// Image only contains color data
		subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		// Start at first mip level
		subresourceRange.baseMipLevel = 0;
		subresourceRange.levelCount = GetMipLevelCount();
		subresourceRange.layerCount = 6;

		VkImageMemoryBarrier imageMemoryBarrier{};
		imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		imageMemoryBarrier.image = m_Image;
		imageMemoryBarrier.subresourceRange = subresourceRange;
		imageMemoryBarrier.srcAccessMask = 0;
		imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		imageMemoryBarrier.oldLayout = m_DescriptorImageInfo.imageLayout;
		imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

		// Insert a memory dependency at the proper pipeline stages that will execute the image layout transition 
		// Source pipeline stage is host write/read exection (VK_PIPELINE_STAGE_HOST_BIT)
		// Destination pipeline stage is copy command exection (VK_PIPELINE_STAGE_TRANSFER_BIT)
		vkCmdPipelineBarrier(
			copyCmd,
			VK_PIPELINE_STAGE_HOST_BIT,
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			0,
			0, nullptr,
			0, nullptr,
			1, &imageMemoryBarrier);

		VkBufferImageCopy bufferCopyRegion = {};
		bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		bufferCopyRegion.imageSubresource.mipLevel = 0;
		bufferCopyRegion.imageSubresource.baseArrayLayer = 0;
		bufferCopyRegion.imageSubresource.layerCount = 6;
		bufferCopyRegion.imageExtent.width = m_Specification.Width;
		bufferCopyRegion.imageExtent.height = m_Specification.Height;
		bufferCopyRegion.imageExtent.depth = 1;
		bufferCopyRegion.bufferOffset = 0;

		// Copy mip levels from staging buffer
		vkCmdCopyImageToBuffer(
			copyCmd,
			m_Image,
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			stagingBuffer,
			1,
			&bufferCopyRegion);

		Utils::InsertImageMemoryBarrier(copyCmd, m_Image,
										VK_ACCESS_TRANSFER_WRITE_BIT, 0,
										VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, m_DescriptorImageInfo.imageLayout,
										VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
										subresourceRange);

		device->FlushCommandBufferAssetThread(copyCmd);

		// Copy data from staging buffer
		uint8_t* srcData = allocator.MapMemory<uint8_t>(stagingBufferAllocation);
		buffer.Allocate(m_GPUAllocationSize);
		memcpy(buffer.Data, srcData, m_GPUAllocationSize);
		allocator.UnmapMemory(stagingBufferAllocation);

		allocator.DestroyBuffer(stagingBuffer, stagingBufferAllocation);
	}
#endif

	void VulkanTextureCube::CopyToHostBuffer(Buffer& buffer)
	{
		auto device = VulkanContext::GetCurrentDevice();
		auto vulkanDevice = device->GetVulkanDevice();
		VulkanAllocator allocator("TextureCube");

		uint32_t mipCount = GetMipLevelCount();

		uint32_t faces = 6;
		uint32_t bpp = sizeof(float) * 4;
		uint64_t bufferSize = 0;
		uint32_t w = m_Specification.Width, h = m_Specification.Height;

		for (uint32_t i = 0; i < mipCount; i++)
		{
			bufferSize += w * h * bpp * faces;
			w /= 2;
			h /= 2;
		}

		// Create staging buffer
		VkBufferCreateInfo bufferCreateInfo{};
		bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferCreateInfo.size = bufferSize;
		bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
		bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		VkBuffer stagingBuffer;
		VmaAllocation stagingBufferAllocation = allocator.AllocateBuffer(bufferCreateInfo, VMA_MEMORY_USAGE_GPU_TO_CPU, stagingBuffer);

		uint32_t mipWidth = m_Specification.Width, mipHeight = m_Specification.Height;

		VkCommandBuffer copyCmd = device->CreateCommandBuffer(fmt::eastl_format("Copying TextureCube to host buffer, named: {}", m_Specification.DebugName), true);

		VkImageSubresourceRange subresourceRange = {};
		subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		subresourceRange.baseMipLevel = 0;
		subresourceRange.levelCount = mipCount;
		subresourceRange.layerCount = 6;

		Utils::InsertImageMemoryBarrier(copyCmd, m_Image,
										VK_ACCESS_TRANSFER_READ_BIT, 0,
										m_DescriptorImageInfo.imageLayout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
										VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
										subresourceRange);

		uint64_t mipDataOffset = 0;
		for (uint32_t mip = 0; mip < mipCount; mip++)
		{
			VkBufferImageCopy bufferCopyRegion = {};
			bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			bufferCopyRegion.imageSubresource.mipLevel = mip;
			bufferCopyRegion.imageSubresource.baseArrayLayer = 0;
			bufferCopyRegion.imageSubresource.layerCount = 6;
			bufferCopyRegion.imageExtent.width = mipWidth;
			bufferCopyRegion.imageExtent.height = mipHeight;
			bufferCopyRegion.imageExtent.depth = 1;
			bufferCopyRegion.bufferOffset = mipDataOffset;

			vkCmdCopyImageToBuffer(
				copyCmd,
				m_Image,
				VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				stagingBuffer,
				1,
				&bufferCopyRegion);

			uint64_t mipDataSize = mipWidth * mipHeight * sizeof(float) * 4 * 6;
			mipDataOffset += mipDataSize;
			mipWidth /= 2;
			mipHeight /= 2;
		}

		Utils::InsertImageMemoryBarrier(copyCmd, m_Image,
										VK_ACCESS_TRANSFER_READ_BIT, 0,
										VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, m_DescriptorImageInfo.imageLayout,
										VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
										subresourceRange);

		device->FlushCommandBuffer(copyCmd);

		// Copy data from staging buffer
		uint8_t* srcData = allocator.MapMemory<uint8_t>(stagingBufferAllocation);
		buffer.Allocate(bufferSize);
		memcpy(buffer.Data, srcData, bufferSize);
		allocator.UnmapMemory(stagingBufferAllocation);

		allocator.DestroyBuffer(stagingBuffer, stagingBufferAllocation);
	}

	void VulkanTextureCube::CopyFromBuffer(const Buffer& buffer, uint32_t mips)
	{
		// BEY_CORE_VERIFY(buffer.Size == m_GPUAllocationSize);

		auto device = VulkanContext::GetCurrentDevice();
		auto vulkanDevice = device->GetVulkanDevice();
		VulkanAllocator allocator("TextureCube");

		// Create staging buffer
		VkBufferCreateInfo bufferCreateInfo{};
		bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferCreateInfo.size = buffer.Size;
		bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
		bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		VkBuffer stagingBuffer;
		VmaAllocation stagingBufferAllocation = allocator.AllocateBuffer(bufferCreateInfo, VMA_MEMORY_USAGE_CPU_TO_GPU, stagingBuffer);

		// Copy data from staging buffer
		uint8_t* dstData = allocator.MapMemory<uint8_t>(stagingBufferAllocation);
		memcpy(dstData, buffer.Data, buffer.Size);
		allocator.UnmapMemory(stagingBufferAllocation);

		uint32_t mipWidth = m_Specification.Width, mipHeight = m_Specification.Height;

		VkCommandBuffer copyCmd = device->CreateCommandBuffer(fmt::eastl_format("Copying TextureCube from host buffer, named: {}", m_Specification.DebugName), true);
		uint64_t mipDataOffset = 0;
		for (uint32_t mip = 0; mip < mips; mip++)
		{
			// Image memory barriers for the texture image
			// The sub resource range describes the regions of the image that will be transitioned using the memory barriers below
			VkImageSubresourceRange subresourceRange = {};
			// Image only contains color data
			subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			// Start at first mip level
			subresourceRange.baseMipLevel = mip;
			subresourceRange.levelCount = 1;
			subresourceRange.layerCount = 6;

			VkImageMemoryBarrier imageMemoryBarrier{};
			imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			imageMemoryBarrier.image = m_Image;
			imageMemoryBarrier.subresourceRange = subresourceRange;
			imageMemoryBarrier.srcAccessMask = 0;
			imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			imageMemoryBarrier.oldLayout = m_DescriptorImageInfo.imageLayout;
			imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

			// Insert a memory dependency at the proper pipeline stages that will execute the image layout transition 
			// Source pipeline stage is host write/read exection (VK_PIPELINE_STAGE_HOST_BIT)
			// Destination pipeline stage is copy command exection (VK_PIPELINE_STAGE_TRANSFER_BIT)
			vkCmdPipelineBarrier(
				copyCmd,
				VK_PIPELINE_STAGE_HOST_BIT,
				VK_PIPELINE_STAGE_TRANSFER_BIT,
				0,
				0, nullptr,
				0, nullptr,
				1, &imageMemoryBarrier);

			VkBufferImageCopy bufferCopyRegion = {};
			bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			bufferCopyRegion.imageSubresource.mipLevel = mip;
			bufferCopyRegion.imageSubresource.baseArrayLayer = 0;
			bufferCopyRegion.imageSubresource.layerCount = 6;
			bufferCopyRegion.imageExtent.width = mipWidth;
			bufferCopyRegion.imageExtent.height = mipHeight;
			bufferCopyRegion.imageExtent.depth = 1;
			bufferCopyRegion.bufferOffset = mipDataOffset;

			vkCmdCopyBufferToImage(
				copyCmd,
				stagingBuffer,
				m_Image,
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				1,
				&bufferCopyRegion);

			uint64_t mipDataSize = mipWidth * mipHeight * sizeof(float) * 4 * 6;
			mipDataOffset += mipDataSize;

			mipWidth /= 2;
			mipHeight /= 2;

			Utils::InsertImageMemoryBarrier(copyCmd, m_Image,
											VK_ACCESS_TRANSFER_WRITE_BIT, 0,
											VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, m_DescriptorImageInfo.imageLayout,
											VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
											subresourceRange);
		}
		device->FlushCommandBuffer(copyCmd);

		allocator.DestroyBuffer(stagingBuffer, stagingBufferAllocation);
	}

}
