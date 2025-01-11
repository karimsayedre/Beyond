#pragma once
#include <vulkan/vulkan.h>

#include "VulkanImage.h"
#include "VulkanSampler.h"
#include "Beyond/Renderer/RendererAPI.h"
#include "rtxgi/VulkanExtensions.h"
#include "rtxgi/ddgi/gfx/DDGIVolume_VK.h"

namespace Beyond {

	class VulkanRenderer : public RendererAPI
	{
	public:
		virtual void Init() override;
		virtual void InitBindlessDescriptorSetManager() override;
		virtual void Shutdown() override;

		virtual RendererCapabilities& GetCapabilities() override;


		virtual void BeginFrame() override;
		virtual void EndFrame() override;

		virtual void InsertGPUPerfMarker(Ref<RenderCommandBuffer> renderCommandBuffer, const eastl::string& label, const glm::vec4& markerColor = {}) override;
		virtual void BeginGPUPerfMarker(Ref<RenderCommandBuffer> renderCommandBuffer, const eastl::string& label, const glm::vec4& markerColor = {}) override;
		virtual void EndGPUPerfMarker(Ref<RenderCommandBuffer> renderCommandBuffer) override;

		virtual void RT_InsertGPUPerfMarker(Ref<RenderCommandBuffer> renderCommandBuffer, const eastl::string& label, const glm::vec4& markerColor = {}) override;
		virtual void RT_BeginGPUPerfMarker(Ref<RenderCommandBuffer> renderCommandBuffer, const eastl::string& label, const glm::vec4& markerColor = {}) override;
		virtual void RT_EndGPUPerfMarker(Ref<RenderCommandBuffer> renderCommandBuffer) override;

		virtual void BeginRenderPass(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<RenderPass> renderPass, bool explicitClear = false) override;
		virtual void EndRenderPass(Ref<RenderCommandBuffer> renderCommandBuffer) override;

		virtual void SubmitBarrier(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<Image2D> image, const RendererUtils::ImageBarrier& barrier);

		virtual void BeginComputePass(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<ComputePass> computePass) override;
		virtual void EndComputePass(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<ComputePass> computePass) override;
		virtual void DispatchCompute(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<ComputePass> computePass, Ref<Material> material, const glm::uvec3& workGroups, Buffer constants) override;

		virtual std::vector<std::unique_ptr<rtxgi::vulkan::DDGIVolume>>& GetDDGIVolumes() override;
		virtual void InitDDGI(Ref<RenderCommandBuffer> commandBuffer, const std::vector<rtxgi::DDGIVolumeDesc>& ddgiVolumeDescs) override;
		virtual void SetDDGIStorage(Ref<StorageBuffer> constantsBuffer, Ref<StorageBuffer> resourceIndices) override;
		virtual void UpdateDDGIData(Ref<RenderCommandBuffer> commandBuffer) override;
		virtual void SetDDGIResources(Ref<StorageBuffer> constantBuffer, Ref<StorageBuffer> indicesBuffer) override;
		virtual void SetDDGITextureResources() override;

		virtual void BeginRaytracingPass(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<RaytracingPass> raytracingPass) override;
		virtual void EndRaytracingPass(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<RaytracingPass> raytracingPass) override;
		virtual void DispatchRays(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<RaytracingPass> raytracingPass, Ref<Material> material, uint32_t
								  width, uint32_t height, uint32_t depth) override;
		virtual void SetPushConstant(Ref<RaytracingPass> raytracingPass, Buffer pushConstant, ShaderStage stages) override;

		virtual void SubmitFullscreenQuad(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<RasterPipeline> pipeline, Ref<Material> material) override;
		virtual void SubmitFullscreenQuadWithOverrides(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<RasterPipeline> pipeline, Ref<Material> material, Buffer vertexShaderOverrides, Buffer fragmentShaderOverrides) override;

		virtual std::pair<Ref<TextureCube>, Ref<TextureCube>> CreateEnvironmentMap(const std::string& filepath) override;
		virtual Ref<TextureCube> CreatePreethamSky(float turbidity, float azimuth, float inclination) override;

		virtual void RenderStaticMesh(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<RasterPipeline> pipeline, Ref<StaticMesh> mesh, uint32_t submeshIndex, Ref<MaterialTable> materialTable, uint32_t drawID, uint32_t instanceCount) override;
		virtual void RenderSubmeshInstanced(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<RasterPipeline> pipeline, Ref<Mesh> mesh, uint32_t submeshIndex, Ref<MaterialTable> materialTable, uint32_t drawID, uint32_t boneTransformsOffset, uint32_t instanceCount) override;
		virtual void RenderMeshWithMaterial(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<RasterPipeline> pipeline, Ref<Mesh> mesh, uint32_t submeshIndex, Ref<Material> material, uint32_t boneTransformsOffset, uint32_t drawID, uint32_t instanceCount, Buffer additionalUniforms = Buffer()) override;
		virtual void RenderStaticMeshWithMaterial(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<RasterPipeline> pipeline, Ref<StaticMesh> mesh, uint32_t submeshIndex, Ref<Material> material, uint32_t drawID, uint32_t instanceCount, Buffer additionalUniforms = Buffer()) override;
		virtual void RenderQuad(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<RasterPipeline> pipeline, Ref<Material> material, const glm::mat4& transform) override;
		virtual void RenderGeometry(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<RasterPipeline> pipeline, Ref<Material> material, Ref<VertexBuffer> vertexBuffer, Ref<IndexBuffer> indexBuffer, const glm::mat4& transform, uint32_t indexCount = 0) override;
		virtual void ClearImage(Ref<RenderCommandBuffer> commandBuffer, Ref<Image2D> image, const ImageClearValue& clearValue, ImageSubresourceRange subresourceRange) override;
		virtual void CopyImage(Ref<RenderCommandBuffer> commandBuffer, Ref<Image2D> sourceImage, Ref<Image2D> destinationImage) override;
		virtual void BlitDepthImage(Ref<RenderCommandBuffer> commandBuffer, Ref<Image2D> sourceImage, Ref<Image2D> destinationImage) override;
		virtual void CopyBuffer(Ref<RenderCommandBuffer> renderCommandBuffer, void* dest, Ref<StorageBuffer> storageBuffer)  override;

		virtual void AddBindlessDescriptor(RenderPassInput&& input) override;
		virtual void UpdateBindlessDescriptorSet(bool forceRebakeAll) override;
		virtual void AddBindlessShader(Ref<Shader> shader) override;



		virtual Ref<Sampler> GetBilinearSampler() override;
		virtual Ref<Sampler> GetPointSampler() override;
		virtual Ref<Sampler> GetAnisoSampler() override;
		virtual std::vector<uint32_t> GetBindlessSets() override;

		static uint32_t GetDescriptorAllocationCount(uint32_t frameIndex = 0);

		static int32_t& GetSelectedDrawCall();
	public:
		static VkDescriptorSet RT_AllocateDescriptorSet(VkDescriptorSetAllocateInfo& allocInfo);
		static VkDescriptorSet AllocateMaterialDescriptorSet(VkDescriptorSetAllocateInfo& allocInfo, uint32_t set, uint32_t frame, std::string_view shaderName);
	};

	namespace Utils {

		void InsertImageMemoryBarrier(
			VkCommandBuffer cmdbuffer,
			VkImage image,
			VkAccessFlags srcAccessMask,
			VkAccessFlags dstAccessMask,
			VkImageLayout oldImageLayout,
			VkImageLayout newImageLayout,
			VkPipelineStageFlags srcStageMask,
			VkPipelineStageFlags dstStageMask,
			VkImageSubresourceRange subresourceRange);

		void InsertBufferMemoryBarrier(
		VkCommandBuffer cmdbuffer,
		VkBuffer buffer,
		VkAccessFlags srcAccessMask,
		VkAccessFlags dstAccessMask,
		VkPipelineStageFlags srcStageMask,
		VkPipelineStageFlags dstStageMask,
		uint32_t size,
		uint32_t offset);

		void SetImageLayout(
			VkCommandBuffer cmdbuffer,
			VkImage image,
			VkImageLayout oldImageLayout,
			VkImageLayout newImageLayout,
			VkImageSubresourceRange subresourceRange,
			VkPipelineStageFlags srcStageMask,
			VkPipelineStageFlags dstStageMask);

		void SetImageLayout(
			VkCommandBuffer cmdbuffer,
			VkImage image,
			VkImageAspectFlags aspectMask,
			VkImageLayout oldImageLayout,
			VkImageLayout newImageLayout,
			VkPipelineStageFlags srcStageMask,
			VkPipelineStageFlags dstStageMask);

	}

}
