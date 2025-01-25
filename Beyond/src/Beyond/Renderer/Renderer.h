#pragma once
#include <unordered_set>
#include <stacktrace>

#include "RendererUtils.h"
#include "GPUStats.h"
#include "Image.h"
#include "Raytracer.h"
#include "RenderCommandQueue.h"
#include "RendererCapabilities.h"
#include "RendererConfig.h"
#include "Beyond/Debug/Profiler.h"
#include "Beyond/Platform/Vulkan/RenderPassInput.h"
#include "rtxgi/ddgi/DDGIVolume.h"
#include "rtxgi/ddgi/gfx/DDGIVolume_VK.h"


namespace Beyond {
	struct BindlessDescriptorSetManager;
	struct DescriptorSetManager;
	class IndexBuffer;
	class MaterialTable;
	class SceneRenderer;
	class RenderThread;
	class VertexBuffer;
	struct Buffer;
	class Mesh;
	class StaticMesh;
	class Shader;
	class Texture2D;
	class TextureCube;
	class Environment;
	class Image2D;
	class Material;
	class RenderPass;
	class RaytracingPass;
	class ComputePass;
	class RaytracingPipeline;
	class RasterPipeline;
	class PipelineCompute;
	class RenderCommandBuffer;
	class RendererContext;
	struct PushConstantRay;
	class ShaderLibrary;

	extern bool s_Validation; // Let's leave this on for now...


	class Renderer
	{
	public:
		typedef void(*RenderCommandFn)(void*);

		static Ref<RendererContext> GetContext();

		static void Init();
		static void LoadDDGIShaders();

		static void Shutdown();

		static RendererCapabilities& GetCapabilities();

		static Ref<ShaderLibrary> GetShaderLibrary();

		template<typename FuncT>
		static void Submit(FuncT&& func
#ifdef SUBMIT_STACK_TRACES
			, std::stacktrace st = std::stacktrace::current()
#endif
		)
		{
			BEY_PROFILE_FUNC();

			auto renderCmd = [](void* ptr) {
				auto pFunc = (FuncT*)ptr;
				// Wanna know where this submission came from?
				// Go down the stack frames to RenderCommandQueue::Execute and look at the variable 'st'.
				(*pFunc)();
				// NOTE: Instead of destroying we could try and enforce all items to be trivally destructible
				// however some items like uniforms which contain std::strings still exist for now
				// static_assert(std::is_trivially_destructible_v<FuncT>, "FuncT must be trivially destructible");
				pFunc->~FuncT();
			};

			auto storageBuffer = GetRenderCommandQueue().Allocate(renderCmd, sizeof(func)
#ifdef SUBMIT_STACK_TRACES
				, std::move(st)
#endif
			);
			new (storageBuffer) FuncT(std::forward<FuncT>(func));
		}

		template<typename FuncT>
		static void SubmitResourceFree(FuncT&& func
#ifdef SUBMIT_STACK_TRACES
			, std::stacktrace st = std::stacktrace::current()
#endif
		)
		{
			auto renderCmd = [](void* ptr) {
				auto pFunc = (FuncT*)ptr;
				// Wanna know where this submission came from?
				// Go down the stack frames to RenderCommandQueue::Execute and look at the variable 'st'.
				(*pFunc)();

				// NOTE: Instead of destroying we could try and enforce all items to be trivally destructible
				// however some items like uniforms which contain std::strings still exist for now
				// static_assert(std::is_trivially_destructible_v<FuncT>, "FuncT must be trivially destructible");
				pFunc->~FuncT();
			};

			Submit([renderCmd, func
#ifdef SUBMIT_STACK_TRACES
				, st = std::move(st)
#endif
			]() mutable
			{
				const uint32_t index = Renderer::RT_GetCurrentFrameIndex();
				auto storageBuffer = GetRenderResourceReleaseQueue(index).Allocate(renderCmd, sizeof(func)
#ifdef SUBMIT_STACK_TRACES
					, std::move(st)
#endif
				);
				new (storageBuffer) FuncT(std::forward<FuncT>((FuncT&&)func));
			});
		}

		/*static void* Submit(RenderCommandFn fn, unsigned int size)
		{
			return s_Instance->m_CommandQueue.Allocate(fn, size);
		}*/

		static void WaitAndRender(RenderThread* renderThread);
		static void SwapQueues();

		static void RenderThreadFunc(RenderThread* renderThread);
		static uint32_t GetRenderQueueIndex();
		static uint32_t GetRenderQueueSubmissionIndex();

		// ~Actual~ Renderer here... TODO: remove confusion later

		// Render Pass API
		static void BeginRenderPass(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<RenderPass> renderPass, bool explicitClear = false);
		static void EndRenderPass(Ref<RenderCommandBuffer> renderCommandBuffer);

		static void SubmitBarrier(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<Image2D> image, const RendererUtils::ImageBarrier& barrier);

		// Compute Pass API
		static void BeginComputePass(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<ComputePass> computePass);
		static void EndComputePass(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<ComputePass> computePass);
		static void DispatchCompute(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<ComputePass> computePass, Ref<Material> material, const glm::uvec3& workGroups, Buffer constants = Buffer());



		// Raytracing Pass API
		static void BeginRaytracingPass(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<RaytracingPass> raytracingPass);
		static void EndRaytracingPass(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<RaytracingPass> raytracingPass);
		static void DispatchRays(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<RaytracingPass> raytracingPass, Ref<Material> material, uint32_t width, uint32_t height, const uint32_t depth);
		static void SetPushConstant(Ref<RaytracingPass> raytracingPass, Buffer pushConstant, ShaderStage stages);

		static void BeginGPUPerfMarker(Ref<RenderCommandBuffer> renderCommandBuffer, const eastl::string& label, const glm::vec4& markerColor = {});
		static void InsertGPUPerfMarker(Ref<RenderCommandBuffer> renderCommandBuffer, const eastl::string& label, const glm::vec4& markerColor = {});
		static void EndGPUPerfMarker(Ref<RenderCommandBuffer> renderCommandBuffer);

		static void RT_BeginGPUPerfMarker(Ref<RenderCommandBuffer> renderCommandBuffer, const eastl::string& label, const glm::vec4& markerColor = {});
		static void RT_InsertGPUPerfMarker(Ref<RenderCommandBuffer> renderCommandBuffer, const eastl::string& label, const glm::vec4& markerColor = {});
		static void RT_EndGPUPerfMarker(Ref<RenderCommandBuffer> renderCommandBuffer);

		static void BeginFrame();
		static void EndFrame();

		static std::pair<Ref<TextureCube>, Ref<TextureCube>> CreateEnvironmentMap(const std::string& filepath);
		static Ref<TextureCube> CreatePreethamSky(float turbidity, float azimuth, float inclination);

		static void RenderStaticMesh(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<RasterPipeline> pipeline, Ref<StaticMesh> mesh, uint32_t submeshIndex, Ref
									 <MaterialTable> materialTable, uint32_t drawID, uint32_t instanceCount);
		static void RenderSubmeshInstanced(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<RasterPipeline> pipeline, Ref<Mesh> mesh, uint32_t submeshIndex, Ref<
										   MaterialTable> materialTable, uint32_t boneTransformsOffset, uint32_t drawID, uint32_t instanceCount);
		static void RenderMeshWithMaterial(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<RasterPipeline> pipeline, Ref<Mesh> mesh, uint32_t submeshIndex, uint32_t
										   boneTransformsOffset, uint32_t drawID, uint32_t instanceCount, Ref<Material> material, Buffer additionalUniforms =
											   Buffer());
		static void RenderStaticMeshWithMaterial(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<RasterPipeline> pipeline, Ref<StaticMesh> mesh, uint32_t submeshIndex, Ref
												 <Material> material, uint32_t
												 drawID, uint32_t instanceCount, Buffer additionalUniforms = Buffer());
		static void RenderQuad(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<RasterPipeline> pipeline, Ref<Material> material, const glm::mat4& transform);
		static void SubmitFullscreenQuad(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<RasterPipeline> pipeline, Ref<Material> material);
		static void SubmitFullscreenQuadWithOverrides(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<RasterPipeline> pipeline, Ref<Material> material, Buffer vertexShaderOverrides, Buffer fragmentShaderOverrides);
		static void RenderGeometry(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<RasterPipeline> pipeline, Ref<Material> material, Ref<VertexBuffer> vertexBuffer, Ref<IndexBuffer> indexBuffer, const glm::mat4& transform, uint32_t indexCount = 0);
		static void SubmitQuad(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<Material> material, const glm::mat4& transform = glm::mat4(1.0f));

		static void ClearImage(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<Image2D> image, const ImageClearValue& clearValue, ImageSubresourceRange subresourceRange = ImageSubresourceRange());
		static void CopyImage(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<Image2D> destinationImage, Ref<Image2D> sourceImage);
		static void BlitDepthImage(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<Image2D> sourceImage, Ref<Image2D> destinationImage);
		static void CopyBuffer(Ref<RenderCommandBuffer> renderCommandBuffer, void* dest, Ref<StorageBuffer> storageBuffer);


		static void AddBindlessDescriptor(RenderPassInput&& input);
		static void UpdateBindlessDescriptorSet(bool forceRebakeAll = true);
		static void AddBindlessShader(Ref<Shader> shader);

		// DDGI
		static void UpdateDDGIVolumes(Ref<RenderCommandBuffer> commandBuffer);
		static void SetDDGITextureResources();
		static void SetDDGIResources(Ref<StorageBuffer> constantsBuffer, Ref<StorageBuffer> indicesBuffer);
		static void InitDDGI(Ref<RenderCommandBuffer> commandBuffer, const std::vector<rtxgi::DDGIVolumeDesc>& ddgiVolumeDescs);
		static void SetDDGIStorage(Ref<StorageBuffer> constantsBuffer, Ref<StorageBuffer> resourceIndices);


		static Ref<StorageBuffer> GetDefaultStorageBuffer();
		static Ref<UniformBuffer> GetDefaultUniformBuffer();
		static Ref<Texture2D> GetMissingTexture();
		static Ref<Texture2D> GetWhiteTexture();
		static Ref<Texture2D> GetWhiteArrayTexture();
		static Ref<Texture2D> GetBlackTexture();
		static Ref<Texture2D> GetHilbertLut();
		static Ref<Texture2D> GetBRDFLutTexture();
		static Ref<TextureCube> GetBlackCubeTexture();
		static Ref<Environment> GetEmptyEnvironment();
		static Ref<Sampler> GetBilinearSampler();
		static Ref<Sampler> GetPointSampler();
		static Ref<Sampler> GetAnisoSampler();
		static std::vector<uint32_t> GetBindlessSets();
		static std::vector<std::unique_ptr<rtxgi::vulkan::DDGIVolume>>& GetDDGIVolumes();

		static void RegisterShaderDependency(Ref<Shader> shader, Ref<PipelineCompute> computePipeline);
		static void RegisterShaderDependency(Ref<Shader> shader, Ref<RasterPipeline> pipeline);
		static void RegisterShaderDependency(Ref<Shader> shader, Ref<RaytracingPipeline> raytracingPipeline);
		static void RegisterShaderDependency(Ref<Shader> shader, Ref<Material> material);
		static void RegisterShaderDependency(Ref<Shader> shader, Ref<BindlessDescriptorSetManager> descriptorSetManager);
		static void RegisterShaderDependency(Ref<Shader> shader, Ref<DescriptorSetManager> descriptorSetManager);
		static void UnRegisterShaderDependency(Ref<Shader> shader, Ref<Material> material);
		static void OnShaderReloaded(size_t hash);

		static uint32_t GetCurrentFrameIndex();
		static uint32_t RT_GetCurrentFrameIndex();

		static RendererConfig& GetConfig();
		static void SetConfig(const RendererConfig& config);

		static RenderCommandQueue& GetRenderResourceReleaseQueue(uint32_t index);

		// Add known macro from shader.
		static const std::unordered_map<std::string, std::string>& GetGlobalShaderMacros();
		static void AcknowledgeParsedGlobalMacros(const std::unordered_set<std::string>& macros, Ref<Shader> shader);
		static void SetMacroInShader(Ref<Shader> shader, const std::string& name, const std::string& value = "");
		static void SetGlobalMacroInShaders(const std::string& name, const std::string& value = "");
		// Returns true if any shader is actually updated.
		static bool UpdateDirtyShaders();

		static GPUMemoryStats GetGPUMemoryStats();
		static bool UpdatedShaders();
		static void NotifyShaderUpdate();

	private:
		static RenderCommandQueue& GetRenderCommandQueue();
	};

	namespace Utils {

		inline void DumpGPUInfo()
		{
			auto& caps = Renderer::GetCapabilities();
			BEY_CORE_TRACE_TAG("Renderer", "GPU Info:");
			BEY_CORE_TRACE_TAG("Renderer", "  Vendor: {0}", caps.Vendor);
			BEY_CORE_TRACE_TAG("Renderer", "  Device: {0}", caps.Device);
			BEY_CORE_TRACE_TAG("Renderer", "  Version: {0}", caps.Version);
		}

	}

}
