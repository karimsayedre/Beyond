#pragma once

#include "Beyond/Renderer/Framebuffer.h"

#include "Vulkan.h"

#include "VulkanImage.h"

namespace Beyond {

	class VulkanFramebuffer : public Framebuffer
	{
	public:
		VulkanFramebuffer(const FramebufferSpecification& spec);
		virtual ~VulkanFramebuffer() override;

		virtual void Resize(uint32_t width, uint32_t height, bool forceRecreate = false) override;
		virtual void AddResizeCallback(const std::function<void(Ref<Framebuffer>)>& func) override;

		virtual uint32_t GetWidth() const override { return m_Width; }
		virtual uint32_t GetHeight() const override { return m_Height; }
		
		std::vector<Ref<Image2D>> GetImages() const { return m_AttachmentImages; }
		virtual Ref<Image2D> GetImage(uint32_t attachmentIndex = 0) const override { BEY_CORE_ASSERT(attachmentIndex < m_AttachmentImages.size()); return m_AttachmentImages[attachmentIndex]; }
		virtual Ref<Image2D> GetDepthImage() const override { return m_DepthAttachmentImage; }
		virtual size_t GetColorAttachmentCount() const override { return m_Specification.SwapChainTarget ? 1 : m_AttachmentImages.size(); }
		virtual bool HasDepthAttachment() const override { return (bool)m_DepthAttachmentImage; }
		VkRenderPass GetRenderPass() const { return m_RenderPass; }
		VkFramebuffer GetVulkanFramebuffer() const { return m_Framebuffer; }
		const std::vector<VkClearValue>& GetVulkanClearValues() const { return m_ClearValues; }

		virtual const FramebufferSpecification& GetSpecification() const override { return m_Specification; }

		void Invalidate();
		void RT_Invalidate();
		void Release();

	private:
		FramebufferSpecification m_Specification;
		uint32_t m_Width = 0, m_Height = 0;

		std::vector<Ref<Image2D>> m_AttachmentImages;
		Ref<Image2D> m_DepthAttachmentImage;

		std::vector<VkClearValue> m_ClearValues;

		VkRenderPass m_RenderPass = nullptr;
		VkFramebuffer m_Framebuffer = nullptr;

		std::vector<std::function<void(Ref<Framebuffer>)>> m_ResizeCallbacks;
	};

}
