#pragma once
#include <Beyond/Core/Buffer.h>
#include <Beyond/Renderer/Image.h>
#include <Beyond/Renderer/RendererCapabilities.h>
#include <Beyond/Renderer/Shader.h>

#include "RendererUtils.h"
#include "Beyond/Platform/Vulkan/RenderPassInput.h"
#include "rtxgi/ddgi/DDGIVolume.h"
#include "rtxgi/ddgi/gfx/DDGIVolume_VK.h"

namespace Beyond {
	class Environment;
	class RasterPipeline;
	class RenderCommandBuffer;
	class RenderPass;
	class RaytracingPass;
	class ComputePass;
	class Material;
	class Image2D;
	class TextureCube;
	class StaticMesh;
	class Mesh;
	class MaterialTable;
	class VertexBuffer;
	class IndexBuffer;
	class SceneRenderer;


	enum class RendererAPIType
	{
		None,
		Vulkan
	}; 

	enum class PrimitiveType
	{
		None = 0, Triangles, Lines
	};

	class RendererAPI
	{
	public:
		virtual void Init() = 0;
		virtual void InitBindlessDescriptorSetManager() = 0;
		virtual void Shutdown() = 0;

		virtual void BeginFrame() = 0;
		virtual void EndFrame() = 0;

		virtual void InsertGPUPerfMarker(Ref<RenderCommandBuffer> renderCommandBuffer, const eastl::string& label, const glm::vec4& color) = 0;
		virtual void BeginGPUPerfMarker(Ref<RenderCommandBuffer> renderCommandBuffer, const eastl::string& label, const glm::vec4& markerColor) = 0;
		virtual void EndGPUPerfMarker(Ref<RenderCommandBuffer> renderCommandBuffer) = 0;

		virtual void RT_InsertGPUPerfMarker(Ref<RenderCommandBuffer> renderCommandBuffer, const eastl::string& label, const glm::vec4& color) = 0;
		virtual void RT_BeginGPUPerfMarker(Ref<RenderCommandBuffer> renderCommandBuffer, const eastl::string& label, const glm::vec4& markerColor) = 0;
		virtual void RT_EndGPUPerfMarker(Ref<RenderCommandBuffer> renderCommandBuffer) = 0;

		virtual void BeginRenderPass(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<RenderPass> renderPass, bool explicitClear = false) = 0;
		virtual void EndRenderPass(Ref<RenderCommandBuffer> renderCommandBuffer) = 0;

		virtual void SubmitBarrier(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<Image2D> image, const RendererUtils::ImageBarrier& barrier) = 0;

		virtual void BeginComputePass(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<ComputePass> computePass) = 0;
		virtual void EndComputePass(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<ComputePass> computePass) = 0;
		virtual void DispatchCompute(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<ComputePass> computePass, Ref<Material> material, const glm::uvec3& workGroups, Buffer constants = Buffer()) = 0;

		virtual void BeginRaytracingPass(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<RaytracingPass> raytracingPass) = 0;
		virtual void EndRaytracingPass(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<RaytracingPass> raytracingPass) = 0;
		virtual void DispatchRays(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<RaytracingPass> raytracingPass, Ref<Material> material, uint32_t  width, uint32_t height, uint32_t depth) = 0;
		virtual void SetPushConstant(Ref<RaytracingPass> raytracingPass, Buffer pushConstant, ShaderStage stages) = 0;

		virtual void SubmitFullscreenQuad(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<RasterPipeline> pipeline, Ref<Material> material) = 0;
		virtual void SubmitFullscreenQuadWithOverrides(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<RasterPipeline> pipeline, Ref<Material> material, Buffer vertexShaderOverrides, Buffer fragmentShaderOverrides) = 0;

		virtual std::pair<Ref<TextureCube>, Ref<TextureCube>> CreateEnvironmentMap(const std::string& filepath) = 0;
		virtual Ref<TextureCube> CreatePreethamSky(float turbidity, float azimuth, float inclination) = 0;

		virtual void RenderStaticMesh(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<RasterPipeline> pipeline, Ref<StaticMesh> mesh, uint32_t submeshIndex, Ref<MaterialTable> materialTable, uint32_t drawID, uint32_t instanceCount) = 0;
		virtual void RenderSubmeshInstanced(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<RasterPipeline> pipeline, Ref<Mesh> mesh, uint32_t submeshIndex, Ref<MaterialTable> materialTable, uint32_t boneTransformsOffset, uint32_t drawID, uint32_t instanceCount) = 0;
		virtual void RenderMeshWithMaterial(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<RasterPipeline> pipeline, Ref<Mesh> mesh, uint32_t submeshIndex, Ref<Material> material, uint32_t boneTransformsOffset, uint32_t drawID, uint32_t instanceCount, Buffer additionalUniforms = Buffer()) = 0;
		virtual void RenderStaticMeshWithMaterial(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<RasterPipeline> pipeline, Ref<StaticMesh> staticMesh, uint32_t submeshIndex, Ref<Material> material, uint32_t drawID, uint32_t instanceCount, Buffer additionalUniforms = Buffer()) = 0;

		virtual void RenderQuad(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<RasterPipeline> pipeline, Ref<Material> material, const glm::mat4& transform) = 0;
		virtual void RenderGeometry(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<RasterPipeline> pipeline, Ref<Material> material, Ref<VertexBuffer> vertexBuffer, Ref<IndexBuffer> indexBuffer, const glm::mat4& transform, uint32_t indexCount = 0) = 0;

		virtual void ClearImage(Ref<RenderCommandBuffer> commandBuffer, Ref<Image2D> image, const ImageClearValue& clearValue, ImageSubresourceRange subresourceRange) = 0;
		virtual void CopyImage(Ref<RenderCommandBuffer> commandBuffer, Ref<Image2D> sourceImage, Ref<Image2D> destinationImage) = 0;
		virtual void BlitDepthImage(Ref<RenderCommandBuffer> commandBuffer, Ref<Image2D> sourceImage, Ref<Image2D> destinationImage) = 0;
		virtual void CopyBuffer(Ref<RenderCommandBuffer> renderCommandBuffer, void* dest, Ref<StorageBuffer> storageBuffer) = 0;

		virtual void AddBindlessDescriptor(RenderPassInput&& input) = 0;
		virtual void AddBindlessShader(Ref<Shader> shader) = 0;
		virtual void UpdateBindlessDescriptorSet(bool forceRebakeAll) = 0;
		virtual std::vector<uint32_t> GetBindlessSets() = 0;

		virtual Ref<Sampler> GetBilinearSampler() = 0;
		virtual Ref<Sampler> GetPointSampler() = 0;
		virtual Ref<Sampler> GetAnisoSampler() = 0;

		virtual void InitDDGI(Ref<RenderCommandBuffer> commandBuffer, const std::vector<rtxgi::DDGIVolumeDesc>& ddgiVolumeDescs) = 0;
		virtual void SetDDGIStorage(Ref<StorageBuffer> constantsBuffer, Ref<StorageBuffer> resourceIndices) = 0;
		virtual void UpdateDDGIData(Ref<RenderCommandBuffer> commandBuffer) = 0;
		virtual void SetDDGIResources(Ref<StorageBuffer> buffer, Ref<StorageBuffer> indicesBuffer) = 0;
		virtual void SetDDGITextureResources() = 0;
		virtual std::vector<std::unique_ptr<rtxgi::vulkan::DDGIVolume>>& GetDDGIVolumes() = 0;

		virtual RendererCapabilities& GetCapabilities() = 0;

		static RendererAPIType Current() { return s_CurrentRendererAPI; }
		static void SetAPI(RendererAPIType api);
	private:
		inline static RendererAPIType s_CurrentRendererAPI = RendererAPIType::Vulkan;
	};

}
