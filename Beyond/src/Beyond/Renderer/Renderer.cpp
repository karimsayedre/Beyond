#include "pch.h"
#include "Renderer.h"

#include "Shader.h"

#include <map>

#include "RendererAPI.h"
#include "Renderer2D.h"
#include "SceneRenderer.h"
#include "ShaderPack.h"

#include "Beyond/Core/Timer.h"
#include "Beyond/Debug/Profiler.h"

#include "Beyond/Platform/Vulkan/VulkanContext.h"

#include "Beyond/Project/Project.h"

#include <filesystem>
#include "Beyond/Core/Application.h"
#include "Beyond/Platform/Vulkan/VulkanComputePipeline.h"
#include "Beyond/Platform/Vulkan/VulkanRaytracingPipeline.h"
#include "Beyond/Platform/Vulkan/VulkanRenderer.h"
#include "Beyond/Platform/Vulkan/DescriptorSetManager.h"
#include "Beyond/Platform/Vulkan/BindlessDescriptorSetManager.h"
#include "Beyond/Platform/Vulkan/VulkanRasterPipeline.h"

namespace std {
	template<>
	struct hash<Beyond::WeakRef<Beyond::Shader>>
	{
		size_t operator()(const Beyond::WeakRef<Beyond::Shader>& shader) const noexcept
		{
			return shader->GetHash();
		}
	};
}

namespace Beyond {

	static RendererAPI* s_RendererAPI = nullptr;

	struct ShaderDependencies
	{
		std::vector<Ref<PipelineCompute>> ComputePipelines;
		std::vector<Ref<RasterPipeline>> RasterPipelines;
		std::vector<Ref<RaytracingPipeline>> RaytracingPipelines;
		std::vector<Ref<Material>> Materials;
		std::vector<Ref<BindlessDescriptorSetManager>> BindlessDescriptorSetManagers;
		std::vector<Ref<DescriptorSetManager>> DescriptorSetManagers;
	};
	static std::unordered_map<size_t, ShaderDependencies> s_ShaderDependencies;

	struct GlobalShaderInfo
	{
		// Macro name, set of shaders with that macro.
		std::unordered_map<std::string, std::unordered_map<size_t, WeakRef<Shader>>> ShaderGlobalMacrosMap;
		// Shaders waiting to be reloaded.
		std::unordered_set<WeakRef<Shader>> DirtyShaders;
	};
	static GlobalShaderInfo s_GlobalShaderInfo;

	void Renderer::RegisterShaderDependency(Ref<Shader> shader, Ref<PipelineCompute> computePipeline)
	{
		s_ShaderDependencies[shader->GetHash()].ComputePipelines.push_back(computePipeline);
	}

	void Renderer::RegisterShaderDependency(Ref<Shader> shader, Ref<RasterPipeline> pipeline)
	{
		s_ShaderDependencies[shader->GetHash()].RasterPipelines.push_back(pipeline);
	}

	void Renderer::RegisterShaderDependency(Ref<Shader> shader, Ref<RaytracingPipeline> raytracingPipeline)
	{
		s_ShaderDependencies[shader->GetHash()].RaytracingPipelines.push_back(raytracingPipeline);
	}

	void Renderer::RegisterShaderDependency(Ref<Shader> shader, Ref<Material> material)
	{
		s_ShaderDependencies[shader->GetHash()].Materials.push_back(material);
	}

	void Renderer::RegisterShaderDependency(Ref<Shader> shader, Ref<BindlessDescriptorSetManager> descriptorSetManager)
	{
		auto& managers = s_ShaderDependencies[shader->GetHash()].BindlessDescriptorSetManagers;

		// Check if the descriptorSetManager is already in the vector
		if (std::ranges::find(managers, descriptorSetManager) == managers.end())
		{
			managers.push_back(descriptorSetManager);
		}
	}

	void Renderer::RegisterShaderDependency(Ref<Shader> shader, Ref<DescriptorSetManager> descriptorSetManager)
	{
		s_ShaderDependencies[shader->GetHash()].DescriptorSetManagers.push_back(descriptorSetManager);
	}

	void Renderer::UnRegisterShaderDependency(Ref<Shader> shader, Ref<Material> material)
	{
		std::erase(s_ShaderDependencies[shader->GetHash()].Materials, material);
	}

	void Renderer::OnShaderReloaded(size_t hash)
	{
		if (s_ShaderDependencies.contains(hash))
		{
			auto& dependencies = s_ShaderDependencies.at(hash);
			for (auto& bindlessDescriptorSetManager : dependencies.BindlessDescriptorSetManagers)
			{
				bindlessDescriptorSetManager->OnShaderReloaded();
			}

			for (auto& descriptorSetManager : dependencies.DescriptorSetManagers)
			{
				descriptorSetManager->OnShaderReloaded();
			}

			for (auto& pipeline : dependencies.RasterPipelines)
			{
				pipeline.As<VulkanRasterPipeline>()->Invalidate();
			}

			for (auto& pipeline : dependencies.RaytracingPipelines)
			{
				pipeline.As<VulkanRaytracingPipeline>()->RT_CreatePipeline();
			}

			for (auto& computePipeline : dependencies.ComputePipelines)
			{
				computePipeline.As<VulkanComputePipeline>()->RT_CreatePipeline();
			}

			for (auto& material : dependencies.Materials)
			{
				material->OnShaderReloaded();
			}

		}
	}

	uint32_t Renderer::RT_GetCurrentFrameIndex()
	{
		// Swapchain owns the Render Thread frame index
		return Application::Get().GetWindow().GetSwapChain().GetCurrentBufferIndex();
	}

	uint32_t Renderer::GetCurrentFrameIndex()
	{
		return Application::Get().GetCurrentFrameIndex();
	}

	void RendererAPI::SetAPI(RendererAPIType api)
	{
		// TODO: make sure this is called at a valid time
		BEY_CORE_VERIFY(api == RendererAPIType::Vulkan, "Vulkan is currently the only supported Renderer API");
		s_CurrentRendererAPI = api;
	}

	struct RendererData
	{
		Ref<ShaderLibrary> m_ShaderLibrary;

		Ref<Texture2D> MissingTexture;
		Ref<Texture2D> WhiteTexture;
		Ref<Texture2D> WhiteTextureArray;
		Ref<Texture2D> BlackTexture;
		Ref<Texture2D> BRDFLutTexture;
		Ref<UniformBuffer> DefaultUniformBuffer;
		Ref<StorageBuffer> DefaultStorageBuffer;
		Ref<Texture2D> HilbertLut;
		Ref<TextureCube> BlackCubeTexture;
		Ref<Environment> EmptyEnvironment;

		std::unordered_map<std::string, std::string> GlobalShaderMacros;
		std::atomic_bool HasUpdatedShaders = false;
	};

	static RendererConfig s_Config;
	static RendererData* s_Data = nullptr;
	constexpr static uint32_t s_RenderCommandQueueCount = 2;
	static RenderCommandQueue* s_CommandQueue[s_RenderCommandQueueCount];
	static std::atomic<uint32_t> s_RenderCommandQueueSubmissionIndex = 0;
	static RenderCommandQueue s_ResourceFreeQueue[3];

	static RendererAPI* InitRendererAPI()
	{
		switch (RendererAPI::Current())
		{
			case RendererAPIType::Vulkan: return hnew VulkanRenderer();
		}
		BEY_CORE_ASSERT(false, "Unknown RendererAPI");
		return nullptr;
	}

	Ref<RendererContext> Renderer::GetContext()
	{
		return Application::Get().GetWindow().GetRenderContext();
	}

	void Renderer::LoadDDGIShaders()
	{


		std::vector<std::pair<std::wstring, std::wstring>> defines{
			{ L"HLSL", L"1" },
			{ L"__spirv__", L"1" },

			/*{ L"VOLUME_CONSTS_REGISTER", L"0" },
			{ L"VOLUME_CONSTS_SPACE ", L"4" },
			{ L"RAY_DATA_REGISTER  ", L"1" },
			{ L"RAY_DATA_SPACE", L"4" },
			{ L"OUTPUT_REGISTER", L"2" },
			{ L"OUTPUT_SPACE", L"4" },
			{ L"PROBE_DATA_REGISTER", L"3" },
			{ L"PROBE_DATA_SPACE ", L"4" },
			{ L"PROBE_VARIABILITY_REGISTER", L"4" },
			{ L"PROBE_VARIABILITY_SPACE ", L"4" },
			{ L"PROBE_VARIABILITY_AVERAGE_REGISTER ", L"5" },*/

			//{ L"RTXGI_PUSH_CONSTS_TYPE", L"2" },
			{ L"RTXGI_DDGI_USE_SHADER_CONFIG_FILE", L"0" },
			{ L"RTXGI_DDGI_BINDLESS_RESOURCES", L"0" },
			{ L"RTXGI_BINDLESS_TYPE", L"0" },


			{ L"RTXGI_DDGI_RESOURCE_MANAGEMENT", std::to_wstring(RTXGI_DDGI_RESOURCE_MANAGEMENT).c_str() },
			{ L"RTXGI_DDGI_SHADER_REFLECTION", L"0" },
			{ L"RTXGI_DDGI_DEBUG_PROBE_INDEXING", L"0" },
			{ L"RTXGI_DDGI_DEBUG_OCTAHEDRAL_INDEXING", L"0" },
			{ L"RTXGI_COORDINATE_SYSTEM", L"2" },
			{ L"RTXGI_DDGI_WAVE_LANE_COUNT", L"32" },
			{ L"RTXGI_PUSH_CONSTS_TYPE", L"1" },
		};
		std::vector<std::pair<std::wstring, std::wstring>> probeShaderDefines(defines);
		/*probeShaderDefines.emplace_back(L"RTXGI_PUSH_CONSTS_STRUCT_NAME", L"GlobalConstants");
		probeShaderDefines.emplace_back(L"RTXGI_PUSH_CONSTS_VARIABLE_NAME", L"GlobalConst");
		probeShaderDefines.emplace_back(L"RTXGI_PUSH_CONSTS_FIELD_DDGI_VOLUME_INDEX_NAME", L"ddgi_volumeIndex");
		probeShaderDefines.emplace_back(L"RTXGI_PUSH_CONSTS_FIELD_DDGI_REDUCTION_INPUT_SIZE_X_NAME", L"ddgi_reductionInputSizeX");
		probeShaderDefines.emplace_back(L"RTXGI_PUSH_CONSTS_FIELD_DDGI_REDUCTION_INPUT_SIZE_Y_NAME", L"ddgi_reductionInputSizeY");
		probeShaderDefines.emplace_back(L"RTXGI_PUSH_CONSTS_FIELD_DDGI_REDUCTION_INPUT_SIZE_Z_NAME", L"ddgi_reductionInputSizeZ");*/
		Renderer::GetShaderLibrary()->Load(RootSignature::DDGI, "Resources/Shaders/DDGIIrradiance.hlsl", false, false, true, L"main", L"cs_6_6", probeShaderDefines);
		Renderer::GetShaderLibrary()->Load(RootSignature::DDGI, "Resources/Shaders/DDGITexVis.hlsl", false, false, true, L"main", L"cs_6_6", probeShaderDefines);



		Renderer::GetShaderLibrary()->Load(RootSignature::DDGICompute, "Resources/Shaders/DDGIVis.hlsl", false, false, false, L"main", L"lib_6_3", defines);
		Renderer::GetShaderLibrary()->Load(RootSignature::DDGI, "Resources/Shaders/DDGIProbeUpdate.hlsl", false, false, false, L"main", L"lib_6_3", defines);

		Renderer::GetShaderLibrary()->Load(RootSignature::DDGIRaytrace, "Resources/Shaders/DDGIRaytrace.hlsl", false, false, false, L"main", L"lib_6_3", probeShaderDefines);
		{
			std::vector<std::pair<std::wstring, std::wstring>> probeShaderDefines(defines);
			probeShaderDefines.emplace_back(L"RTXGI_DDGI_BLEND_RADIANCE", L"1");
			probeShaderDefines.emplace_back(L"RTXGI_DDGI_PROBE_NUM_TEXELS", L"8");
			probeShaderDefines.emplace_back(L"RTXGI_DDGI_PROBE_NUM_INTERIOR_TEXELS", L"6");
			probeShaderDefines.emplace_back(L"RTXGI_DDGI_BLEND_SHARED_MEMORY", L"1");
			probeShaderDefines.emplace_back(L"RTXGI_DDGI_BLEND_RAYS_PER_PROBE", L"256");
			probeShaderDefines.emplace_back(L"RTXGI_DDGI_BLEND_SCROLL_SHARED_MEMORY", L"false");

			Renderer::GetShaderLibrary()->Load(RootSignature::DDGI, "Resources/Shaders/RTXGI/ddgi/ProbeBlendingIrradianceCS.hlsl", false, false, true, L"DDGIProbeBlendingCS", L"cs_6_6", probeShaderDefines);
		}

		{
			std::vector<std::pair<std::wstring, std::wstring>> probeShaderDefines(defines);
			probeShaderDefines.emplace_back(L"RTXGI_DDGI_BLEND_RADIANCE", L"0");
			probeShaderDefines.emplace_back(L"RTXGI_DDGI_PROBE_NUM_TEXELS", L"16");
			probeShaderDefines.emplace_back(L"RTXGI_DDGI_PROBE_NUM_INTERIOR_TEXELS", L"14");
			probeShaderDefines.emplace_back(L"RTXGI_DDGI_BLEND_SHARED_MEMORY", L"1");
			probeShaderDefines.emplace_back(L"RTXGI_DDGI_BLEND_RAYS_PER_PROBE", L"256");
			probeShaderDefines.emplace_back(L"RTXGI_DDGI_BLEND_SCROLL_SHARED_MEMORY", L"false");
			Renderer::GetShaderLibrary()->Load(RootSignature::DDGI, "Resources/Shaders/RTXGI/ddgi/ProbeBlendingDistanceCS.hlsl", false, false, true, L"DDGIProbeBlendingCS", L"cs_6_6", probeShaderDefines);
		}
		Renderer::GetShaderLibrary()->Load(RootSignature::DDGI, "Resources/Shaders/RTXGI/ddgi/ProbeRelocationCS.hlsl", false, false, true, L"DDGIProbeRelocationCS", L"cs_6_6", defines);
		Renderer::GetShaderLibrary()->Load(RootSignature::DDGI, "Resources/Shaders/RTXGI/ddgi/ProbeRelocationCS.hlsl", false, false, true, L"DDGIProbeRelocationResetCS", L"cs_6_6", defines);
		Renderer::GetShaderLibrary()->Load(RootSignature::DDGI, "Resources/Shaders/RTXGI/ddgi/ProbeClassificationCS.hlsl", false, false, true, L"DDGIProbeClassificationCS", L"cs_6_6", defines);
		Renderer::GetShaderLibrary()->Load(RootSignature::DDGI, "Resources/Shaders/RTXGI/ddgi/ProbeClassificationCS.hlsl", false, false, true, L"DDGIProbeClassificationResetCS", L"cs_6_6", defines);

		{
			std::vector<std::pair<std::wstring, std::wstring>> probeShaderDefines(defines);
			probeShaderDefines.emplace_back(L"RTXGI_DDGI_PROBE_NUM_INTERIOR_TEXELS", L"6");
			Renderer::GetShaderLibrary()->Load(RootSignature::DDGI, "Resources/Shaders/RTXGI/ddgi/ReductionCS.hlsl", false, false, true, L"DDGIReductionCS", L"cs_6_6", probeShaderDefines);
		}
		{
			std::vector<std::pair<std::wstring, std::wstring>> probeShaderDefines(defines);
			probeShaderDefines.emplace_back(L"RTXGI_DDGI_PROBE_NUM_INTERIOR_TEXELS", L"6");
			Renderer::GetShaderLibrary()->Load(RootSignature::DDGI, "Resources/Shaders/RTXGI/ddgi/ReductionCS.hlsl", false, false, true, L"DDGIExtraReductionCS", L"cs_6_6", probeShaderDefines);
		}
	}

	void Renderer::Init()
	{
		s_Data = hnew RendererData();
		s_CommandQueue[0] = hnew RenderCommandQueue();
		s_CommandQueue[1] = hnew RenderCommandQueue();

		// Make sure we don't have more frames in flight than swapchain images
		s_Config.FramesInFlight = glm::min<uint32_t>(s_Config.FramesInFlight, Application::Get().GetWindow().GetSwapChain().GetImageCount());

		s_RendererAPI = InitRendererAPI();
		s_RendererAPI->InitBindlessDescriptorSetManager();
		s_Data->m_ShaderLibrary = Ref<ShaderLibrary>::Create();

		//s_Config.ShaderPackPath = "Resources/ShaderPack.hsp";

		if (!s_Config.ShaderPackPath.empty())
			Renderer::GetShaderLibrary()->LoadShaderPack(s_Config.ShaderPackPath);

		// Ray tracing
		if (VulkanContext::GetCurrentDevice()->IsRaytracingSupported())
		{
			Renderer::GetShaderLibrary()->Load(RootSignature::ComputeHLSL, "Resources/Shaders/Path-Restir-comp.hlsl");
			Renderer::GetShaderLibrary()->Load(RootSignature::RaytracingHLSL, "Resources/Shaders/Pathtracing.hlsl");
			Renderer::GetShaderLibrary()->Load(RootSignature::RaytracingHLSL, "Resources/Shaders/Path-Restir.hlsl");
			Renderer::GetShaderLibrary()->Load(RootSignature::RaytracingHLSL, "Resources/Shaders/Raytracing.hlsl");
		}

		//LoadDDGIShaders();

		Renderer::GetShaderLibrary()->Load(RootSignature::Draw, "Resources/Shaders/PBR_Transparent.glsl");
		Renderer::GetShaderLibrary()->Load(RootSignature::Draw, "Resources/Shaders/PBR_Static.glsl");
		Renderer::GetShaderLibrary()->Load(RootSignature::Draw, "Resources/Shaders/PBR_Anim.glsl");


		Renderer::GetShaderLibrary()->Load(RootSignature::ComputeHLSL, "Resources/Shaders/Exposure.hlsl");
		Renderer::GetShaderLibrary()->Load(RootSignature::ComputeGLSL, "Resources/Shaders/HZB.glsl");
		Renderer::GetShaderLibrary()->Load(RootSignature::Draw, "Resources/Shaders/Grid.glsl");
		Renderer::GetShaderLibrary()->Load(RootSignature::Draw, "Resources/Shaders/Wireframe.glsl");
		Renderer::GetShaderLibrary()->Load(RootSignature::Draw, "Resources/Shaders/Wireframe_Anim.glsl");
		Renderer::GetShaderLibrary()->Load(RootSignature::Draw, "Resources/Shaders/Skybox.glsl");
		Renderer::GetShaderLibrary()->Load(RootSignature::Draw, "Resources/Shaders/DirShadowMap.glsl");
		Renderer::GetShaderLibrary()->Load(RootSignature::Draw, "Resources/Shaders/DirShadowMap_Anim.glsl");
		Renderer::GetShaderLibrary()->Load(RootSignature::Draw, "Resources/Shaders/SpotShadowMap.glsl");
		Renderer::GetShaderLibrary()->Load(RootSignature::Draw, "Resources/Shaders/SpotShadowMap_Anim.glsl");

		//SSR
		Renderer::GetShaderLibrary()->Load(RootSignature::Draw, "Resources/Shaders/Pre-Integration.glsl");
		Renderer::GetShaderLibrary()->Load(RootSignature::ComputeGLSL, "Resources/Shaders/PostProcessing/Pre-Convolution.glsl");
		Renderer::GetShaderLibrary()->Load(RootSignature::ComputeGLSL, "Resources/Shaders/PostProcessing/SSR.glsl");
		Renderer::GetShaderLibrary()->Load(RootSignature::Draw, "Resources/Shaders/PostProcessing/SSR-Composite.glsl");

		// Environment compute shaders
		Renderer::GetShaderLibrary()->Load(RootSignature::ComputeGLSL, "Resources/Shaders/EnvironmentMipFilter.glsl");
		Renderer::GetShaderLibrary()->Load(RootSignature::ComputeGLSL, "Resources/Shaders/EquirectangularToCubeMap.glsl");
		Renderer::GetShaderLibrary()->Load(RootSignature::ComputeGLSL, "Resources/Shaders/EnvironmentIrradiance.glsl");
		Renderer::GetShaderLibrary()->Load(RootSignature::ComputeGLSL, "Resources/Shaders/PreethamSky.glsl");

		// Post-processing
		Renderer::GetShaderLibrary()->Load(RootSignature::ComputeGLSL, "Resources/Shaders/PostProcessing/Bloom.glsl");
		Renderer::GetShaderLibrary()->Load(RootSignature::ComputeGLSL, "Resources/Shaders/PostProcessing/DOF.glsl");
		Renderer::GetShaderLibrary()->Load(RootSignature::ComputeGLSL, "Resources/Shaders/PostProcessing/EdgeDetection.glsl");
		Renderer::GetShaderLibrary()->Load(RootSignature::Draw, "Resources/Shaders/PostProcessing/SceneComposite.glsl");

		// Light-culling
		Renderer::GetShaderLibrary()->Load(RootSignature::Draw, "Resources/Shaders/PreDepth.glsl");
		Renderer::GetShaderLibrary()->Load(RootSignature::Draw, "Resources/Shaders/PreDepth_Anim.glsl");
		Renderer::GetShaderLibrary()->Load(RootSignature::ComputeGLSL, "Resources/Shaders/LightCulling.glsl");

		// Renderer2D Shaders
		Renderer::GetShaderLibrary()->Load(RootSignature::Draw, "Resources/Shaders/Renderer2D.glsl");
		Renderer::GetShaderLibrary()->Load(RootSignature::Draw, "Resources/Shaders/Renderer2D_Line.glsl");
		Renderer::GetShaderLibrary()->Load(RootSignature::Draw, "Resources/Shaders/Renderer2D_Circle.glsl");
		Renderer::GetShaderLibrary()->Load(RootSignature::Draw, "Resources/Shaders/Renderer2D_Text.glsl");

		// Jump Flood Shaders
		Renderer::GetShaderLibrary()->Load(RootSignature::Draw, "Resources/Shaders/JumpFlood_Init.glsl");
		Renderer::GetShaderLibrary()->Load(RootSignature::Draw, "Resources/Shaders/JumpFlood_Pass.glsl");
		Renderer::GetShaderLibrary()->Load(RootSignature::Draw, "Resources/Shaders/JumpFlood_Composite.glsl");

		// GTAO
		Renderer::GetShaderLibrary()->Load(RootSignature::ComputeHLSL, "Resources/Shaders/PostProcessing/GTAO.hlsl");
		Renderer::GetShaderLibrary()->Load(RootSignature::ComputeGLSL, "Resources/Shaders/PostProcessing/GTAO-Denoise.glsl");

		// AO
		Renderer::GetShaderLibrary()->Load(RootSignature::Draw, "Resources/Shaders/PostProcessing/AO-Composite.glsl");

		// Misc
		Renderer::GetShaderLibrary()->Load(RootSignature::Draw, "Resources/Shaders/SelectedGeometry.glsl");
		Renderer::GetShaderLibrary()->Load(RootSignature::Draw, "Resources/Shaders/SelectedGeometry_Anim.glsl");
		Renderer::GetShaderLibrary()->Load(RootSignature::Draw, "Resources/Shaders/TexturePass.glsl");
		Renderer::GetShaderLibrary()->Load(RootSignature::Draw, "Resources/Shaders/TextureCopy.glsl");

		// Compile shaders
		Application::Get().GetRenderThread().Pump();

		{
			constexpr uint32_t missingTextureData = 0xffff45e0;
			TextureSpecification spec;
			spec.Format = ImageFormat::RGBA;
			spec.CreateBindlessDescriptor = true;
			spec.DebugName = "Missing Texture";
			s_Data->MissingTexture = Texture2D::Create(spec, Buffer(&missingTextureData, sizeof(uint32_t)));
		}

		{
			constexpr uint32_t whiteTextureData = 0xffffffff;
			TextureSpecification spec;
			spec.Format = ImageFormat::RGBA;
			spec.CreateBindlessDescriptor = true;
			spec.DebugName = "White Texture";
			s_Data->WhiteTexture = Texture2D::Create(spec, Buffer(&whiteTextureData, sizeof(uint32_t)));

			constexpr uint32_t blackTextureData = 0xff000000;
			spec.DebugName = "Black Texture";
			s_Data->BlackTexture = Texture2D::Create(spec, Buffer(&blackTextureData, sizeof(uint32_t)));

		}


		{
			constexpr uint32_t whiteTextureData = 0xffffffff;

			TextureSpecification spec;
			spec.Format = ImageFormat::RGBA;
			spec.CreateBindlessDescriptor = true;
			spec.GenerateMips = false;
			spec.Storage = true;
			spec.DebugName = "White Texture Array";
			spec.Layers = 2;
			s_Data->WhiteTextureArray = Texture2D::Create(spec, std::vector{ Buffer(&whiteTextureData, sizeof(uint32_t)), Buffer(&whiteTextureData, sizeof(uint32_t)) });


			constexpr uint32_t blackCubeTextureData[6] = { 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000 };
			spec.DebugName = "Black Cube Texture";
			s_Data->BlackCubeTexture = TextureCube::Create(spec, Buffer(&blackCubeTextureData, sizeof(blackCubeTextureData)));
		}




		{
			TextureSpecification spec;
			spec.SamplerWrap = TextureWrap::ClampToEdge;
			spec.DebugName = "BRDF LUT";
			spec.CreateBindlessDescriptor = true;
			s_Data->BRDFLutTexture = Texture2D::Create(spec, std::filesystem::path("Resources/Renderer/BRDF_LUT.png"));
		}

		s_Data->EmptyEnvironment = Ref<Environment>::Create(s_Data->BlackCubeTexture, s_Data->BlackCubeTexture);

		{
			s_Data->DefaultUniformBuffer = UniformBuffer::Create(1, "Default Uniform Buffer");
			StorageBufferSpecification storageBufferSpec;
			storageBufferSpec.DebugName = "Default Storage Buffer";
			s_Data->DefaultStorageBuffer = StorageBuffer::Create(1, storageBufferSpec);
		}

		// Hilbert look-up texture! It's a 64 x 64 uint16 texture
		{
			TextureSpecification spec;
			spec.Format = ImageFormat::RED16UI;
			spec.Width = 64;
			spec.Height = 64;
			spec.SamplerWrap = TextureWrap::ClampToEdge;
			spec.SamplerFilter = TextureFilter::Nearest;
			spec.DebugName = "Hilbert LUT";

			constexpr auto HilbertIndex = [](uint32_t posX, uint32_t posY)
			{
				uint16_t index = 0u;
				for (uint16_t curLevel = 64 / 2u; curLevel > 0u; curLevel /= 2u)
				{
					const uint16_t regionX = (posX & curLevel) > 0u;
					const uint16_t regionY = (posY & curLevel) > 0u;
					index += curLevel * curLevel * ((3u * regionX) ^ regionY);
					if (regionY == 0u)
					{
						if (regionX == 1u)
						{
							posX = uint16_t((64 - 1u)) - posX;
							posY = uint16_t((64 - 1u)) - posY;
						}

						std::swap(posX, posY);
					}
				}
				return index;
			};

			// Compile-time generation of the Hilbert curve lookup table
			constexpr auto hilbertLut = [HilbertIndex]() -> std::array<uint16_t, 64 * 64>
			{
				std::array<uint16_t, 64 * 64> data = {};
				for (uint32_t x = 0; x < 64; x++)
				{
					for (uint32_t y = 0; y < 64; y++)
					{
						const uint16_t r2index = HilbertIndex(x, y);
						data[x + 64 * y] = r2index;
					}
				}
				return data;
			};
			s_Data->HilbertLut = Texture2D::Create(spec, Buffer(hilbertLut().data(), 1));

		}

		s_RendererAPI->Init();
	}

	void Renderer::Shutdown()
	{
		s_ShaderDependencies.clear();
		s_RendererAPI->Shutdown();

		delete s_Data;

		// Resource release queue
		for (uint32_t i = 0; i < s_Config.FramesInFlight; i++)
		{
			auto& queue = Renderer::GetRenderResourceReleaseQueue(i);
			queue.Execute();
		}

		delete s_CommandQueue[0];
		delete s_CommandQueue[1];
	}

	RendererCapabilities& Renderer::GetCapabilities()
	{
		return s_RendererAPI->GetCapabilities();
	}

	Ref<ShaderLibrary> Renderer::GetShaderLibrary()
	{
		return s_Data->m_ShaderLibrary;
	}

	void Renderer::RenderThreadFunc(RenderThread* renderThread)
	{
		BEY_PROFILE_THREAD("Render Thread");

		while (renderThread->IsRunning())
		{
			WaitAndRender(renderThread);
		}
	}

	void Renderer::WaitAndRender(RenderThread* renderThread)
	{
		BEY_PROFILE_FUNC();
		auto& performanceTimers = Application::Get().m_PerformanceTimers;

		// Wait for kick, then set render thread to busy
		{
			BEY_PROFILE_SCOPE("Wait");
			Timer waitTimer;
			renderThread->WaitAndSet(RenderThread::State::Kick, RenderThread::State::Busy);
			performanceTimers.RenderThreadWaitTime = waitTimer.ElapsedMillis();
		}

		Timer workTimer;
		s_CommandQueue[GetRenderQueueIndex()]->Execute();
		// ExecuteRenderCommandQueue();

		// Rendering has completed, set state to idle
		renderThread->Set(RenderThread::State::Idle);

		performanceTimers.RenderThreadWorkTime = workTimer.ElapsedMillis();
	}

	void Renderer::SwapQueues()
	{
		s_RenderCommandQueueSubmissionIndex = (s_RenderCommandQueueSubmissionIndex + 1) % s_RenderCommandQueueCount;
	}

	uint32_t Renderer::GetRenderQueueIndex()
	{
		return (s_RenderCommandQueueSubmissionIndex + 1) % s_RenderCommandQueueCount;
	}

	uint32_t Renderer::GetRenderQueueSubmissionIndex()
	{
		return s_RenderCommandQueueSubmissionIndex;
	}

	void Renderer::BeginRenderPass(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<RenderPass> renderPass, bool explicitClear)
	{
		BEY_CORE_ASSERT(renderPass, "RenderPass cannot be null!");

		s_RendererAPI->BeginRenderPass(renderCommandBuffer, renderPass, explicitClear);
	}

	void Renderer::EndRenderPass(Ref<RenderCommandBuffer> renderCommandBuffer)
	{
		s_RendererAPI->EndRenderPass(renderCommandBuffer);
	}

	void Renderer::BeginComputePass(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<ComputePass> computePass)
	{
		BEY_CORE_ASSERT(computePass, "ComputePass cannot be null!");

		s_RendererAPI->BeginComputePass(renderCommandBuffer, computePass);
	}

	void Renderer::EndComputePass(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<ComputePass> computePass)
	{
		s_RendererAPI->EndComputePass(renderCommandBuffer, computePass);
	}

	void Renderer::DispatchCompute(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<ComputePass> computePass, Ref<Material> material, const glm::uvec3& workGroups, Buffer constants)
	{
		s_RendererAPI->DispatchCompute(renderCommandBuffer, computePass, material, workGroups, constants);
	}

	void Renderer::UpdateDDGIVolumes(Ref<RenderCommandBuffer> commandBuffer)
	{
		s_RendererAPI->UpdateDDGIData(commandBuffer);
	}

	void Renderer::BeginRaytracingPass(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<RaytracingPass> raytracingPass)
	{
		BEY_CORE_ASSERT(raytracingPass, "raytracingPass cannot be null!");
		s_RendererAPI->BeginRaytracingPass(renderCommandBuffer, raytracingPass);
	}

	void Renderer::EndRaytracingPass(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<RaytracingPass> raytracingPass)
	{
		s_RendererAPI->EndRaytracingPass(renderCommandBuffer, raytracingPass);
	}

	void Renderer::DispatchRays(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<RaytracingPass> raytracingPass, Ref<Material> material, const uint32_t width, const uint32_t height, const uint32_t depth)
	{
		s_RendererAPI->DispatchRays(renderCommandBuffer, raytracingPass, material, width, height, depth);
	}

	void Renderer::SetPushConstant(Ref<RaytracingPass> raytracingPass, Buffer pushConstant, ShaderStage stages)
	{
		s_RendererAPI->SetPushConstant(raytracingPass, pushConstant, stages);
	}

	void Renderer::InsertGPUPerfMarker(Ref<RenderCommandBuffer> renderCommandBuffer, const eastl::string& label, const glm::vec4& color)
	{
		s_RendererAPI->InsertGPUPerfMarker(renderCommandBuffer, label, color);
	}

	void Renderer::BeginGPUPerfMarker(Ref<RenderCommandBuffer> renderCommandBuffer, const eastl::string& label, const glm::vec4& markerColor)
	{
		s_RendererAPI->BeginGPUPerfMarker(renderCommandBuffer, label, markerColor);
	}

	void Renderer::EndGPUPerfMarker(Ref<RenderCommandBuffer> renderCommandBuffer)
	{
		s_RendererAPI->EndGPUPerfMarker(renderCommandBuffer);
	}

	void Renderer::RT_InsertGPUPerfMarker(Ref<RenderCommandBuffer> renderCommandBuffer, const eastl::string& label, const glm::vec4& color)
	{
		s_RendererAPI->RT_InsertGPUPerfMarker(renderCommandBuffer, label, color);
	}

	void Renderer::RT_BeginGPUPerfMarker(Ref<RenderCommandBuffer> renderCommandBuffer, const eastl::string& label, const glm::vec4& markerColor)
	{
		s_RendererAPI->RT_BeginGPUPerfMarker(renderCommandBuffer, label, markerColor);
	}

	void Renderer::RT_EndGPUPerfMarker(Ref<RenderCommandBuffer> renderCommandBuffer)
	{
		s_RendererAPI->RT_EndGPUPerfMarker(renderCommandBuffer);
	}

	void Renderer::BeginFrame()
	{
		s_RendererAPI->BeginFrame();
	}

	void Renderer::EndFrame()
	{
		s_RendererAPI->EndFrame();
	}

	std::pair<Ref<TextureCube>, Ref<TextureCube>> Renderer::CreateEnvironmentMap(const std::string& filepath)
	{
		return s_RendererAPI->CreateEnvironmentMap(filepath);
	}

	Ref<TextureCube> Renderer::CreatePreethamSky(float turbidity, float azimuth, float inclination)
	{
		return s_RendererAPI->CreatePreethamSky(turbidity, azimuth, inclination);
	}

	void Renderer::RenderStaticMesh(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<RasterPipeline> pipeline, Ref<StaticMesh> mesh, uint32_t submeshIndex, Ref<MaterialTable> materialTable, uint32_t drawID, uint32_t instanceCount)
	{
		s_RendererAPI->RenderStaticMesh(renderCommandBuffer, pipeline, mesh, submeshIndex, materialTable, drawID, instanceCount);
	}

#if 0
	void Renderer::RenderSubmesh(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<RasterPipeline> pipeline, Ref<UniformBufferSet> uniformBufferSet, Ref<StorageBufferSet> storageBufferSet, Ref<Mesh> mesh, uint32_t submeshIndex, Ref<MaterialTable> materialTable, const glm::mat4& transform)
	{
		s_RendererAPI->RenderSubmesh(renderCommandBuffer, pipeline, uniformBufferSet, storageBufferSet, mesh, submeshIndex, materialTable, transform);
	}
#endif

	void Renderer::RenderSubmeshInstanced(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<RasterPipeline> pipeline, Ref<Mesh> mesh, uint32_t submeshIndex, Ref<MaterialTable> materialTable, uint32_t boneTransformsOffset, uint32_t drawID, uint32_t instanceCount)
	{
		s_RendererAPI->RenderSubmeshInstanced(renderCommandBuffer, pipeline, mesh, submeshIndex, materialTable, boneTransformsOffset, drawID, instanceCount);
	}

	void Renderer::RenderMeshWithMaterial(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<RasterPipeline> pipeline, Ref<Mesh> mesh, uint32_t submeshIndex, uint32_t boneTransformsOffset, uint32_t drawID, uint32_t instanceCount, Ref<Material> material, Buffer additionalUniforms)
	{
		s_RendererAPI->RenderMeshWithMaterial(renderCommandBuffer, pipeline, mesh, submeshIndex, material, boneTransformsOffset, drawID, instanceCount, additionalUniforms);
	}

	void Renderer::RenderStaticMeshWithMaterial(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<RasterPipeline> pipeline, Ref<StaticMesh> mesh, uint32_t submeshIndex, Ref<Material> material, uint32_t drawID, uint32_t instanceCount, Buffer additionalUniforms)
	{
		s_RendererAPI->RenderStaticMeshWithMaterial(renderCommandBuffer, pipeline, mesh, submeshIndex, material, drawID, instanceCount, additionalUniforms);
	}

	void Renderer::RenderQuad(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<RasterPipeline> pipeline, Ref<Material> material, const glm::mat4& transform)
	{
		s_RendererAPI->RenderQuad(renderCommandBuffer, pipeline, material, transform);
	}

	void Renderer::RenderGeometry(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<RasterPipeline> pipeline, Ref<Material> material, Ref<VertexBuffer> vertexBuffer, Ref<IndexBuffer> indexBuffer, const glm::mat4& transform, uint32_t indexCount /*= 0*/)
	{
		s_RendererAPI->RenderGeometry(renderCommandBuffer, pipeline, material, vertexBuffer, indexBuffer, transform, indexCount);
	}

	void Renderer::SubmitQuad(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<Material> material, const glm::mat4& transform)
	{
		BEY_CORE_ASSERT(false, "Not Implemented");
		/*bool depthTest = true;
		if (material)
		{
				material->Bind();
				depthTest = material->GetFlag(MaterialFlag::DepthTest);
				cullFace = !material->GetFlag(MaterialFlag::TwoSided);

				auto shader = material->GetShader();
				shader->SetUniformBuffer("Transform", &transform, sizeof(glm::mat4));
		}

		s_Data->m_FullscreenQuadVertexBuffer->Bind();
		s_Data->m_FullscreenQuadPipeline->Bind();
		s_Data->m_FullscreenQuadIndexBuffer->Bind();
		Renderer::DrawIndexed(6, PrimitiveType::Triangles, depthTest);*/
	}

	void Renderer::ClearImage(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<Image2D> image, const ImageClearValue& clearValue, ImageSubresourceRange subresourceRange)
	{
		s_RendererAPI->ClearImage(renderCommandBuffer, image, clearValue, subresourceRange);
	}

	void Renderer::CopyImage(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<Image2D> sourceImage, Ref<Image2D> destinationImage)
	{
		s_RendererAPI->CopyImage(renderCommandBuffer, sourceImage, destinationImage);
	}

	void Renderer::BlitDepthImage(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<Image2D> sourceImage, Ref<Image2D> destinationImage)
	{
		s_RendererAPI->BlitDepthImage(renderCommandBuffer, sourceImage, destinationImage);
	}

	void Renderer::CopyBuffer(Ref<RenderCommandBuffer> renderCommandBuffer, void* dest, Ref<StorageBuffer> storageBuffer)
	{
		s_RendererAPI->CopyBuffer(renderCommandBuffer, dest, storageBuffer);
	}

	void Renderer::SubmitBarrier(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<Image2D> image, const RendererUtils::ImageBarrier& barrier)
	{
		s_RendererAPI->SubmitBarrier(renderCommandBuffer, image, barrier);
	}

	void Renderer::AddBindlessDescriptor(RenderPassInput&& input)
	{
		s_RendererAPI->AddBindlessDescriptor(std::forward<RenderPassInput>(input));
	}

	void Renderer::UpdateBindlessDescriptorSet(bool forceRebakeAll)
	{
		s_RendererAPI->UpdateBindlessDescriptorSet(forceRebakeAll);
	}

	void Renderer::AddBindlessShader(Ref<Shader> shader)
	{
		s_RendererAPI->AddBindlessShader(shader);
	}

	void Renderer::SetDDGITextureResources()
	{
		s_RendererAPI->SetDDGITextureResources();
	}

	void Renderer::SetDDGIResources(Ref<StorageBuffer> constantsBuffer, Ref<StorageBuffer> indicesBuffer)
	{
		s_RendererAPI->SetDDGIResources(constantsBuffer, indicesBuffer);
	}

	void Renderer::InitDDGI(Ref<RenderCommandBuffer> commandBuffer, const std::vector<rtxgi::DDGIVolumeDesc>& ddgiVolumeDescs)
	{
		s_RendererAPI->InitDDGI(commandBuffer, ddgiVolumeDescs);
	}

	void Renderer::SetDDGIStorage(Ref<StorageBuffer> constantsBuffer, Ref<StorageBuffer> resourceIndices)
	{
		s_RendererAPI->SetDDGIStorage(constantsBuffer, resourceIndices);
	}

	/*rtxgi::vulkan::DDGIVolumeResources& Renderer::GetVulkanDDGIResources()
	{
		return s_RendererAPI->GetVulkanDDGIResources();
	}*/

	void Renderer::SubmitFullscreenQuad(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<RasterPipeline> pipeline, Ref<Material> material)
	{
		s_RendererAPI->SubmitFullscreenQuad(renderCommandBuffer, pipeline, material);
	}

	void Renderer::SubmitFullscreenQuadWithOverrides(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<RasterPipeline> pipeline, Ref<Material> material, Buffer vertexShaderOverrides, Buffer fragmentShaderOverrides)
	{
		s_RendererAPI->SubmitFullscreenQuadWithOverrides(renderCommandBuffer, pipeline, material, vertexShaderOverrides, fragmentShaderOverrides);
	}



#if 0
	void Renderer::SubmitFullscreenQuad(Ref<Material> material)
	{
		// Retrieve pipeline from cache
		auto& shader = material->GetShader();
		auto hash = shader->GetHash();
		if (s_PipelineCache.find(hash) == s_PipelineCache.end())
		{
			// Create pipeline
			PipelineSpecification spec = s_Data->m_FullscreenQuadPipelineSpec;
			spec.Shader = shader;
			spec.DebugName = "Renderer-FullscreenQuad-" + shader->GetName();
			s_PipelineCache[hash] = RasterPipeline::Create(spec);
		}

		auto& pipeline = s_PipelineCache[hash];

		bool depthTest = true;
		bool cullFace = true;
		if (material)
		{
			// material->Bind();
			depthTest = material->GetFlag(MaterialFlag::DepthTest);
			cullFace = !material->GetFlag(MaterialFlag::TwoSided);
		}

		s_Data->FullscreenQuadVertexBuffer->Bind();
		pipeline->Bind();
		s_Data->FullscreenQuadIndexBuffer->Bind();
		Renderer::DrawIndexed(6, PrimitiveType::Triangles, depthTest);
	}
#endif

	Ref<Texture2D> Renderer::GetMissingTexture()
	{
		return s_Data->MissingTexture;
	}

	Ref<Texture2D> Renderer::GetWhiteTexture()
	{
		return s_Data->WhiteTexture;
	}

	Ref<Texture2D> Renderer::GetWhiteArrayTexture()
	{
		return s_Data->WhiteTextureArray;
	}

	Ref<Texture2D> Renderer::GetBlackTexture()
	{
		return s_Data->BlackTexture;
	}

	Ref<Texture2D> Renderer::GetHilbertLut()
	{
		return s_Data->HilbertLut;
	}

	Ref<Texture2D> Renderer::GetBRDFLutTexture()
	{
		return s_Data->BRDFLutTexture;
	}

	Ref<TextureCube> Renderer::GetBlackCubeTexture()
	{
		return s_Data->BlackCubeTexture;
	}

	Ref<StorageBuffer> Renderer::GetDefaultStorageBuffer()
	{
		return s_Data->DefaultStorageBuffer;
	}

	Ref<UniformBuffer> Renderer::GetDefaultUniformBuffer()
	{
		return s_Data->DefaultUniformBuffer;
	}

	Ref<Environment> Renderer::GetEmptyEnvironment()
	{
		return s_Data->EmptyEnvironment;
	}

	Ref<Sampler> Renderer::GetBilinearSampler()
	{
		return s_RendererAPI->GetBilinearSampler();
	}

	Ref<Sampler> Renderer::GetPointSampler()
	{
		return s_RendererAPI->GetPointSampler();
	}

	Ref<Sampler> Renderer::GetAnisoSampler()
	{
		return s_RendererAPI->GetAnisoSampler();
	}

	std::vector<uint32_t> Renderer::GetBindlessSets()
	{
		return s_RendererAPI->GetBindlessSets();
	}

	std::vector<std::unique_ptr<rtxgi::vulkan::DDGIVolume>>& Renderer::GetDDGIVolumes()
	{
		return s_RendererAPI->GetDDGIVolumes();
	}

	RenderCommandQueue& Renderer::GetRenderCommandQueue()
	{
		return *s_CommandQueue[s_RenderCommandQueueSubmissionIndex];
	}

	RenderCommandQueue& Renderer::GetRenderResourceReleaseQueue(uint32_t index)
	{
		return s_ResourceFreeQueue[index];
	}


	const std::unordered_map<std::string, std::string>& Renderer::GetGlobalShaderMacros()
	{
		return s_Data->GlobalShaderMacros;
	}

	RendererConfig& Renderer::GetConfig()
	{
		return s_Config;
	}

	void Renderer::SetConfig(const RendererConfig& config)
	{
		s_Config = config;
	}

	void Renderer::AcknowledgeParsedGlobalMacros(const std::unordered_set<std::string>& macros, Ref<Shader> shader)
	{
		for (const std::string& macro : macros)
		{
			s_GlobalShaderInfo.ShaderGlobalMacrosMap[macro][shader->GetHash()] = shader;
		}
	}

	void Renderer::SetMacroInShader(Ref<Shader> shader, const std::string& name, const std::string& value)
	{
		shader->SetMacro(name, value);
		s_GlobalShaderInfo.DirtyShaders.emplace(shader.Raw());
	}

	void Renderer::SetGlobalMacroInShaders(const std::string& name, const std::string& value)
	{
		if (s_Data->GlobalShaderMacros.find(name) != s_Data->GlobalShaderMacros.end())
		{
			if (s_Data->GlobalShaderMacros.at(name) == value)
				return;
		}

		s_Data->GlobalShaderMacros[name] = value;

		if (s_GlobalShaderInfo.ShaderGlobalMacrosMap.find(name) == s_GlobalShaderInfo.ShaderGlobalMacrosMap.end())
		{
			BEY_CORE_WARN("No shaders with {} macro found", name);
			return;
		}

		BEY_CORE_ASSERT(s_GlobalShaderInfo.ShaderGlobalMacrosMap.find(name) != s_GlobalShaderInfo.ShaderGlobalMacrosMap.end(), "Macro has not been passed from any shader!");
		for (auto& shader : s_GlobalShaderInfo.ShaderGlobalMacrosMap.at(name) | std::views::values)
		{
			BEY_CORE_ASSERT(shader.IsValid(), "Shader is deleted!");
			s_GlobalShaderInfo.DirtyShaders.emplace(shader);
		}
	}

	bool Renderer::UpdateDirtyShaders()
	{
		// TODO: how is this going to work for dist?
		const bool updatedAnyShaders = !s_GlobalShaderInfo.DirtyShaders.empty();
		for (WeakRef<Shader> shader : s_GlobalShaderInfo.DirtyShaders)
		{
			BEY_CORE_ASSERT(shader.IsValid(), "Shader is deleted!");
			shader->RT_Reload(true);
		}
		s_GlobalShaderInfo.DirtyShaders.clear();

		return updatedAnyShaders;
	}

	GPUMemoryStats Renderer::GetGPUMemoryStats()
	{
		return VulkanAllocator::GetStats();
	}

	bool Renderer::UpdatedShaders()
	{
		return s_Data->HasUpdatedShaders.exchange(false);
	}

	void Renderer::NotifyShaderUpdate()
	{
		s_Data->HasUpdatedShaders = true;
	}
}
