#pragma once


#include "Beyond/Platform/Vulkan/VulkanContext.h"

#include "VulkanMemoryAllocator/vk_mem_alloc.h"
#include <nvsdk_ngx_vk.h>

namespace Beyond {

	struct VulkanImageInfo
	{
		VkImage Image = nullptr;
		VkImageView ImageView = nullptr;
		VkSampler Sampler = nullptr;
		VmaAllocation MemoryAlloc = nullptr;
	};

	class VulkanImage2D : public Image2D
	{
	public:
		VulkanImage2D(const ImageSpecification& specification);
		virtual ~VulkanImage2D() override;

		virtual void Resize(const glm::uvec2& size)
		{
			Resize(size.x, size.y);
		}
		virtual void Resize(const uint32_t width, const uint32_t height) override
		{
			m_Specification.Width = width;
			m_Specification.Height = height;
			Invalidate();
		}
		virtual void Invalidate() override;
		virtual void Release() override;

		virtual uint32_t GetWidth() const override { return m_Specification.Width; }
		virtual uint32_t GetHeight() const override { return m_Specification.Height; }
		virtual glm::uvec2 GetSize() const override { return { m_Specification.Width, m_Specification.Height }; }

		virtual float GetAspectRatio() const override { return (float)m_Specification.Width / (float)m_Specification.Height; }

		virtual ImageSpecification& GetSpecification() override { return m_Specification; }
		virtual const ImageSpecification& GetSpecification() const override { return m_Specification; }

		void RT_Resize(const uint32_t width, const uint32_t height)
		{
			m_Specification.Width = width;
			m_Specification.Height = height;
			RT_Invalidate();
		}
		void RT_Invalidate();

		virtual void CreatePerLayerImageViews() override;
		void RT_CreatePerLayerImageViews();
		void RT_CreatePerSpecificLayerImageViews(const std::vector<uint32_t>& layerIndices);

		virtual VkImageView GetLayerImageView(uint32_t layer)
		{
			BEY_CORE_ASSERT(layer < m_PerLayerImageViews.size());
			return m_PerLayerImageViews[layer];
		}

		VkImageView GetMipImageView(uint32_t mip);
		VkImageView RT_GetMipImageView(uint32_t mip);

		VulkanImageInfo& GetImageInfo() { return m_Info; }
		const VulkanImageInfo& GetImageInfo() const { return m_Info; }

		//const NVSDK_NGX_ImageViewInfo_VK& GetNVXInfo() const { return m_NVXInfo; }
		NVSDK_NGX_Resource_VK* GetNVXResourceInfo() { return &m_NVXInfo; }
		const NVSDK_NGX_Resource_VK* const GetNVXResourceInfo() const { return &m_NVXInfo; }

		virtual ResourceDescriptorInfo GetDescriptorInfo() const override { return (ResourceDescriptorInfo)&m_DescriptorImageInfo; }
		const VkDescriptorImageInfo& GetVulkanDescriptorInfo() const { return *(VkDescriptorImageInfo*)GetDescriptorInfo(); }

		virtual Buffer GetBuffer() const override { return m_ImageData; }
		virtual Buffer& GetBuffer() override { return m_ImageData; }

		virtual uint64_t GetHash() const override { return (uint64_t)m_Info.Image; }
		virtual uint32_t GetBindlessIndex() const override
		{
			return  m_BindlessIndex;
		}

		virtual uint32_t GetFlaggedBindlessIndex() const override
		{
			return  m_BindlessIndex | (uint32_t)m_Specification.HasTransparency << 31;
		}

		void UpdateDescriptor();

		// Debug
		static const std::map<VkImage, WeakRef<VulkanImage2D>>& GetImageRefs();

		void CopyToHostBuffer(Buffer& buffer) override;
		void SaveImageToFile(const std::filesystem::path& path) override;

	private:
		ImageSpecification m_Specification;
		uint32_t m_BindlessIndex = 0;

		Buffer m_ImageData;

		VulkanImageInfo m_Info;
		NVSDK_NGX_Resource_VK m_NVXInfo;
		VkDeviceSize m_GPUAllocationSize;

		std::vector<VkImageView> m_PerLayerImageViews;
		std::map<uint32_t, VkImageView> m_PerMipImageViews;
		VkDescriptorImageInfo m_DescriptorImageInfo = {};
	};

	class VulkanImageView : public ImageView
	{
	public:
		VulkanImageView(const ImageViewSpecification& specification);
		VulkanImageView(const ImageViewSpecification& specification, const VkDescriptorImageInfo& info, VkImage image);
		~VulkanImageView() override;

		void Invalidate();
		void RT_Invalidate();

		VkImageView GetImageView() const { return m_ImageView; }

		virtual ResourceDescriptorInfo GetDescriptorInfo() const override { return (ResourceDescriptorInfo)&m_DescriptorImageInfo; }
		const VkDescriptorImageInfo& GetVulkanDescriptorInfo() const { return *(VkDescriptorImageInfo*)GetDescriptorInfo(); }
		uint32_t GetBindlessIndex() const override { return m_BindlessIndex; }
		uint32_t GetFlaggedBindlessIndex() const override { return m_BindlessIndex; }

	private:
		ImageViewSpecification m_Specification;
		VkImageView m_ImageView = nullptr;
		VkImage m_Image = nullptr;
		uint32_t m_BindlessIndex = 0;
		VkDescriptorImageInfo m_DescriptorImageInfo = {};
	};

	namespace Utils {

		inline VkFormat VulkanImageFormat(ImageFormat format)
		{
			switch (format)
			{
				case ImageFormat::RED8UN:               return VK_FORMAT_R8_UNORM;
				case ImageFormat::RED8UI:               return VK_FORMAT_R8_UINT;
				case ImageFormat::RED16UI:              return VK_FORMAT_R16_UINT;
				case ImageFormat::RED32UI:              return VK_FORMAT_R32_UINT;
				case ImageFormat::RED16F:				return VK_FORMAT_R16_SFLOAT;
				case ImageFormat::RED32F:				return VK_FORMAT_R32_SFLOAT;
				case ImageFormat::RG8:				    return VK_FORMAT_R8G8_UNORM;
				case ImageFormat::RG16F:				return VK_FORMAT_R16G16_SFLOAT;
				case ImageFormat::RG32F:				return VK_FORMAT_R32G32_SFLOAT;
				case ImageFormat::RGB:					return VK_FORMAT_R8G8B8_UINT; // Unsupported
				case ImageFormat::SRGB:					return VK_FORMAT_R8G8B8A8_SRGB;
				case ImageFormat::RGBA:					return VK_FORMAT_R8G8B8A8_UNORM;
				case ImageFormat::SRGBA:                return VK_FORMAT_R8G8B8A8_SRGB;
				case ImageFormat::RGBA16F:				return VK_FORMAT_R16G16B16A16_SFLOAT;
				case ImageFormat::RGB16F:				return VK_FORMAT_R16G16B16_SFLOAT;
				case ImageFormat::RGB32F:				return VK_FORMAT_R32G32B32_SFLOAT;
				case ImageFormat::RGBA32F:				return VK_FORMAT_R32G32B32A32_SFLOAT;
				case ImageFormat::A2B10R11G11UNorm:		return VK_FORMAT_A2B10G10R10_UNORM_PACK32;
				case ImageFormat::B10G11R11UFLOAT:		return VK_FORMAT_B10G11R11_UFLOAT_PACK32;
				//case ImageFormat::DEPTH32FSTENCIL8UINT: return VK_FORMAT_D32_SFLOAT_S8_UINT;
				case ImageFormat::DEPTH32F:				return VK_FORMAT_D32_SFLOAT;
				case ImageFormat::BC1_RGB_UNORM:		return VK_FORMAT_BC1_RGB_UNORM_BLOCK;
				case ImageFormat::BC1_RGB_SRGB:			return VK_FORMAT_BC1_RGB_SRGB_BLOCK;
				case ImageFormat::BC1_RGBA_UNORM:		return VK_FORMAT_BC1_RGBA_UNORM_BLOCK;
				case ImageFormat::BC1_RGBA_SRGB:		return VK_FORMAT_BC1_RGBA_SRGB_BLOCK;
				case ImageFormat::BC2_UNORM:			return VK_FORMAT_BC2_UNORM_BLOCK;
				case ImageFormat::BC2_SRGB:				return VK_FORMAT_BC2_SRGB_BLOCK;
				case ImageFormat::BC3_UNORM:			return VK_FORMAT_BC3_UNORM_BLOCK;
				case ImageFormat::BC3_SRGB:				return VK_FORMAT_BC3_SRGB_BLOCK;
				case ImageFormat::BC4_UNORM:			return VK_FORMAT_BC4_UNORM_BLOCK;
				case ImageFormat::BC4_SNORM:			return VK_FORMAT_BC4_SNORM_BLOCK;
				case ImageFormat::BC5_UNORM:			return VK_FORMAT_BC5_UNORM_BLOCK;
				case ImageFormat::BC5_SNORM:			return VK_FORMAT_BC5_SNORM_BLOCK;
				case ImageFormat::BC6H_UFLOAT:			return VK_FORMAT_BC6H_UFLOAT_BLOCK;
				case ImageFormat::BC6H_SFLOAT:			return VK_FORMAT_BC6H_SFLOAT_BLOCK;
				case ImageFormat::BC7_UNORM:			return VK_FORMAT_BC7_UNORM_BLOCK;
				case ImageFormat::BC7_SRGB:				return VK_FORMAT_BC7_SRGB_BLOCK;

				//case ImageFormat::DEPTH24STENCIL8:		return VulkanContext::GetCurrentDevice()->GetPhysicalDevice()->GetDepthFormat();
			}
			BEY_CORE_ASSERT(false);
			return VK_FORMAT_UNDEFINED;
		}

	}

}
