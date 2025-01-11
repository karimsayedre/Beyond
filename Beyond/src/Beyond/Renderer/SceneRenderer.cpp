#include "pch.h"
#include "SceneRenderer.h"

#include "Renderer.h"
#include "SceneEnvironment.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/compatibility.hpp>
#include <imgui.h>
#include <imgui/imgui_internal.h>

#include "Beyond/Core/Application.h"
#include "Beyond/Core/Math/Noise.h"
#include "Raytracer.h"
#include "Renderer2D.h"
#include "UniformBuffer.h"

#include "Beyond/Utilities/FileSystem.h"

#include "Beyond/Debug/Profiler.h"
#include "Beyond/Editor/PanelManager.h"
#include "Beyond/ImGui/ImGui.h"
#include "Beyond/Math/Math.h"
#include "Beyond/Platform/Vulkan/VulkanRenderer.h"
#include "rtxgi/VulkanExtensions.h"
#include "rtxgi/ddgi/DDGIVolume.h"
#include "rtxgi/ddgi/gfx/DDGIVolume_D3D12.h"
#include "rtxgi/ddgi/gfx/DDGIVolume_VK.h"
#include "RTXGI.h"
#include "Beyond/Platform/Vulkan/VulkanDLSS.h"

namespace Beyond {

	static std::vector<std::thread> s_ThreadPool;

	SceneRenderer::SceneRenderer(Ref<Scene> scene, SceneRendererSpecification specification)
		: m_Scene(scene), m_Specification(specification)
	{
		Init();
	}

	SceneRenderer::~SceneRenderer()
	{
		Shutdown();
	}

	void SceneRenderer::Init()
	{
		BEY_SCOPE_TIMER("SceneRenderer::Init");

		m_ShadowCascadeSplits[0] = 0.1f;
		m_ShadowCascadeSplits[1] = 0.2f;
		m_ShadowCascadeSplits[2] = 0.3f;
		m_ShadowCascadeSplits[3] = 1.0f;

		// Tiering
		{
			using namespace Tiering::Renderer;
			if (VulkanContext::GetCurrentDevice()->IsDLSSSupported())
			{
				m_DLSS = DLSS::Create();
				m_DLSSSupported = m_DLSS->CheckSupport();
			}
			else
			{
				m_DLSSSupported = false;
				m_DLSSSettings.Enable = false;
			}

			if (!VulkanContext::GetCurrentDevice()->IsRaytracingSupported())
			{
				m_RaytracingSettings.Mode = RaytracingMode::None;
			}


			const auto& tiering = m_Specification.Tiering;

			RendererDataUB.SoftShadows = tiering.ShadowQuality == ShadowQualitySetting::High;

			m_Options.EnableGTAO = false;

			switch (tiering.AOQuality)
			{
				case Tiering::Renderer::AmbientOcclusionQualitySetting::High:
					m_Options.EnableGTAO = true;
					GTAODataCB.HalfRes = true;
					break;
				case Tiering::Renderer::AmbientOcclusionQualitySetting::Ultra:
					m_Options.EnableGTAO = true;
					GTAODataCB.HalfRes = false;
					break;
			}

			if (tiering.EnableAO)
			{
				switch (tiering.AOType)
				{
					case Tiering::Renderer::AmbientOcclusionTypeSetting::GTAO:
						m_Options.EnableGTAO = true;
						break;
				}
			}

			switch (tiering.SSRQuality)
			{
				case SSRQualitySetting::Off:
					m_Options.EnableSSR = false;
					m_SSROptions.HalfRes = true;
					break;
				case Tiering::Renderer::SSRQualitySetting::Medium:
					m_Options.EnableSSR = true;
					m_SSROptions.HalfRes = true;
					break;
				case Tiering::Renderer::SSRQualitySetting::High:
					m_Options.EnableSSR = true;
					m_SSROptions.HalfRes = false;
					break;
			}


			// OVERRIDE
			m_Options.EnableGTAO = false;
		}

		m_CommandBuffers.reserve(CmdBuffers::Count);
		for (int i = 0; i < CmdBuffers::Count; ++i)
		{
			m_CommandBuffers.emplace_back(RenderCommandBuffer::Create(0, s_CommandBufferIsComputeQueue[i], s_CommandBufferNames[i]));
		}
		m_MainCommandBuffer = m_CommandBuffers[CmdBuffers::eMain];

		m_GPUSemaphores[GPUSemaphoreUsage::TLASBuild] = GPUSemaphore::Create();

		uint32_t framesInFlight = Renderer::GetConfig().FramesInFlight;

		m_UBSCamera = UniformBufferSet::Create(sizeof(UBCamera), "SceneRenderer Camera");
		m_UBSShadow = UniformBufferSet::Create(sizeof(UBShadow), "Dir Shadow Matrices");
		m_UBSScene = UniformBufferSet::Create(sizeof(UBScene), "Scene Data");
		m_UBSRendererData = UniformBufferSet::Create(sizeof(UBRendererData), "Renderer Data");
		m_UBSPointLights = UniformBufferSet::Create(sizeof(UBPointLights), "Point Lights");
		m_UBSScreenData = UniformBufferSet::Create(sizeof(UBScreenData), "Screen Data");
		m_UBSSpotLights = UniformBufferSet::Create(sizeof(UBSpotLights), "Spot Lights");
		m_UBSSpotShadowData = UniformBufferSet::Create(sizeof(UBSpotShadowData), "Spot Light Shadow Matrices");

		{
			StorageBufferSpecification spec;
			spec.DebugName = "BoneTransforms";
			spec.GPUOnly = false;
			const size_t BoneTransformBufferCount = 1 * 1024; // basically means limited to 1024 animated meshes   TODO(0x): resizeable/flushable
			m_SBSBoneTransforms = StorageBufferSet::Create(spec, sizeof(BoneTransforms) * BoneTransformBufferCount);
			m_BoneTransformsData = hnew BoneTransforms[BoneTransformBufferCount];
		}

		{
			StorageBufferSpecification spec;
			spec.DebugName = "Transforms";
			spec.GPUOnly = false;
			const size_t transformsCount = 1 * 1024; // basically means limited to 1024 animated meshes   TODO(0x): resizeable/flushable
			m_SBSTransforms = StorageBufferSet::Create(spec, sizeof(TransformData) * transformsCount);
			//m_TransformsData = hnew TransformVertexData[transformsCount];
		}

		//DDGI 
		{
			StorageBufferSpecification spec;
			spec.DebugName = "DDGIConstantsBuffer";
			spec.GPUOnly = false;
			m_SBDDGIConstants = StorageBuffer::Create(sizeof(rtxgi::DDGIVolumeDescGPUPacked) * 1 * Renderer::GetConfig().FramesInFlight, spec); // 1 should be the volume count

			spec.DebugName = "DDGIVolumeResourceIndices";
			spec.GPUOnly = false;
			m_SBDDGIReourceIndices = StorageBuffer::Create(sizeof(rtxgi::DDGIVolumeResourceIndices) * 1 * Renderer::GetConfig().FramesInFlight, spec); // 1 should be the volume count
		}

		// DDGI Vis Probe Instances
		{
			StorageBufferSpecification spec;
			spec.DebugName = "DDGI Vis Probe Instances";
			spec.GPUOnly = false;
			m_SBSDDGIProbeInstances = StorageBufferSet::Create(spec, 1);
		}

		ImageSpecification imageSpec;
		imageSpec.Usage = ImageUsage::Storage;
		imageSpec.Format = ImageFormat::B10G11R11UFLOAT;
		imageSpec.DebugName = "DebugImage";
		m_DebugImage = Image2D::Create(imageSpec);
		m_DebugImage->Invalidate();

		// Create passes and specify "flow" -> this can (and should) include outputs and chain together,
		// for eg. shadow pass output goes into geo pass input

		m_Renderer2D = Ref<Renderer2D>::Create();
		m_Renderer2DScreenSpace = Ref<Renderer2D>::Create();
		m_DebugRenderer = Ref<DebugRenderer>::Create();

		m_CompositeShader = Renderer::GetShaderLibrary()->Get("SceneComposite");
		m_CompositeMaterial = Material::Create(m_CompositeShader);

		// Descriptor Set Layout
		// 

		// Light culling compute pipeline
		{
			StorageBufferSpecification spec;
			spec.DebugName = "VisiblePointLightIndices";
			m_SBSVisiblePointLightIndicesBuffer = StorageBufferSet::Create(spec, 4); // Resized later

			spec.DebugName = "VisibleSpotLightIndices";
			m_SBSVisibleSpotLightIndicesBuffer = StorageBufferSet::Create(spec, 4); // Resized later

			Ref<Shader> lightCullingShader = Renderer::GetShaderLibrary()->Get("LightCulling");
			m_LightCullingPipeline = PipelineCompute::Create(lightCullingShader);
		}

		VertexBufferLayout vertexLayout = {
			{ ShaderDataType::Float3, "a_Position" },
			{ ShaderDataType::Float3, "a_Normal" },
			{ ShaderDataType::Float3, "a_Tangent" },
			{ ShaderDataType::Float3, "a_Binormal" },
			{ ShaderDataType::Float2, "a_TexCoord" }
		};

		//VertexBufferLayout instanceLayout = {
		//	{ ShaderDataType::Float4, "a_MRow0" },
		//	{ ShaderDataType::Float4, "a_MRow1" },
		//	{ ShaderDataType::Float4, "a_MRow2" },
		//};

		VertexBufferLayout boneInfluenceLayout = {
			{ ShaderDataType::Int4,   "a_BoneIDs" },
			{ ShaderDataType::Float4, "a_BoneWeights" },
		};

		uint32_t shadowMapResolution = 4096;
		switch (m_Specification.Tiering.ShadowResolution)
		{
			case Tiering::Renderer::ShadowResolutionSetting::Low:
				shadowMapResolution = 1024;
				break;
			case Tiering::Renderer::ShadowResolutionSetting::Medium:
				shadowMapResolution = 2048;
				break;
			case Tiering::Renderer::ShadowResolutionSetting::High:
				shadowMapResolution = 4096;
				break;
		}

		// Bloom Compute
		{
			auto shader = Renderer::GetShaderLibrary()->Get("Bloom");
			m_BloomComputePipeline = PipelineCompute::Create(shader);
			{
				TextureSpecification spec;
				spec.Format = ImageFormat::B10G11R11UFLOAT;
				spec.SamplerWrap = TextureWrap::ClampToEdge;
				spec.Storage = true;
				spec.Compress = false;
				spec.DebugName = "BloomCompute-0";
				m_BloomComputeTextures[0].Texture = Texture2D::Create(spec);
				spec.DebugName = "BloomCompute-1";
				m_BloomComputeTextures[1].Texture = Texture2D::Create(spec);
				spec.DebugName = "BloomCompute-2";
				m_BloomComputeTextures[2].Texture = Texture2D::Create(spec);
			}

			// Image Views (per-mip)
			ImageViewSpecification imageViewSpec;
			for (int i = 0; i < 3; i++)
			{
				imageViewSpec.DebugName = fmt::eastl_format("BloomCompute-{}", i);
				uint32_t mipCount = m_BloomComputeTextures[i].Texture->GetMipLevelCount();
				m_BloomComputeTextures[i].ImageViews.resize(mipCount);
				for (uint32_t mip = 0; mip < mipCount; mip++)
				{
					imageViewSpec.Image = m_BloomComputeTextures[i].Texture->GetImage();
					imageViewSpec.Mip = mip;
					m_BloomComputeTextures[i].ImageViews[mip] = ImageView::Create(imageViewSpec);
				}
			}

			{
				ComputePassSpecification spec;
				spec.DebugName = "Bloom-Compute";
				auto shader = Renderer::GetShaderLibrary()->Get("Bloom");
				spec.Pipeline = PipelineCompute::Create(shader);
				m_BloomComputePass = ComputePass::Create(spec);
				BEY_CORE_VERIFY(m_BloomComputePass->Validate());
				m_BloomComputePass->Bake();
			}

			m_BloomDirtTexture = Renderer::GetBlackTexture();

			// TODO
			// m_BloomComputePrefilterMaterial = Material::Create(shader);
		}

		// Directional Shadow pass
		{
			ImageSpecification spec;
			spec.Format = ImageFormat::DEPTH32F;
			spec.Usage = ImageUsage::Attachment;
			spec.Width = shadowMapResolution;
			spec.Height = shadowMapResolution;
			spec.Layers = m_Specification.NumShadowCascades;
			spec.DebugName = "ShadowCascades";
			Ref<Image2D> cascadedDepthImage = Image2D::Create(spec);
			cascadedDepthImage->Invalidate();
			if (m_Specification.NumShadowCascades > 1)
				cascadedDepthImage->CreatePerLayerImageViews();

			FramebufferSpecification shadowMapFramebufferSpec;
			shadowMapFramebufferSpec.DebugName = "Dir Shadow Map";
			shadowMapFramebufferSpec.Width = shadowMapResolution;
			shadowMapFramebufferSpec.Height = shadowMapResolution;
			shadowMapFramebufferSpec.Attachments = { ImageFormat::DEPTH32F };
			shadowMapFramebufferSpec.ClearColor = { 0.0f, 0.0f, 0.0f, 0.0f };
			shadowMapFramebufferSpec.DepthClearValue = 1.0f;
			shadowMapFramebufferSpec.NoResize = true;
			shadowMapFramebufferSpec.ExistingImage = cascadedDepthImage;

			auto shadowPassShader = Renderer::GetShaderLibrary()->Get("DirShadowMap");
			auto shadowPassShaderAnim = Renderer::GetShaderLibrary()->Get("DirShadowMap_Anim");

			PipelineSpecification pipelineSpec;
			pipelineSpec.DebugName = "DirShadowPass";
			pipelineSpec.Shader = shadowPassShader;
			pipelineSpec.DepthOperator = DepthCompareOperator::LessOrEqual;
			pipelineSpec.Layout = vertexLayout;
			//pipelineSpec.InstanceLayout = instanceLayout;

			PipelineSpecification pipelineSpecAnim = pipelineSpec;
			pipelineSpecAnim.DebugName = "DirShadowPass-Anim";
			pipelineSpecAnim.Shader = shadowPassShaderAnim;
			pipelineSpecAnim.BoneInfluenceLayout = boneInfluenceLayout;

			RenderPassSpecification shadowMapRenderPassSpec;
			shadowMapRenderPassSpec.DebugName = shadowMapFramebufferSpec.DebugName;

			// 4 cascades by default
			m_DirectionalShadowMapPass.resize(m_Specification.NumShadowCascades);
			m_DirectionalShadowMapAnimPass.resize(m_Specification.NumShadowCascades);
			for (uint32_t i = 0; i < m_Specification.NumShadowCascades; i++)
			{
				shadowMapFramebufferSpec.ExistingImageLayers.clear();
				shadowMapFramebufferSpec.ExistingImageLayers.emplace_back(i);

				shadowMapFramebufferSpec.ClearDepthOnLoad = true;
				pipelineSpec.TargetFramebuffer = Framebuffer::Create(shadowMapFramebufferSpec);

				m_ShadowPassPipelines[i] = RasterPipeline::Create(pipelineSpec);
				shadowMapRenderPassSpec.Pipeline = m_ShadowPassPipelines[i];

				shadowMapFramebufferSpec.ClearDepthOnLoad = false;
				pipelineSpecAnim.TargetFramebuffer = Framebuffer::Create(shadowMapFramebufferSpec);
				m_ShadowPassPipelinesAnim[i] = RasterPipeline::Create(pipelineSpecAnim);

				m_DirectionalShadowMapPass[i] = RenderPass::Create(shadowMapRenderPassSpec);
				m_DirectionalShadowMapPass[i]->SetInput("u_DirShadow", m_UBSShadow);
				m_DirectionalShadowMapPass[i]->SetInput("r_Transforms", m_SBSTransforms);
				BEY_CORE_VERIFY(m_DirectionalShadowMapPass[i]->Validate());
				m_DirectionalShadowMapPass[i]->Bake();

				shadowMapRenderPassSpec.Pipeline = m_ShadowPassPipelinesAnim[i];
				m_DirectionalShadowMapAnimPass[i] = RenderPass::Create(shadowMapRenderPassSpec);
				m_DirectionalShadowMapAnimPass[i]->SetInput("u_DirShadow", m_UBSShadow);
				m_DirectionalShadowMapAnimPass[i]->SetInput("r_BoneTransforms", m_SBSBoneTransforms);
				m_DirectionalShadowMapAnimPass[i]->SetInput("r_Transforms", m_SBSTransforms);
				BEY_CORE_VERIFY(m_DirectionalShadowMapAnimPass[i]->Validate());
				m_DirectionalShadowMapAnimPass[i]->Bake();
			}
			m_ShadowPassMaterial = Material::Create(shadowPassShader, "DirShadowPass");
		}

		// Non-directional shadow mapping pass
		{
			FramebufferSpecification framebufferSpec;
			framebufferSpec.Width = shadowMapResolution; // TODO: could probably halve these
			framebufferSpec.Height = shadowMapResolution;
			framebufferSpec.Attachments = { ImageFormat::DEPTH32F };
			framebufferSpec.DepthClearValue = 1.0f;
			framebufferSpec.NoResize = true;
			framebufferSpec.DebugName = "Spot Shadow Map";

			auto shadowPassShader = Renderer::GetShaderLibrary()->Get("SpotShadowMap");
			auto shadowPassShaderAnim = Renderer::GetShaderLibrary()->Get("SpotShadowMap_Anim");

			PipelineSpecification pipelineSpec;
			pipelineSpec.DebugName = "SpotShadowPass";
			pipelineSpec.Shader = shadowPassShader;
			pipelineSpec.TargetFramebuffer = Framebuffer::Create(framebufferSpec);
			pipelineSpec.DepthOperator = DepthCompareOperator::LessOrEqual;

			pipelineSpec.Layout = vertexLayout;
			//pipelineSpec.InstanceLayout = {
			//	{ ShaderDataType::Float4, "a_MRow0" },
			//	{ ShaderDataType::Float4, "a_MRow1" },
			//	{ ShaderDataType::Float4, "a_MRow2" },
			//};

			PipelineSpecification pipelineSpecAnim = pipelineSpec;
			pipelineSpecAnim.DebugName = "SpotShadowPass-Anim";
			pipelineSpecAnim.Shader = shadowPassShaderAnim;
			pipelineSpecAnim.BoneInfluenceLayout = boneInfluenceLayout;

			m_SpotShadowPassPipeline = RasterPipeline::Create(pipelineSpec);
			m_SpotShadowPassAnimPipeline = RasterPipeline::Create(pipelineSpecAnim);

			m_SpotShadowPassMaterial = Material::Create(shadowPassShader, "SpotShadowPass");

			RenderPassSpecification spotShadowPassSpec;
			spotShadowPassSpec.DebugName = "SpotShadowMap";
			spotShadowPassSpec.Pipeline = m_SpotShadowPassPipeline;
			m_SpotShadowPass = RenderPass::Create(spotShadowPassSpec);

			m_SpotShadowPass->SetInput("u_SpotLightMatrices", m_UBSSpotShadowData);
			m_SpotShadowPass->SetInput("r_Transforms", m_SBSTransforms);

			BEY_CORE_VERIFY(m_SpotShadowPass->Validate());
			m_SpotShadowPass->Bake();
		}

		// Define/setup renderer resources that are provided by CPU (meaning not GPU pass outputs)
		// eg. camera buffer, environment textures, BRDF LUT, etc.

		// CHANGES:
		// - RenderPass class becomes implemented instead of just placeholder for data
		// - RasterPipeline no longer contains render pass in spec (NOTE: double check this is okay)
		// - RenderPass contains RasterPipeline in spec

		{
			//
			// A render pass should provide context and initiate (prepare) certain layout transitions for all
			// required resources. This should be easy to check and ensure everything is ready.
			// 
			// Passes should be somewhat pre-baked to contain ready-to-go descriptors that aren't draw-call
			// specific. Beyond defines Set 0 as per-draw - so usually materials. Sets 1-3 are scene/renderer owned.
			//
			// This means that when you define a render pass, you need to set-up required inputs from Sets 1-3
			// and based on what is used here, these need to get baked into ready-to-go allocated descriptor sets,
			// for that specific render pass draw call (so we can use vkCmdBindDescriptorSets).
			//
			// API could look something like:

			// Ref<RenderPass> shadowMapRenderPass[4]; // Four shadow map passes, into single layered framebuffer
			// 
			// {
			// 	for (int i = 0; i < 4; i++)
			// 	{
			// 		RenderPassSpecification spec;
			// 		spec.DebugName = "ShadowMapPass";
			// 		spec.RasterPipeline = m_ShadowPassPipelines[i];
			// 		//spec.TargetFramebuffer = m_ShadowPassPipelines[i]->GetSpecification().RenderPass->GetSpecification().TargetFramebuffer; // <- set framebuffer here
			// 
			// 		shadowMapRenderPass[i] = RenderPass::Create(spec);
			// 		// shadowMapRenderPass[i]->GetRequiredInputs(); // Returns list of sets+bindings of required resources from descriptor layout
			// 
			// 		// NOTE:
			// 		// AddInput needs to potentially take in the set + binding of the resource
			// 		// We (currently) don't store the actual variable name, just the struct type (eg. ShadowData and not u_ShadowData),
			// 		// so if there are multiple instances of the ShadowData struct then it's ambiguous.
			// 		// I suspect clashes will be rare - usually we only have one input per type/buffer
			// 		shadowMapRenderPass[i]->SetInput("u_DirShadow", m_UBSShadow);
			// 		// Note: outputs are automatically set by framebuffer
			// 
			// 		// Bake will create descriptor sets and ensure everything is ready for rendering
			// 		// If resources (eg. storage buffers/images) resize, passes need to be invalidated
			// 		// so we can re-create proper descriptors to the newly created replacement resources
			// 		BEY_CORE_VERIFY(shadowMapRenderPass[i]->Validate());
			// 		shadowMapRenderPass[i]->Bake();
			// 	}
			// }

			// PreDepth
			{
				FramebufferSpecification preDepthFramebufferSpec;
				preDepthFramebufferSpec.DebugName = "PreDepth-Opaque";
				//Linear depth, reversed device depth
				preDepthFramebufferSpec.Attachments = { ImageFormat::RG16F, ImageFormat::DEPTH32F };
				preDepthFramebufferSpec.ClearColor = { 0.0f, 0.0f, 0.0f, 0.0f };
				preDepthFramebufferSpec.DepthClearValue = 0.0f;
				preDepthFramebufferSpec.Transfer = true;

				Ref<Framebuffer> clearFramebuffer = Framebuffer::Create(preDepthFramebufferSpec);
				preDepthFramebufferSpec.ClearDepthOnLoad = false;
				preDepthFramebufferSpec.ExistingImages[1] = clearFramebuffer->GetDepthImage();
				Ref<Framebuffer> loadFramebuffer = Framebuffer::Create(preDepthFramebufferSpec);

				PipelineSpecification pipelineSpec;
				pipelineSpec.DebugName = preDepthFramebufferSpec.DebugName;
				pipelineSpec.TargetFramebuffer = clearFramebuffer;
				pipelineSpec.Shader = Renderer::GetShaderLibrary()->Get("PreDepth");
				pipelineSpec.Layout = vertexLayout;
				//pipelineSpec.InstanceLayout = instanceLayout;
				m_PreDepthPipeline = RasterPipeline::Create(pipelineSpec);
				m_PreDepthMaterial = Material::Create(pipelineSpec.Shader, pipelineSpec.DebugName.c_str());

				// Change to loading framebuffer so we don't clear
				pipelineSpec.TargetFramebuffer = loadFramebuffer;

				pipelineSpec.DebugName = "PreDepth-Anim";
				pipelineSpec.Shader = Renderer::GetShaderLibrary()->Get("PreDepth_Anim");
				pipelineSpec.BoneInfluenceLayout = boneInfluenceLayout;
				m_PreDepthPipelineAnim = RasterPipeline::Create(pipelineSpec);  // same renderpass as Predepth-Opaque

				pipelineSpec.DebugName = "PreDepth-Transparent";
				pipelineSpec.Shader = Renderer::GetShaderLibrary()->Get("PreDepth");
				preDepthFramebufferSpec.DebugName = pipelineSpec.DebugName;
				m_PreDepthTransparentPipeline = RasterPipeline::Create(pipelineSpec);

				// TODO(0x): Need PreDepth-Transparent-Animated pipeline

				// Static
				RenderPassSpecification preDepthRenderPassSpec;
				preDepthRenderPassSpec.DebugName = preDepthFramebufferSpec.DebugName;
				preDepthRenderPassSpec.Pipeline = m_PreDepthPipeline;

				m_PreDepthPass = RenderPass::Create(preDepthRenderPassSpec);
				m_PreDepthPass->SetInput("u_Camera", m_UBSCamera);
				//m_PreDepthPass->SetInput("u_ScreenData", m_UBSScreenData);
				m_PreDepthPass->SetInput("r_Transforms", m_SBSTransforms);
				BEY_CORE_VERIFY(m_PreDepthPass->Validate());
				m_PreDepthPass->Bake();

				// Animated
				preDepthRenderPassSpec.DebugName = "PreDepth-Anim";
				preDepthRenderPassSpec.Pipeline = m_PreDepthPipelineAnim;
				m_PreDepthAnimPass = RenderPass::Create(preDepthRenderPassSpec);
				m_PreDepthAnimPass->SetInput("u_Camera", m_UBSCamera);
				//m_PreDepthAnimPass->SetInput("u_ScreenData", m_UBSScreenData);
				m_PreDepthAnimPass->SetInput("r_BoneTransforms", m_SBSBoneTransforms);
				m_PreDepthAnimPass->SetInput("r_Transforms", m_SBSTransforms);
				BEY_CORE_VERIFY(m_PreDepthAnimPass->Validate());
				m_PreDepthAnimPass->Bake();

				// Transparent
				preDepthRenderPassSpec.DebugName = "PreDepth-Transparent";
				preDepthRenderPassSpec.Pipeline = m_PreDepthTransparentPipeline;
				m_PreDepthTransparentPass = RenderPass::Create(preDepthRenderPassSpec);
				m_PreDepthTransparentPass->SetInput("u_Camera", m_UBSCamera);
				m_PreDepthTransparentPass->SetInput("r_Transforms", m_SBSTransforms);
				//m_PreDepthTransparentPass->SetInput("u_ScreenData", m_UBSScreenData);

				BEY_CORE_VERIFY(m_PreDepthTransparentPass->Validate());
				m_PreDepthTransparentPass->Bake();
			}

			// Geometry
			{
				/*ImageSpecification imageSpec;
				imageSpec.Width = Application::Get().GetWindow().GetWidth();
				imageSpec.Height = Application::Get().GetWindow().GetHeight();
				imageSpec.Format = ImageFormat::RGBA32F;
				imageSpec.Usage = ImageUsage::Attachment;
				imageSpec.DebugName = "GeometryPass-ColorAttachment0";
				m_GeometryPassColorAttachmentImage = Image2D::Create(imageSpec);
				m_GeometryPassColorAttachmentImage->Invalidate();*/

				FramebufferSpecification geoFramebufferSpec;
				geoFramebufferSpec.Attachments = { ImageFormat::B10G11R11UFLOAT, ImageFormat::A2B10R11G11UNorm, ImageFormat::RGBA, ImageFormat::RGBA, ImageFormat::DEPTH32F };
				geoFramebufferSpec.ExistingImages[4] = m_PreDepthPass->GetDepthOutput();
				geoFramebufferSpec.Transfer = true;

				// Don't clear primary color attachment (skybox pass writes into it)
				geoFramebufferSpec.Attachments.Attachments[0].LoadOp = AttachmentLoadOp::Load;
				geoFramebufferSpec.Attachments.Attachments[1].LoadOp = AttachmentLoadOp::Load;
				// Don't blend with luminance in the alpha channel.
				geoFramebufferSpec.Attachments.Attachments[1].Blend = false;
				geoFramebufferSpec.ClearColor = { 0.0f, 0.0f, 0.0f, 1.0f };
				geoFramebufferSpec.DebugName = "Geometry";
				geoFramebufferSpec.ClearDepthOnLoad = false;
				Ref<Framebuffer> clearFramebuffer = Framebuffer::Create(geoFramebufferSpec);
				geoFramebufferSpec.ClearColorOnLoad = false;
				geoFramebufferSpec.ExistingImages[0] = clearFramebuffer->GetImage(0);
				geoFramebufferSpec.ExistingImages[1] = clearFramebuffer->GetImage(1);
				geoFramebufferSpec.ExistingImages[2] = clearFramebuffer->GetImage(2);
				geoFramebufferSpec.ExistingImages[3] = clearFramebuffer->GetImage(3);
				geoFramebufferSpec.ExistingImages[4] = m_PreDepthPass->GetDepthOutput();
				Ref<Framebuffer> loadFramebuffer = Framebuffer::Create(geoFramebufferSpec);

				PipelineSpecification pipelineSpecification;
				pipelineSpecification.DebugName = "PBR-Static";
				pipelineSpecification.Shader = Renderer::GetShaderLibrary()->Get("PBR_Static");
				pipelineSpecification.TargetFramebuffer = clearFramebuffer;
				pipelineSpecification.DepthOperator = DepthCompareOperator::Equal;
				pipelineSpecification.DepthWrite = false;
				pipelineSpecification.Layout = vertexLayout;
				//pipelineSpecification.InstanceLayout = instanceLayout;
				pipelineSpecification.LineWidth = m_LineWidth;
				m_GeometryPipeline = RasterPipeline::Create(pipelineSpecification);

				// Switch to load framebuffer to not clear subsequent passes
				pipelineSpecification.TargetFramebuffer = loadFramebuffer;

				//
				// Transparent Geometry
				//
				pipelineSpecification.DebugName = "PBR-Transparent";
				pipelineSpecification.Shader = Renderer::GetShaderLibrary()->Get("PBR_Transparent");
				pipelineSpecification.DepthOperator = DepthCompareOperator::GreaterOrEqual;
				m_TransparentGeometryPipeline = RasterPipeline::Create(pipelineSpecification);

				//
				// Animated Geometry
				//
				pipelineSpecification.DebugName = "PBR-Anim";
				pipelineSpecification.Shader = Renderer::GetShaderLibrary()->Get("PBR_Anim");
				pipelineSpecification.DepthOperator = DepthCompareOperator::Equal;
				pipelineSpecification.BoneInfluenceLayout = boneInfluenceLayout;
				m_GeometryPipelineAnim = RasterPipeline::Create(pipelineSpecification);

				// TODO(0x): Need Transparent-Animated geometry pipeline
			}


			// Selected Geometry isolation (for outline pass)
			{
				FramebufferSpecification framebufferSpec;
				framebufferSpec.DebugName = "SelectedGeometry";
				framebufferSpec.Attachments = { ImageFormat::A2B10R11G11UNorm, ImageFormat::Depth };
				framebufferSpec.ClearColor = { 0.0f, 0.0f, 0.0f, 0.0f };
				framebufferSpec.DepthClearValue = 1.0f;

				PipelineSpecification pipelineSpecification;
				pipelineSpecification.DebugName = framebufferSpec.DebugName;
				pipelineSpecification.TargetFramebuffer = Framebuffer::Create(framebufferSpec);
				pipelineSpecification.Shader = Renderer::GetShaderLibrary()->Get("SelectedGeometry");
				pipelineSpecification.Layout = vertexLayout;
				//pipelineSpecification.InstanceLayout = instanceLayout;
				pipelineSpecification.DepthOperator = DepthCompareOperator::LessOrEqual;

				RenderPassSpecification rpSpec;
				rpSpec.DebugName = "SelectedGeometry";
				rpSpec.Pipeline = RasterPipeline::Create(pipelineSpecification);
				m_SelectedGeometryPass = RenderPass::Create(rpSpec);
				m_SelectedGeometryPass->SetInput("u_Camera", m_UBSCamera);
				m_SelectedGeometryPass->SetInput("r_Transforms", m_SBSTransforms);
				BEY_CORE_VERIFY(m_SelectedGeometryPass->Validate());
				m_SelectedGeometryPass->Bake();

				m_SelectedGeometryMaterial = Material::Create(pipelineSpecification.Shader, pipelineSpecification.DebugName.c_str());

				pipelineSpecification.DebugName = "SelectedGeometry-Anim";
				pipelineSpecification.Shader = Renderer::GetShaderLibrary()->Get("SelectedGeometry_Anim");
				pipelineSpecification.BoneInfluenceLayout = boneInfluenceLayout;
				framebufferSpec.ExistingFramebuffer = m_SelectedGeometryPass->GetTargetFramebuffer();
				framebufferSpec.ClearColorOnLoad = false;
				framebufferSpec.ClearDepthOnLoad = false;
				pipelineSpecification.TargetFramebuffer = Framebuffer::Create(framebufferSpec);
				rpSpec.Pipeline = RasterPipeline::Create(pipelineSpecification);
				m_SelectedGeometryAnimPass = RenderPass::Create(rpSpec); // Note: same framebuffer and renderpass as m_SelectedGeometryPipeline
				m_SelectedGeometryAnimPass->SetInput("u_Camera", m_UBSCamera);
				m_SelectedGeometryAnimPass->SetInput("r_BoneTransforms", m_SBSBoneTransforms);
				m_SelectedGeometryAnimPass->SetInput("r_Transforms", m_SBSTransforms);
				BEY_CORE_VERIFY(m_SelectedGeometryAnimPass->Validate());
				m_SelectedGeometryAnimPass->Bake();
			}

			{
				RenderPassSpecification spec;
				spec.DebugName = "GeometryPass";
				spec.Pipeline = m_GeometryPipeline;

				m_GeometryPass = RenderPass::Create(spec);
				// geometryRenderPass->GetRequiredInputs(); // Returns list of sets+bindings of required resources from descriptor layout

				m_GeometryPass->SetInput("u_Camera", m_UBSCamera);
				m_GeometryPass->SetInput("r_Transforms", m_SBSTransforms);

				m_GeometryPass->SetInput("u_SpotLightMatrices", m_UBSSpotShadowData);
				m_GeometryPass->SetInput("u_DirShadow", m_UBSShadow);
				m_GeometryPass->SetInput("u_PointLights", m_UBSPointLights);
				m_GeometryPass->SetInput("u_SpotLights", m_UBSSpotLights);
				m_GeometryPass->SetInput("u_SceneData", m_UBSScene);
				//m_GeometryPass->SetInput("u_ScreenData", m_UBSScreenData);
				m_GeometryPass->SetInput("s_VisiblePointLightIndicesBuffer", m_SBSVisiblePointLightIndicesBuffer);
				m_GeometryPass->SetInput("s_VisibleSpotLightIndicesBuffer", m_SBSVisibleSpotLightIndicesBuffer);

				m_GeometryPass->SetInput("u_RendererData", m_UBSRendererData);

				// Some resources that are scene specific cannot be set before
				// SceneRenderer::BeginScene. As such these should be placeholders that
				// when invalidated are replaced with real resources
				m_GeometryPass->SetInput("u_EnvRadianceTex", Renderer::GetBlackCubeTexture());
				m_GeometryPass->SetInput("u_EnvIrradianceTex", Renderer::GetBlackCubeTexture());

				// Set 3
				m_GeometryPass->SetInput("u_BRDFLUTTexture", Renderer::GetBRDFLutTexture());

				// Some resources are the results of previous passes. This will enforce
				// a layout transition when RenderPass::Begin() is called. GetOutput(0)
				// refers to the first output of the pass - since the pass is depth-only,
				// this will be the depth image
				m_GeometryPass->SetInput("u_ShadowMapTexture", m_DirectionalShadowMapPass[0]->GetDepthOutput());
				m_GeometryPass->SetInput("u_SpotShadowTexture", m_SpotShadowPass->GetDepthOutput());

				// Note: outputs are automatically set by framebuffer

				// Bake will create descriptor sets and ensure everything is ready for rendering
				// If resources (eg. storage buffers/images) resize, passes need to be invalidated
				// so we can re-create proper descriptors to the newly created replacement resources
				BEY_CORE_VERIFY(m_GeometryPass->Validate());
				m_GeometryPass->Bake();
			}

			{
				RenderPassSpecification spec;
				spec.DebugName = "GeometryPass-Animated";
				spec.Pipeline = m_GeometryPipelineAnim;
				m_GeometryAnimPass = RenderPass::Create(spec);

				m_GeometryAnimPass->SetInput("u_Camera", m_UBSCamera);

				m_GeometryAnimPass->SetInput("u_SpotLightMatrices", m_UBSSpotShadowData);
				m_GeometryAnimPass->SetInput("u_DirShadow", m_UBSShadow);
				m_GeometryAnimPass->SetInput("u_PointLights", m_UBSPointLights);
				m_GeometryAnimPass->SetInput("u_SpotLights", m_UBSSpotLights);
				m_GeometryAnimPass->SetInput("u_SceneData", m_UBSScene);
				m_GeometryAnimPass->SetInput("s_VisiblePointLightIndicesBuffer", m_SBSVisiblePointLightIndicesBuffer);
				m_GeometryAnimPass->SetInput("s_VisibleSpotLightIndicesBuffer", m_SBSVisibleSpotLightIndicesBuffer);

				m_GeometryAnimPass->SetInput("r_BoneTransforms", m_SBSBoneTransforms);
				m_GeometryAnimPass->SetInput("r_Transforms", m_SBSTransforms);

				m_GeometryAnimPass->SetInput("u_RendererData", m_UBSRendererData);
				//m_GeometryAnimPass->SetInput("u_ScreenData", m_UBSScreenData);

				m_GeometryAnimPass->SetInput("u_EnvRadianceTex", Renderer::GetBlackCubeTexture());
				m_GeometryAnimPass->SetInput("u_EnvIrradianceTex", Renderer::GetBlackCubeTexture());

				// Set 3
				m_GeometryAnimPass->SetInput("u_BRDFLUTTexture", Renderer::GetBRDFLutTexture());

				m_GeometryAnimPass->SetInput("u_ShadowMapTexture", m_DirectionalShadowMapPass[0]->GetDepthOutput());
				m_GeometryAnimPass->SetInput("u_SpotShadowTexture", m_SpotShadowPass->GetDepthOutput());

				BEY_CORE_VERIFY(m_GeometryAnimPass->Validate());
				m_GeometryAnimPass->Bake();
			}


			// Path tracing and Raytracing
			if (VulkanContext::GetCurrentDevice()->IsRaytracingSupported())
			{
				{
					{
						ImageSpecification imageSpec;
						imageSpec.Usage = ImageUsage::Storage;
						imageSpec.Format = ImageFormat::B10G11R11UFLOAT;
						imageSpec.DebugName = "RaytracingAlbedoImage";
						m_AlbedoImage = Image2D::Create(imageSpec);
						m_AlbedoImage->Invalidate();

						imageSpec.DebugName = "DLSSImage";
						imageSpec.Transfer = true;
						m_DLSSImage = Image2D::Create(imageSpec);
						m_DLSSImage->Invalidate();

						imageSpec.Format = ImageFormat::RGBA32F;
						imageSpec.DebugName = "PathtracingAccumulation";
						m_AccumulationImage = Image2D::Create(imageSpec);
						m_AccumulationImage->Invalidate();

						//imageSpec.CreateBindlessDescriptor = true;
						imageSpec.Format = ImageFormat::RGBA32F;
						imageSpec.DebugName = "PrevPositionsHitT";
						m_PreviousPositionImage = Image2D::Create(imageSpec);
						m_PreviousPositionImage->Invalidate();
						//m_CurrentPositionImage = Image2D::Create(imageSpec);
					}

					ImageSpecification imageSpec;
					imageSpec.Usage = ImageUsage::Storage;
					imageSpec.Format = ImageFormat::B10G11R11UFLOAT;
					imageSpec.DebugName = "RaytracingStorageImage";
					imageSpec.Transfer = true;
					m_RaytracingImage = Image2D::Create(imageSpec);
					m_RaytracingImage->Invalidate();

					imageSpec.Format = ImageFormat::A2B10R11G11UNorm;
					imageSpec.DebugName = "RaytracingNormalsStorageImage";
					m_RaytracingNormalsImage = Image2D::Create(imageSpec);
					m_RaytracingNormalsImage->Invalidate();

					imageSpec.Transfer = false;
					imageSpec.Format = ImageFormat::RGBA;
					imageSpec.DebugName = "RaytracingMetalnessRoughnessStorageImage";
					m_RaytracingMetalnessRoughnessImage = Image2D::Create(imageSpec);
					m_RaytracingMetalnessRoughnessImage->Invalidate();

					imageSpec.Format = ImageFormat::RED16F;
					imageSpec.DebugName = "RaytracingPrimaryHitT";
					m_RaytracingPrimaryHitT = Image2D::Create(imageSpec);
					m_RaytracingPrimaryHitT->Invalidate();

					m_SceneTLAS = AccelerationStructureSet::Create(false, "Main TLAS", framesInFlight);

					StorageBufferSpecification objSpecBufferSpec;
					objSpecBufferSpec.DebugName = "RaytracingObjSpec";
					objSpecBufferSpec.GPUOnly = false;
					m_SBSObjectSpecs = StorageBufferSet::Create(objSpecBufferSpec, 1); // Resized later

					StorageBufferSpecification objMaterialBufferSpec;
					objMaterialBufferSpec.DebugName = "Raytracing Materials";
					objMaterialBufferSpec.GPUOnly = false;
					m_SBSMaterialBuffer = StorageBufferSet::Create(objMaterialBufferSpec, 1); // Resized later

					{
						Ref<Shader> rayShader = Renderer::GetShaderLibrary()->Get("Raytracing");

						m_RaytracingMaterial = Material::Create(rayShader, "Raytracing");

						RaytracingPassSpecification raytracingPassSpecification;
						raytracingPassSpecification.DebugName = "Raytracing";
						raytracingPassSpecification.Pipeline = RaytracingPipeline::Create(rayShader);
						m_RayTracingRenderPass = RaytracingPass::Create(raytracingPassSpecification);
						m_RayTracingRenderPass->SetInput("u_Camera", m_UBSCamera);
						m_RayTracingRenderPass->SetInput("o_Image", m_RaytracingImage);
						m_RayTracingRenderPass->SetInput("o_AlbedoColor", m_AlbedoImage);
						m_RayTracingRenderPass->SetInput("o_ViewNormalsLuminance", m_RaytracingNormalsImage);
						m_RayTracingRenderPass->SetInput("o_MetalnessRoughness", m_RaytracingMetalnessRoughnessImage);
						m_RayTracingRenderPass->SetInput("objDescs", m_SBSObjectSpecs);
						m_RayTracingRenderPass->SetInput("materials", m_SBSMaterialBuffer);
						m_RayTracingRenderPass->SetInput("TLAS", m_SceneTLAS);
						m_RayTracingRenderPass->SetInput("r_Transforms", m_SBSTransforms);


						m_RayTracingRenderPass->SetInput("u_PointLights", m_UBSPointLights);
						m_RayTracingRenderPass->SetInput("u_SpotLights", m_UBSSpotLights);
						m_RayTracingRenderPass->SetInput("u_SceneData", m_UBSScene);
						//m_RayTracingRenderPass->SetInput("s_VisiblePointLightIndicesBuffer", m_SBSVisiblePointLightIndicesBuffer);
						//m_RayTracingRenderPass->SetInput("s_VisibleSpotLightIndicesBuffer", m_SBSVisibleSpotLightIndicesBuffer);

						m_RayTracingRenderPass->SetInput("DebugImage", m_DebugImage);

						//m_RayTracingRenderPass->SetInput("u_RendererData", m_UBSRendererData);


						m_RayTracingRenderPass->SetInput("Samplers", Renderer::GetBilinearSampler(), 0);
						m_RayTracingRenderPass->SetInput("Samplers", Renderer::GetPointSampler(), 1);
						m_RayTracingRenderPass->SetInput("Samplers", Renderer::GetAnisoSampler(), 2);

						m_RayTracingRenderPass->SetInput("u_EnvRadianceTex", Renderer::GetBlackCubeTexture());
						m_RayTracingRenderPass->SetInput("u_EnvIrradianceTex", Renderer::GetBlackCubeTexture());

						// Set 3
						m_RayTracingRenderPass->SetInput("u_BRDFLUTTexture", Renderer::GetBRDFLutTexture());
						BEY_CORE_VERIFY(m_RayTracingRenderPass->Validate());
						m_RayTracingRenderPass->Bake();
					}

					{
						Ref<Shader> pathShader = Renderer::GetShaderLibrary()->Get("Pathtracing");
						m_PathtracingMaterial = Material::Create(pathShader, "Pathtracing");

						RaytracingPassSpecification raytracingPassSpecification;
						raytracingPassSpecification.DebugName = "Pathtracing";
						raytracingPassSpecification.Pipeline = RaytracingPipeline::Create(pathShader);
						m_PathTracingRenderPass = RaytracingPass::Create(raytracingPassSpecification);

						m_PathTracingRenderPass->SetInput("u_Camera", m_UBSCamera);
						m_PathTracingRenderPass->SetInput("o_Image", m_RaytracingImage);
						m_PathTracingRenderPass->SetInput("o_AlbedoColor", m_AlbedoImage);
						m_PathTracingRenderPass->SetInput("io_AccumulatedColor", m_AccumulationImage);
						m_PathTracingRenderPass->SetInput("o_ViewNormalsLuminance", m_RaytracingNormalsImage);
						//m_PathTracingRenderPass->SetInput("io_PrevWorldPositions", m_PreviousPositionImage);
						m_PathTracingRenderPass->SetInput("o_MetalnessRoughness", m_RaytracingMetalnessRoughnessImage);
						m_PathTracingRenderPass->SetInput("o_PrimaryHitT", m_RaytracingPrimaryHitT);
						m_PathTracingRenderPass->SetInput("objDescs", m_SBSObjectSpecs);
						m_PathTracingRenderPass->SetInput("materials", m_SBSMaterialBuffer);
						m_PathTracingRenderPass->SetInput("TLAS", m_SceneTLAS);
						m_PathTracingRenderPass->SetInput("u_ScreenData", m_UBSScreenData);
						m_PathTracingRenderPass->SetInput("r_Transforms", m_SBSTransforms);

						m_PathTracingRenderPass->SetInput("Samplers", Renderer::GetBilinearSampler(), 0);
						m_PathTracingRenderPass->SetInput("Samplers", Renderer::GetPointSampler(), 1);
						m_PathTracingRenderPass->SetInput("Samplers", Renderer::GetAnisoSampler(), 2);


						m_PathTracingRenderPass->SetInput("u_PointLights", m_UBSPointLights);
						m_PathTracingRenderPass->SetInput("u_SpotLights", m_UBSSpotLights);
						m_PathTracingRenderPass->SetInput("u_SceneData", m_UBSScene);
						//m_PathTracingRenderPass->SetInput("s_VisiblePointLightIndicesBuffer", m_SBSVisiblePointLightIndicesBuffer);
						//m_PathTracingRenderPass->SetInput("s_VisibleSpotLightIndicesBuffer", m_SBSVisibleSpotLightIndicesBuffer);

						m_PathTracingRenderPass->SetInput("DebugImage", m_DebugImage);


						//m_PathTracingRenderPass->SetInput("u_RendererData", m_UBSRendererData);

						m_PathTracingRenderPass->SetInput("u_EnvRadianceTex", Renderer::GetBlackCubeTexture());
						m_PathTracingRenderPass->SetInput("u_EnvIrradianceTex", Renderer::GetBlackCubeTexture());

						/*m_PathTracingRenderPass->SetInput("Samplers", Renderer::GetBilinearSampler(), 0);
						m_PathTracingRenderPass->SetInput("Samplers", Renderer::GetPointSampler(), 1);
						m_PathTracingRenderPass->SetInput("Samplers", Renderer::GetAnisoSampler(), 2);*/
						// Set 3
						m_PathTracingRenderPass->SetInput("u_BRDFLUTTexture", Renderer::GetBRDFLutTexture());
						BEY_CORE_VERIFY(m_PathTracingRenderPass->Validate());
						m_PathTracingRenderPass->Bake();
					}

					BEY_CORE_VERIFY(m_PathTracingRenderPass->Validate());
					m_PathTracingRenderPass->Bake();
					m_MainRaytracer = Raytracer::Create(m_SceneTLAS);
				}

				{
					Ref<Shader> restirShader = Renderer::GetShaderLibrary()->Get("Path-Restir");
					m_RestirMaterial = Material::Create(restirShader, "Path-Restir");

					RaytracingPassSpecification raytracingPassSpecification;
					raytracingPassSpecification.DebugName = "Path-Restir";
					raytracingPassSpecification.Pipeline = RaytracingPipeline::Create(restirShader);
					m_RestirRenderPass = RaytracingPass::Create(raytracingPassSpecification);

					m_RestirRenderPass->SetInput("u_Camera", m_UBSCamera);
					m_RestirRenderPass->SetInput("o_Image", m_RaytracingImage);
					m_RestirRenderPass->SetInput("o_AlbedoColor", m_AlbedoImage);
					m_RestirRenderPass->SetInput("io_AccumulatedColor", m_AccumulationImage);
					m_RestirRenderPass->SetInput("o_ViewNormalsLuminance", m_RaytracingNormalsImage);
					m_RestirRenderPass->SetInput("o_MetalnessRoughness", m_RaytracingMetalnessRoughnessImage);
					m_RestirRenderPass->SetInput("objDescs", m_SBSObjectSpecs);
					m_RestirRenderPass->SetInput("materials", m_SBSMaterialBuffer);
					m_RestirRenderPass->SetInput("TLAS", m_SceneTLAS);
					m_RestirRenderPass->SetInput("o_PrimaryHitT", m_RaytracingPrimaryHitT);
					m_RestirRenderPass->SetInput("r_Transforms", m_SBSTransforms);

					m_RestirRenderPass->SetInput("Samplers", Renderer::GetBilinearSampler(), 0);
					m_RestirRenderPass->SetInput("Samplers", Renderer::GetPointSampler(), 1);
					m_RestirRenderPass->SetInput("Samplers", Renderer::GetAnisoSampler(), 2);


					m_RestirRenderPass->SetInput("u_PointLights", m_UBSPointLights);
					m_RestirRenderPass->SetInput("u_SpotLights", m_UBSSpotLights);
					m_RestirRenderPass->SetInput("u_SceneData", m_UBSScene);
					//m_RestirRenderPass->SetInput("s_VisiblePointLightIndicesBuffer", m_SBSVisiblePointLightIndicesBuffer);
					//m_RestirRenderPass->SetInput("s_VisibleSpotLightIndicesBuffer", m_SBSVisibleSpotLightIndicesBuffer);

					m_RestirRenderPass->SetInput("DebugImage", m_DebugImage);


					//m_RestirRenderPass->SetInput("u_RendererData", m_UBSRendererData);

					m_RestirRenderPass->SetInput("u_EnvRadianceTex", Renderer::GetBlackCubeTexture());
					m_RestirRenderPass->SetInput("u_EnvIrradianceTex", Renderer::GetBlackCubeTexture());

					/*m_RestirRenderPass->SetInput("Samplers", Renderer::GetBilinearSampler(), 0);
					m_RestirRenderPass->SetInput("Samplers", Renderer::GetPointSampler(), 1);
					m_RestirRenderPass->SetInput("Samplers", Renderer::GetAnisoSampler(), 2);*/
					// Set 3
					m_RestirRenderPass->SetInput("u_BRDFLUTTexture", Renderer::GetBRDFLutTexture());
					BEY_CORE_VERIFY(m_RestirRenderPass->Validate());
					m_RestirRenderPass->Bake();

				}


				{
					Ref<Shader> restirShader = Renderer::GetShaderLibrary()->Get("Path-Restir-comp");
					m_RestirCompMaterial = Material::Create(restirShader, "Path-Restir-comp");

					ComputePassSpecification computePassSpecification;
					computePassSpecification.DebugName = "Path-Restir-Comp";
					computePassSpecification.Pipeline = PipelineCompute::Create(restirShader);
					m_RestirCompRenderPass = ComputePass::Create(computePassSpecification);

					m_RestirCompRenderPass->SetInput("u_Camera", m_UBSCamera);
					m_RestirCompRenderPass->SetInput("o_Image", m_RaytracingImage);
					m_RestirCompRenderPass->SetInput("o_AlbedoColor", m_AlbedoImage);
					m_RestirCompRenderPass->SetInput("io_AccumulatedColor", m_AccumulationImage);
					m_RestirCompRenderPass->SetInput("o_ViewNormalsLuminance", m_RaytracingNormalsImage);
					m_RestirCompRenderPass->SetInput("o_MetalnessRoughness", m_RaytracingMetalnessRoughnessImage);
					m_RestirCompRenderPass->SetInput("o_PrimaryHitT", m_RaytracingPrimaryHitT);

					m_RestirCompRenderPass->SetInput("objDescs", m_SBSObjectSpecs);
					m_RestirCompRenderPass->SetInput("materials", m_SBSMaterialBuffer);
					m_RestirCompRenderPass->SetInput("TLAS", m_SceneTLAS);

					m_RestirCompRenderPass->SetInput("Samplers", Renderer::GetBilinearSampler(), 0);
					m_RestirCompRenderPass->SetInput("Samplers", Renderer::GetPointSampler(), 1);
					m_RestirCompRenderPass->SetInput("Samplers", Renderer::GetAnisoSampler(), 2);


					m_RestirCompRenderPass->SetInput("u_PointLights", m_UBSPointLights);
					m_RestirCompRenderPass->SetInput("u_SpotLights", m_UBSSpotLights);
					m_RestirCompRenderPass->SetInput("u_SceneData", m_UBSScene);
					//m_RestirCompRenderPass->SetInput("s_VisiblePointLightIndicesBuffer", m_SBSVisiblePointLightIndicesBuffer);
					//m_RestirCompRenderPass->SetInput("s_VisibleSpotLightIndicesBuffer", m_SBSVisibleSpotLightIndicesBuffer);

					m_RestirCompRenderPass->SetInput("DebugImage", m_DebugImage);


					m_RestirCompRenderPass->SetInput("u_ScreenData", m_UBSScreenData);

					m_RestirCompRenderPass->SetInput("u_EnvRadianceTex", Renderer::GetBlackCubeTexture());
					m_RestirCompRenderPass->SetInput("u_EnvIrradianceTex", Renderer::GetBlackCubeTexture());


					// Set 3
					m_RestirCompRenderPass->SetInput("u_BRDFLUTTexture", Renderer::GetBRDFLutTexture());
					BEY_CORE_VERIFY(m_RestirCompRenderPass->Validate());
					m_RestirCompRenderPass->Bake();

				}


				//// DDGI
				//{
				//	Ref<Shader> shader = Renderer::GetShaderLibrary()->Get("DDGIRaytrace");

				//	RaytracingPassSpecification passSpecification;
				//	passSpecification.DebugName = "DDGI Raytracing";
				//	passSpecification.Pipeline = RaytracingPipeline::Create(shader);
				//	m_DDGIRayTracingRenderPass = RaytracingPass::Create(passSpecification);

				//	m_DDGIRayTracingRenderPass->SetInput("objDescs", m_SBSObjectSpecs);
				//	m_DDGIRayTracingRenderPass->SetInput("materials", m_SBSMaterialBuffer);
				//	m_DDGIRayTracingRenderPass->SetInput("TLAS", m_SceneTLAS);
				//	m_DDGIRayTracingRenderPass->SetInput("Samplers", Renderer::GetBilinearSampler(), 0);
				//	m_DDGIRayTracingRenderPass->SetInput("Samplers", Renderer::GetPointSampler(), 1);
				//	m_DDGIRayTracingRenderPass->SetInput("Samplers", Renderer::GetAnisoSampler(), 2);
				//	m_DDGIRayTracingRenderPass->SetInput("DebugImage", m_DebugImage);

				//	m_DDGIRayTracingRenderPass->SetInput("DDGIVolumes", m_SBDDGIConstants);
				//	m_DDGIRayTracingRenderPass->SetInput("DDGIVolumeBindless", m_SBDDGIReourceIndices);

				//	m_DDGIRayTracingRenderPass->SetInput("u_PointLights", m_UBSPointLights);
				//	m_DDGIRayTracingRenderPass->SetInput("u_SpotLights", m_UBSSpotLights);
				//	m_DDGIRayTracingRenderPass->SetInput("u_SceneData", m_UBSScene);


				//	BEY_CORE_VERIFY(m_DDGIRayTracingRenderPass->Validate());
				//	m_DDGIRayTracingRenderPass->Bake();
				//}

				// DDGI Vis
				//{

				//	std::string filePath = (Project::GetProjectDirectory() / std::filesystem::path("Assets/Meshes/Source/Default")).string();
				//	std::string targetFilePath = Project::GetProjectDirectory().string() + "/Assets/Meshes/Default/";
				//	if (std::filesystem::exists(filePath / std::filesystem::path("Sphere.gltf")))
				//	{
				//		AssetHandle assetHandle = Project::GetEditorAssetManager()->GetAssetHandleFromFilePath("Meshes/Source/Default/Sphere.gltf");
				//		Ref<Asset> asset = AssetManager::GetAsset<Asset>(assetHandle);
				//		if (asset)
				//		{
				//			m_SphereMesh = Ref<StaticMesh>::Create(AssetManager::GetAsset<MeshSource>(assetHandle), "DDGI Vis Sphere");


				//		}
				//	}
				//	else
				//		BEY_CONSOLE_LOG_WARN("Please import the default mesh source files to the following path: {0}", filePath);


				//	Ref<Shader> shader = Renderer::GetShaderLibrary()->Get("DDGIVis");
				//	m_DDGIVisTLAS = AccelerationStructureSet::Create(false, "Probe Vis TLAS", framesInFlight);
				//	m_DDGIVisRaytracer = Raytracer::Create(m_DDGIVisTLAS);

				//	RaytracingPassSpecification passSpecification;
				//	passSpecification.DebugName = "DDGI Vis";
				//	passSpecification.Pipeline = RaytracingPipeline::Create(shader);
				//	m_DDGIVisRenderPass = RaytracingPass::Create(passSpecification);

				//	m_DDGIVisRenderPass->SetInput("u_Camera", m_UBSCamera);
				//	m_DDGIVisRenderPass->SetInput("TLAS", m_DDGIVisTLAS);

				//	m_DDGIVisRenderPass->SetInput("Samplers", Renderer::GetBilinearSampler(), 0);
				//	m_DDGIVisRenderPass->SetInput("Samplers", Renderer::GetPointSampler(), 1);
				//	m_DDGIVisRenderPass->SetInput("Samplers", Renderer::GetAnisoSampler(), 2);

				//	m_DDGIVisRenderPass->SetInput("GBufferA", m_RaytracingImage);
				//	m_DDGIVisRenderPass->SetInput("GBufferB", m_PreDepthPass->GetDepthOutput());
				//	m_DDGIVisRenderPass->SetInput("DebugImage", m_DebugImage);

				//	m_DDGIVisRenderPass->SetInput("DDGIVolumes", m_SBDDGIConstants);
				//	m_DDGIVisRenderPass->SetInput("DDGIVolumeBindless", m_SBDDGIReourceIndices);

				//	BEY_CORE_VERIFY(m_DDGIVisRenderPass->Validate());
				//	m_DDGIVisRenderPass->Bake();
				//}

				// DDGI Vis Probe Update
				//{
				//	Ref<Shader>  shader = Renderer::GetShaderLibrary()->Get("DDGIProbeUpdate");
				//	m_VisProbeUpdatePipeline = PipelineCompute::Create(shader);

				//	ComputePassSpecification spec;
				//	spec.DebugName = "Vis Probe Update";
				//	spec.Pipeline = m_VisProbeUpdatePipeline;
				//	m_DDGIProbeUpdatePass = ComputePass::Create(spec);

				//	m_DDGIProbeUpdatePass->SetInput("DDGIVolumes", m_SBDDGIConstants);
				//	m_DDGIProbeUpdatePass->SetInput("DDGIVolumeBindless", m_SBDDGIReourceIndices);
				//	//m_DDGIProbeUpdatePass->SetInput("DebugImage", m_DebugImage);

				//	m_DDGIProbeUpdatePass->SetInput("RWTLASInstances", m_SBSDDGIProbeInstances);
				//	BEY_CORE_VERIFY(m_DDGIProbeUpdatePass->Validate());
				//	m_DDGIProbeUpdatePass->Bake();
				//}
			}

			{
				ComputePassSpecification spec;
				spec.DebugName = "LightCulling";
				spec.Pipeline = m_LightCullingPipeline;
				m_LightCullingPass = ComputePass::Create(spec);

				m_LightCullingPass->SetInput("u_Camera", m_UBSCamera);
				//m_LightCullingPass->SetInput("u_SceneData", m_UBSScene);
				m_LightCullingPass->SetInput("u_ScreenData", m_UBSScreenData);
				m_LightCullingPass->SetInput("u_PointLights", m_UBSPointLights);
				m_LightCullingPass->SetInput("u_SpotLights", m_UBSSpotLights);
				m_LightCullingPass->SetInput("s_VisiblePointLightIndicesBuffer", m_SBSVisiblePointLightIndicesBuffer);
				m_LightCullingPass->SetInput("s_VisibleSpotLightIndicesBuffer", m_SBSVisibleSpotLightIndicesBuffer);
				m_LightCullingPass->SetInput("u_DepthMap", m_PreDepthPass->GetDepthOutput());
				BEY_CORE_VERIFY(m_LightCullingPass->Validate());
				m_LightCullingPass->Bake();
			}

#if 0
			// Render example

			// BeginRenderPass needs to do the following:
			// - insert layout transitions for input resources that are required
			// - call vkCmdBeginRenderPass
			// - bind pipeline - one render pass is only allowed one pipeline, and
			//   the actual render pass should be able to contain it
			// - bind descriptor sets that are not per-draw-call, meaning Sets 1-3
			//     - this makes sense because they are "global" for entire render pass,
			//       so these can easily be bound up-front

			Renderer::BeginRenderPass(m_CommandBuffer, shadowMapRenderPass);

			// Render functions (which issue draw calls) should no longer be concerned with
			// binding the pipeline or descriptor sets 1-3, they only:
			// - bind vertex/index buffers
			// - bind descriptor set 0, which contains material resources
			// - push constants
			// - actual draw call

			Renderer::RenderStaticMeshWithMaterial(m_CommandBuffer, ...);
			Renderer::EndRenderPass(m_CommandBuffer);

			Renderer::BeginRenderPass(m_CommandBuffer, geometryRenderPass);
			Renderer::RenderStaticMesh(m_CommandBuffer, ...);
			Renderer::EndRenderPass(m_CommandBuffer);
#endif
		}



		// Hierarchical Z buffer
		{
			TextureSpecification spec;
			spec.Format = ImageFormat::RED32F;
			spec.SamplerWrap = TextureWrap::ClampToEdge;
			spec.SamplerFilter = TextureFilter::Nearest;
			spec.Compress = false;
			spec.DebugName = "HierarchicalZ";

			m_HierarchicalDepthTexture.Texture = Texture2D::Create(spec);

			Ref<Shader> shader = Renderer::GetShaderLibrary()->Get("HZB");

			ComputePassSpecification hdPassSpec;
			hdPassSpec.DebugName = "HierarchicalDepth";
			hdPassSpec.Pipeline = PipelineCompute::Create(shader);
			m_HierarchicalDepthPass = ComputePass::Create(hdPassSpec);

			BEY_CORE_VERIFY(m_HierarchicalDepthPass->Validate());
			m_HierarchicalDepthPass->Bake();
		}

		// Exposure image
		{
			{
				ImageSpecification spec;
				spec.Format = ImageFormat::RED16F;
				spec.Usage = ImageUsage::Storage;
				spec.DebugName = "Exposure Image";

				m_ExposureImage = Image2D::Create(spec);
				m_ExposureImage->Invalidate();
			}

			Ref<Shader> shader = Renderer::GetShaderLibrary()->Get("Exposure");

			ComputePassSpecification MotionVectorsPassSpec;
			MotionVectorsPassSpec.DebugName = "Exposure";
			MotionVectorsPassSpec.Pipeline = PipelineCompute::Create(shader);
			m_ExposurePass = ComputePass::Create(MotionVectorsPassSpec);

			//m_ExposurePass->SetInput("u_Camera", m_UBSCamera);
			//m_ExposurePass->SetInput("u_ScreenData", m_UBSScreenData);
			//m_ExposurePass->SetInput("u_DepthMap", m_PreDepthPass->GetDepthOutput());
			//m_ExposurePass->SetInput("u_ColorMap", m_->GetOutput(0));
			//m_ExposurePass->SetInput("o_ResolvedMotionVectors", m_ExposureImage);
			m_ExposurePass->SetInput("DebugImage", m_DebugImage);
			m_ExposurePass->SetInput("o_Exposure", m_ExposureImage);

			BEY_CORE_VERIFY(m_ExposurePass->Validate());
			m_ExposurePass->Bake();
		}

		// SSR Composite
		{
			ImageSpecification imageSpec;
			imageSpec.Format = ImageFormat::RGBA16F;
			imageSpec.Usage = ImageUsage::Storage;
			imageSpec.DebugName = "SSR";
			m_SSRImage = Image2D::Create(imageSpec);
			m_SSRImage->Invalidate();

			PipelineSpecification pipelineSpecification;
			pipelineSpecification.Layout = {
				{ ShaderDataType::Float3, "a_Position" },
				{ ShaderDataType::Float2, "a_TexCoord" },
			};
			pipelineSpecification.BackfaceCulling = false;
			pipelineSpecification.DepthTest = false;
			pipelineSpecification.DepthWrite = false;
			pipelineSpecification.DebugName = "SSR-Composite";
			auto shader = Renderer::GetShaderLibrary()->Get("SSR-Composite");
			pipelineSpecification.Shader = shader;

			FramebufferSpecification framebufferSpec;
			framebufferSpec.ClearColor = { 0.0f, 0.0f, 0.0f, 1.0f };
			framebufferSpec.Attachments.Attachments.emplace_back(ImageFormat::B10G11R11UFLOAT);
			framebufferSpec.ExistingImages[0] = m_GeometryPass->GetOutput(0);
			framebufferSpec.DebugName = "SSR-Composite";
			framebufferSpec.BlendMode = FramebufferBlendMode::SrcAlphaOneMinusSrcAlpha;
			framebufferSpec.ClearColorOnLoad = false;
			pipelineSpecification.TargetFramebuffer = Framebuffer::Create(framebufferSpec);

			RenderPassSpecification renderPassSpec;
			renderPassSpec.DebugName = "SSR-Composite";
			renderPassSpec.Pipeline = RasterPipeline::Create(pipelineSpecification);
			m_SSRCompositePass = RenderPass::Create(renderPassSpec);
			m_SSRCompositePass->SetInput("u_SSR", m_SSRImage);
			BEY_CORE_VERIFY(m_SSRCompositePass->Validate());
			m_SSRCompositePass->Bake();
		}

		// Pre-Integration
		{
			TextureSpecification spec;
			spec.Format = ImageFormat::RED8UN;
			spec.Compress = false;
			spec.DebugName = "Pre-Integration";

			m_PreIntegrationVisibilityTexture.Texture = Texture2D::Create(spec);

			Ref<Shader> shader = Renderer::GetShaderLibrary()->Get("Pre-Integration");
			ComputePassSpecification passSpec;
			passSpec.DebugName = "Pre-Integration";
			passSpec.Pipeline = PipelineCompute::Create(shader);
			m_PreIntegrationPass = ComputePass::Create(passSpec);
		}

		// Pre-convolution Compute
		{
			TextureSpecification spec;
			spec.Format = ImageFormat::B10G11R11UFLOAT;
			spec.SamplerWrap = TextureWrap::ClampToEdge;
			spec.DebugName = "Pre-Convoluted";
			spec.Storage = true;
			spec.Compress = false;
			m_PreConvolutedTexture.Texture = Texture2D::Create(spec);

			Ref<Shader> shader = Renderer::GetShaderLibrary()->Get("Pre-Convolution");
			ComputePassSpecification passSpec;
			passSpec.DebugName = "Pre-Integration";
			passSpec.Pipeline = PipelineCompute::Create(shader);
			m_PreConvolutionComputePass = ComputePass::Create(passSpec);
			BEY_CORE_VERIFY(m_PreConvolutionComputePass->Validate());
			m_PreConvolutionComputePass->Bake();
		}

		// Edge Detection
		if (m_Specification.EnableEdgeOutlineEffect)
		{
			FramebufferSpecification compFramebufferSpec;
			compFramebufferSpec.DebugName = "POST-EdgeDetection";
			compFramebufferSpec.ClearColor = { 0.5f, 0.1f, 0.1f, 1.0f };
			compFramebufferSpec.Attachments = { ImageFormat::B10G11R11UFLOAT, ImageFormat::Depth };
			compFramebufferSpec.Transfer = true;

			Ref<Framebuffer> framebuffer = Framebuffer::Create(compFramebufferSpec);

			PipelineSpecification pipelineSpecification;
			pipelineSpecification.Layout = {
				{ ShaderDataType::Float3, "a_Position" },
				{ ShaderDataType::Float2, "a_TexCoord" }
			};
			pipelineSpecification.BackfaceCulling = false;
			pipelineSpecification.Shader = Renderer::GetShaderLibrary()->Get("EdgeDetection");
			pipelineSpecification.TargetFramebuffer = framebuffer;
			pipelineSpecification.DebugName = compFramebufferSpec.DebugName;
			pipelineSpecification.DepthWrite = false;

			RenderPassSpecification renderPassSpec;
			renderPassSpec.DebugName = compFramebufferSpec.DebugName;
			renderPassSpec.Pipeline = RasterPipeline::Create(pipelineSpecification);
			m_EdgeDetectionPass = RenderPass::Create(renderPassSpec);
			m_EdgeDetectionPass->SetInput("u_ViewNormalsTexture", m_GeometryPass->GetOutput(1));
			m_EdgeDetectionPass->SetInput("u_DepthTexture", m_PreDepthPass->GetDepthOutput());
			m_EdgeDetectionPass->SetInput("u_Camera", m_UBSCamera);
			BEY_CORE_VERIFY(m_EdgeDetectionPass->Validate());
			m_EdgeDetectionPass->Bake();
		}

		// Composite
		{
			FramebufferSpecification compFramebufferSpec;
			compFramebufferSpec.DebugName = "SceneComposite";
			compFramebufferSpec.ClearColor = { 0.5f, 0.1f, 0.1f, 1.0f };
			compFramebufferSpec.Attachments = { ImageFormat::B10G11R11UFLOAT, ImageFormat::Depth };
			compFramebufferSpec.Transfer = true;
			compFramebufferSpec.ClearColorOnLoad = false;
			compFramebufferSpec.ClearDepthOnLoad = false;

			Ref<Framebuffer> framebuffer = Framebuffer::Create(compFramebufferSpec);

			PipelineSpecification pipelineSpecification;
			pipelineSpecification.Layout = {
				{ ShaderDataType::Float3, "a_Position" },
				{ ShaderDataType::Float2, "a_TexCoord" }
			};
			pipelineSpecification.BackfaceCulling = false;
			pipelineSpecification.Shader = Renderer::GetShaderLibrary()->Get("SceneComposite");
			pipelineSpecification.TargetFramebuffer = framebuffer;
			pipelineSpecification.DebugName = "SceneComposite";
			pipelineSpecification.DepthWrite = false;
			pipelineSpecification.DepthTest = false;

			RenderPassSpecification renderPassSpec;
			renderPassSpec.DebugName = "SceneComposite";
			renderPassSpec.Pipeline = RasterPipeline::Create(pipelineSpecification);
			m_CompositePass = RenderPass::Create(renderPassSpec);
			//m_CompositePass->SetInput("u_Texture", m_GeometryPass->GetOutput(0));
			m_CompositePass->SetInput("u_Texture", (!m_DLSSSettings.FakeDLSS || m_DLSSSettings.Enable) && VulkanContext::GetCurrentDevice()->IsDLSSSupported() ? m_DLSSImage : m_GeometryPass->GetOutput(0));
			m_CompositePass->SetInput("u_BloomTexture", m_BloomComputeTextures[2].Texture);
			m_CompositePass->SetInput("u_BloomDirtTexture", m_BloomDirtTexture);
			m_CompositePass->SetInput("u_DepthTexture", m_PreDepthPass->GetDepthOutput());
			m_CompositePass->SetInput("u_TransparentDepthTexture", m_PreDepthTransparentPass->GetDepthOutput());

			if (m_Specification.EnableEdgeOutlineEffect)
				m_CompositePass->SetInput("u_EdgeTexture", m_EdgeDetectionPass->GetOutput(0));

			m_CompositePass->SetInput("u_Camera", m_UBSCamera);

			BEY_CORE_VERIFY(m_CompositePass->Validate());
			m_CompositePass->Bake();
		}

		// DOF
		{
			FramebufferSpecification compFramebufferSpec;
			compFramebufferSpec.DebugName = "POST-DepthOfField";
			compFramebufferSpec.ClearColor = { 0.5f, 0.1f, 0.1f, 1.0f };
			compFramebufferSpec.Attachments = { ImageFormat::B10G11R11UFLOAT, ImageFormat::Depth };
			compFramebufferSpec.Transfer = true;

			Ref<Framebuffer> framebuffer = Framebuffer::Create(compFramebufferSpec);

			PipelineSpecification pipelineSpecification;
			pipelineSpecification.Layout = {
				{ ShaderDataType::Float3, "a_Position" },
				{ ShaderDataType::Float2, "a_TexCoord" }
			};
			pipelineSpecification.BackfaceCulling = false;
			pipelineSpecification.Shader = Renderer::GetShaderLibrary()->Get("DOF");
			pipelineSpecification.DebugName = compFramebufferSpec.DebugName;
			pipelineSpecification.DepthWrite = false;
			pipelineSpecification.TargetFramebuffer = framebuffer;
			m_DOFMaterial = Material::Create(pipelineSpecification.Shader, pipelineSpecification.DebugName.c_str());

			RenderPassSpecification renderPassSpec;
			renderPassSpec.DebugName = "POST-DOF";
			renderPassSpec.Pipeline = RasterPipeline::Create(pipelineSpecification);
			m_DOFPass = RenderPass::Create(renderPassSpec);
			m_DOFPass->SetInput("u_Texture", m_CompositePass->GetOutput(0));
			m_DOFPass->SetInput("u_DepthTexture", m_PreDepthPass->GetDepthOutput());
			m_DOFPass->SetInput("u_Camera", m_UBSCamera);
			BEY_CORE_VERIFY(m_DOFPass->Validate());
			m_DOFPass->Bake();
		}

		//FramebufferSpecification fbSpec;
		//fbSpec.Attachments = { ImageFormat::B10G11R11UFLOAT, ImageFormat::DEPTH32FSTENCIL8UINT };
		//fbSpec.ClearColorOnLoad = false;
		//fbSpec.ClearDepthOnLoad = false;
		//fbSpec.ExistingImages[0] = m_CompositePass->GetOutput(0);
		//fbSpec.ExistingImages[1] = m_PreDepthPass->GetDepthOutput();
		//fbSpec.DebugName = "Composite";
		//m_CompositingFramebuffer = Framebuffer::Create(fbSpec);

		// Wireframe
		{
			PipelineSpecification pipelineSpecification;
			pipelineSpecification.DebugName = "Wireframe";
			pipelineSpecification.TargetFramebuffer = m_CompositePass->GetTargetFramebuffer();
			pipelineSpecification.Shader = Renderer::GetShaderLibrary()->Get("Wireframe");
			pipelineSpecification.BackfaceCulling = false;
			pipelineSpecification.Wireframe = true;
			pipelineSpecification.LineWidth = 2.0f;
			pipelineSpecification.Layout = vertexLayout;
			//pipelineSpecification.InstanceLayout = instanceLayout;

			RenderPassSpecification renderPassSpec;
			renderPassSpec.DebugName = "Geometry-Wireframe";
			renderPassSpec.Pipeline = RasterPipeline::Create(pipelineSpecification);;
			m_GeometryWireframePass = RenderPass::Create(renderPassSpec);
			m_GeometryWireframePass->SetInput("u_Camera", m_UBSCamera);
			m_GeometryWireframePass->SetInput("r_Transforms", m_SBSTransforms);
			BEY_CORE_VERIFY(m_GeometryWireframePass->Validate());
			m_GeometryWireframePass->Bake();

			pipelineSpecification.DepthTest = false;
			pipelineSpecification.DebugName = "Wireframe-OnTop";
			renderPassSpec.Pipeline = RasterPipeline::Create(pipelineSpecification);
			renderPassSpec.DebugName = pipelineSpecification.DebugName;
			m_GeometryWireframeOnTopPass = RenderPass::Create(renderPassSpec);
			m_GeometryWireframeOnTopPass->SetInput("u_Camera", m_UBSCamera);
			m_GeometryWireframeOnTopPass->SetInput("r_Transforms", m_SBSTransforms);
			BEY_CORE_VERIFY(m_GeometryWireframeOnTopPass->Validate());
			m_GeometryWireframeOnTopPass->Bake();

			pipelineSpecification.DepthTest = true;
			pipelineSpecification.DebugName = "Wireframe-Anim";
			pipelineSpecification.Shader = Renderer::GetShaderLibrary()->Get("Wireframe_Anim");
			pipelineSpecification.BoneInfluenceLayout = boneInfluenceLayout;
			renderPassSpec.Pipeline = RasterPipeline::Create(pipelineSpecification); // Note: same framebuffer and renderpass as m_GeometryWireframePipeline
			renderPassSpec.DebugName = pipelineSpecification.DebugName;
			m_GeometryWireframeAnimPass = RenderPass::Create(renderPassSpec);
			m_GeometryWireframeAnimPass->SetInput("u_Camera", m_UBSCamera);
			m_GeometryWireframeAnimPass->SetInput("r_BoneTransforms", m_SBSBoneTransforms);
			m_GeometryWireframeAnimPass->SetInput("r_Transforms", m_SBSTransforms);
			BEY_CORE_VERIFY(m_GeometryWireframeAnimPass->Validate());
			m_GeometryWireframeAnimPass->Bake();

			pipelineSpecification.DepthTest = false;
			pipelineSpecification.DebugName = "Wireframe-Anim-OnTop";
			renderPassSpec.Pipeline = RasterPipeline::Create(pipelineSpecification);
			m_GeometryWireframeOnTopAnimPass = RenderPass::Create(renderPassSpec);
			m_GeometryWireframeOnTopAnimPass->SetInput("u_Camera", m_UBSCamera);
			m_GeometryWireframeOnTopAnimPass->SetInput("r_BoneTransforms", m_SBSBoneTransforms);
			m_GeometryWireframeOnTopAnimPass->SetInput("r_Transforms", m_SBSTransforms);
			BEY_CORE_VERIFY(m_GeometryWireframeOnTopAnimPass->Validate());
			m_GeometryWireframeOnTopAnimPass->Bake();
		}

		// Read-back Image
		if (false) // WIP
		{
			ImageSpecification spec;
			spec.Format = ImageFormat::B10G11R11UFLOAT;
			spec.Usage = ImageUsage::HostRead;
			spec.Transfer = true;
			spec.DebugName = "ReadBack";
			m_ReadBackImage = Image2D::Create(spec);
		}

		// Temporary framebuffers for re-use
		{
			FramebufferSpecification framebufferSpec;
			framebufferSpec.Attachments = { ImageFormat::A2B10R11G11UNorm };
			framebufferSpec.ClearColor = { 0.5f, 0.1f, 0.1f, 1.0f };
			framebufferSpec.BlendMode = FramebufferBlendMode::OneZero;
			framebufferSpec.DebugName = "Temporaries";

			for (uint32_t i = 0; i < 2; i++)
				m_TempFramebuffers.emplace_back(Framebuffer::Create(framebufferSpec));
		}

		// Jump Flood (outline)
		{
			PipelineSpecification pipelineSpecification;
			pipelineSpecification.DebugName = "JumpFlood-Init";
			pipelineSpecification.Shader = Renderer::GetShaderLibrary()->Get("JumpFlood_Init");
			pipelineSpecification.TargetFramebuffer = m_TempFramebuffers[0];
			pipelineSpecification.Layout = {
				{ ShaderDataType::Float3, "a_Position" },
				{ ShaderDataType::Float2, "a_TexCoord" }
			};
			m_JumpFloodInitMaterial = Material::Create(pipelineSpecification.Shader, pipelineSpecification.DebugName.c_str());

			RenderPassSpecification renderPassSpec;
			renderPassSpec.DebugName = "JumpFlood-Init";
			renderPassSpec.Pipeline = RasterPipeline::Create(pipelineSpecification);
			m_JumpFloodInitPass = RenderPass::Create(renderPassSpec);
			m_JumpFloodInitPass->SetInput("u_Texture", m_SelectedGeometryPass->GetOutput(0));
			BEY_CORE_VERIFY(m_JumpFloodInitPass->Validate());
			m_JumpFloodInitPass->Bake();

			const char* passName[2] = { "EvenPass", "OddPass" };
			for (uint32_t i = 0; i < 2; i++)
			{
				pipelineSpecification.DebugName = renderPassSpec.DebugName;
				pipelineSpecification.TargetFramebuffer = m_TempFramebuffers[(i + 1) % 2];
				pipelineSpecification.Shader = Renderer::GetShaderLibrary()->Get("JumpFlood_Pass");

				renderPassSpec.DebugName = fmt::eastl_format("JumpFlood-{0}", passName[i]);
				renderPassSpec.Pipeline = RasterPipeline::Create(pipelineSpecification);
				m_JumpFloodPass[i] = RenderPass::Create(renderPassSpec);
				m_JumpFloodPass[i]->SetInput("u_Texture", m_TempFramebuffers[i]->GetImage());
				BEY_CORE_VERIFY(m_JumpFloodPass[i]->Validate());
				m_JumpFloodPass[i]->Bake();

				m_JumpFloodPassMaterial[i] = Material::Create(pipelineSpecification.Shader, pipelineSpecification.DebugName.c_str());
			}

			// Outline compositing
			if (m_Specification.JumpFloodPass)
			{
				FramebufferSpecification fbSpec;
				fbSpec.Attachments = { ImageFormat::B10G11R11UFLOAT };
				fbSpec.ExistingImages[0] = m_CompositePass->GetOutput(0);
				fbSpec.ClearColorOnLoad = false;
				pipelineSpecification.TargetFramebuffer = Framebuffer::Create(fbSpec); // TODO: move this and skybox FB to be central, can use same

				pipelineSpecification.DebugName = "JumpFlood-Composite";
				pipelineSpecification.Shader = Renderer::GetShaderLibrary()->Get("JumpFlood_Composite");
				pipelineSpecification.DepthTest = false;
				renderPassSpec.Pipeline = RasterPipeline::Create(pipelineSpecification);
				m_JumpFloodCompositePass = RenderPass::Create(renderPassSpec);
				m_JumpFloodCompositePass->SetInput("u_Texture", m_TempFramebuffers[1]->GetImage());
				BEY_CORE_VERIFY(m_JumpFloodCompositePass->Validate());
				m_JumpFloodCompositePass->Bake();

				m_JumpFloodCompositeMaterial = Material::Create(pipelineSpecification.Shader, pipelineSpecification.DebugName.c_str());
			}
		}

		// Grid
		{
			PipelineSpecification pipelineSpec;
			pipelineSpec.DebugName = "Grid";
			pipelineSpec.Shader = Renderer::GetShaderLibrary()->Get("Grid");
			pipelineSpec.BackfaceCulling = false;
			pipelineSpec.Layout = {
				{ ShaderDataType::Float3, "a_Position" },
				{ ShaderDataType::Float2, "a_TexCoord" }
			};
			pipelineSpec.TargetFramebuffer = m_CompositePass->GetTargetFramebuffer();

			RenderPassSpecification renderPassSpec;
			renderPassSpec.DebugName = "Grid";
			renderPassSpec.Pipeline = RasterPipeline::Create(pipelineSpec);
			m_GridRenderPass = RenderPass::Create(renderPassSpec);
			m_GridRenderPass->SetInput("u_Camera", m_UBSCamera);
			BEY_CORE_VERIFY(m_GridRenderPass->Validate());
			m_GridRenderPass->Bake();

			const float gridScale = 16.025f;
			const float gridSize = 0.025f;
			m_GridMaterial = Material::Create(pipelineSpec.Shader, pipelineSpec.DebugName.c_str());
			m_GridMaterial->Set("u_Settings.Scale", gridScale);
			m_GridMaterial->Set("u_Settings.Size", gridSize);
		}

		// Collider
		m_SimpleColliderMaterial = Material::Create(Renderer::GetShaderLibrary()->Get("Wireframe"), "SimpleCollider");
		m_SimpleColliderMaterial->Set("u_MaterialUniforms.Color", m_Options.SimplePhysicsCollidersColor);
		m_ComplexColliderMaterial = Material::Create(Renderer::GetShaderLibrary()->Get("Wireframe"), "ComplexCollider");
		m_ComplexColliderMaterial->Set("u_MaterialUniforms.Color", m_Options.ComplexPhysicsCollidersColor);

		m_WireframeMaterial = Material::Create(Renderer::GetShaderLibrary()->Get("Wireframe"), "Wireframe");
		m_WireframeMaterial->Set("u_MaterialUniforms.Color", glm::vec4{ 1.0f, 0.5f, 0.0f, 1.0f });

		// Skybox
		{
			const auto skyboxShader = Renderer::GetShaderLibrary()->Get("Skybox");

			FramebufferSpecification fbSpec;
			fbSpec.Attachments = { ImageFormat::B10G11R11UFLOAT };
			fbSpec.ExistingImages[0] = m_GeometryPass->GetOutput(0);
			fbSpec.DebugName = "Sky box";
			Ref<Framebuffer> skyboxFB = Framebuffer::Create(fbSpec);

			PipelineSpecification pipelineSpec;
			pipelineSpec.DebugName = "Skybox";
			pipelineSpec.Shader = skyboxShader;
			pipelineSpec.DepthWrite = false;
			pipelineSpec.DepthTest = false;
			pipelineSpec.Layout = {
				{ ShaderDataType::Float3, "a_Position" },
				{ ShaderDataType::Float2, "a_TexCoord" }
			};
			pipelineSpec.TargetFramebuffer = skyboxFB;
			m_SkyboxPipeline = RasterPipeline::Create(pipelineSpec);
			m_SkyboxMaterial = Material::Create(pipelineSpec.Shader, pipelineSpec.DebugName.c_str());
			m_SkyboxMaterial->SetFlag(MaterialFlag::DepthTest, false);

			RenderPassSpecification renderPassSpec;
			renderPassSpec.DebugName = "Skybox";
			renderPassSpec.Pipeline = m_SkyboxPipeline;
			m_SkyboxPass = RenderPass::Create(renderPassSpec);
			m_SkyboxPass->SetInput("u_Camera", m_UBSCamera);
			BEY_CORE_VERIFY(m_SkyboxPass->Validate());
			m_SkyboxPass->Bake();
		}

		// TODO: resizeable/flushable
		const size_t TransformBufferCount = 10 * 1024; // 10240 transforms
		m_SubmeshTransformBuffers.resize(framesInFlight);
		for (uint32_t i = 0; i < framesInFlight; i++)
		{
			//m_SubmeshTransformBuffers[i].Buffer = VertexBuffer::Create(sizeof(TransformVertexData) * TransformBufferCount, fmt::format("SceneRenderer m_SubmeshTransformBuffers frame : {}", i));
			m_SubmeshTransformBuffers[i].Data = hnew TransformVertexData[TransformBufferCount];
		}

		// TODO: resizeable/flushable
		m_TransformBuffers.resize(framesInFlight);
		for (uint32_t i = 0; i < framesInFlight; i++)
		{
			m_TransformBuffers[i].Data = hnew TransformData[TransformBufferCount];
		}

		Renderer::Submit([instance = Ref(this)]() mutable { instance->m_ResourcesCreatedGPU = true; });

		InitOptions();

		////////////////////////////////////////
		// COMPUTE
		////////////////////////////////////////

		// GTAO
		{
			{
				ImageSpecification imageSpec;
				if (m_Options.GTAOBentNormals)
					imageSpec.Format = ImageFormat::RED32UI;
				else
					imageSpec.Format = ImageFormat::RED8UI;
				imageSpec.Usage = ImageUsage::Storage;
				imageSpec.DebugName = "GTAO";
				m_GTAOOutputImage = Image2D::Create(imageSpec);
				m_GTAOOutputImage->Invalidate();
			}

			// GTAO-Edges
			{
				ImageSpecification imageSpec;
				imageSpec.Format = ImageFormat::RED8UN;
				imageSpec.Usage = ImageUsage::Storage;
				imageSpec.DebugName = "GTAO-Edges";
				m_GTAOEdgesOutputImage = Image2D::Create(imageSpec);
				m_GTAOEdgesOutputImage->Invalidate();
			}

			Ref<Shader> shader = Renderer::GetShaderLibrary()->Get("GTAO");

			ComputePassSpecification spec;
			spec.DebugName = "GTAO-ComputePass";
			spec.Pipeline = PipelineCompute::Create(shader);
			m_GTAOComputePass = ComputePass::Create(spec);
			m_GTAOComputePass->SetInput("u_HiZDepth", m_HierarchicalDepthTexture.Texture);
			m_GTAOComputePass->SetInput("u_HilbertLut", Renderer::GetHilbertLut());
			m_GTAOComputePass->SetInput("u_ViewNormal", m_GeometryPass->GetOutput(1));
			m_GTAOComputePass->SetInput("o_AOwBentNormals", m_GTAOOutputImage);
			m_GTAOComputePass->SetInput("o_Edges", m_GTAOEdgesOutputImage);

			m_GTAOComputePass->SetInput("u_Camera", m_UBSCamera);
			m_GTAOComputePass->SetInput("u_ScreenData", m_UBSScreenData);
			BEY_CORE_VERIFY(m_GTAOComputePass->Validate());
			m_GTAOComputePass->Bake();
		}

		// GTAO Denoise
		{
			{
				ImageSpecification imageSpec;
				if (m_Options.GTAOBentNormals)
					imageSpec.Format = ImageFormat::RED32UI;
				else
					imageSpec.Format = ImageFormat::RED8UI;
				imageSpec.Usage = ImageUsage::Storage;
				imageSpec.DebugName = "GTAO-Denoise";
				m_GTAODenoiseImage = Image2D::Create(imageSpec);
				m_GTAODenoiseImage->Invalidate();

				Ref<Shader> shader = Renderer::GetShaderLibrary()->Get("GTAO-Denoise");
				m_GTAODenoiseMaterial[0] = Material::Create(shader, "GTAO-Denoise-Ping");
				m_GTAODenoiseMaterial[1] = Material::Create(shader, "GTAO-Denoise-Pong");

				ComputePassSpecification spec;
				spec.DebugName = "GTAO-Denoise";
				spec.Pipeline = PipelineCompute::Create(shader);

				m_GTAODenoisePass[0] = ComputePass::Create(spec);
				m_GTAODenoisePass[0]->SetInput("u_Edges", m_GTAOEdgesOutputImage);
				m_GTAODenoisePass[0]->SetInput("u_AOTerm", m_GTAOOutputImage);
				m_GTAODenoisePass[0]->SetInput("o_AOTerm", m_GTAODenoiseImage);
				m_GTAODenoisePass[0]->SetInput("u_ScreenData", m_UBSScreenData);
				BEY_CORE_VERIFY(m_GTAODenoisePass[0]->Validate());
				m_GTAODenoisePass[0]->Bake();

				m_GTAODenoisePass[1] = ComputePass::Create(spec);
				m_GTAODenoisePass[1]->SetInput("u_Edges", m_GTAOEdgesOutputImage);
				m_GTAODenoisePass[1]->SetInput("u_AOTerm", m_GTAODenoiseImage);
				m_GTAODenoisePass[1]->SetInput("o_AOTerm", m_GTAOOutputImage);
				m_GTAODenoisePass[1]->SetInput("u_ScreenData", m_UBSScreenData);
				BEY_CORE_VERIFY(m_GTAODenoisePass[1]->Validate());
				m_GTAODenoisePass[1]->Bake();
			}

			// GTAO Composite
			{
				PipelineSpecification aoCompositePipelineSpec;
				aoCompositePipelineSpec.DebugName = "AO-Composite";
				FramebufferSpecification framebufferSpec;
				framebufferSpec.DebugName = "AO-Composite";
				framebufferSpec.Attachments = { ImageFormat::B10G11R11UFLOAT };
				framebufferSpec.ExistingImages[0] = m_GeometryPass->GetOutput(0);
				framebufferSpec.ClearColorOnLoad = false;
				framebufferSpec.BlendMode = FramebufferBlendMode::Zero_SrcColor;

				aoCompositePipelineSpec.TargetFramebuffer = Framebuffer::Create(framebufferSpec);
				aoCompositePipelineSpec.DepthTest = false;
				aoCompositePipelineSpec.Layout = {
					{ ShaderDataType::Float3, "a_Position" },
					{ ShaderDataType::Float2, "a_TexCoord" },
				};
				aoCompositePipelineSpec.Shader = Renderer::GetShaderLibrary()->Get("AO-Composite");

				// Create RenderPass
				RenderPassSpecification renderPassSpec;
				renderPassSpec.DebugName = "AO-Composite";
				renderPassSpec.Pipeline = RasterPipeline::Create(aoCompositePipelineSpec);
				m_AOCompositePass = RenderPass::Create(renderPassSpec);
				m_AOCompositePass->SetInput("u_GTAOTex", m_GTAOOutputImage);
				BEY_CORE_VERIFY(m_AOCompositePass->Validate());
				m_AOCompositePass->Bake();

				m_AOCompositeMaterial = Material::Create(aoCompositePipelineSpec.Shader, "GTAO-Composite");
			}
		}

		m_GTAOFinalImage = m_Options.GTAODenoisePasses && m_Options.GTAODenoisePasses % 2 != 0 ? m_GTAODenoiseImage : m_GTAOOutputImage;

		// SSR
		{
			Ref<Shader> shader = Renderer::GetShaderLibrary()->Get("SSR");

			ComputePassSpecification spec;
			spec.DebugName = "SSR-Compute";
			spec.Pipeline = PipelineCompute::Create(shader);
			m_SSRPass = ComputePass::Create(spec);
			m_SSRPass->SetInput("outColor", m_SSRImage);
			m_SSRPass->SetInput("u_InputColor", m_PreConvolutedTexture.Texture);
			m_SSRPass->SetInput("u_VisibilityBuffer", m_PreIntegrationVisibilityTexture.Texture);
			m_SSRPass->SetInput("u_HiZBuffer", m_HierarchicalDepthTexture.Texture);
			m_SSRPass->SetInput("u_Normal", m_GeometryPass->GetOutput(1));
			m_SSRPass->SetInput("u_MetalnessRoughness", m_GeometryPass->GetOutput(2));
			m_SSRPass->SetInput("u_GTAOTex", m_GTAOFinalImage);
			m_SSRPass->SetInput("u_Camera", m_UBSCamera);
			m_SSRPass->SetInput("u_ScreenData", m_UBSScreenData);
			BEY_CORE_VERIFY(m_SSRPass->Validate());
			m_SSRPass->Bake();
		}

		// DDGI Irradiance
		//{
		//	Ref<Shader>  shader = Renderer::GetShaderLibrary()->Get("DDGIIrradiance");
		//	m_DDGIIrradiancePipeline = PipelineCompute::Create(shader);

		//	ImageSpecification imageSpec;
		//	imageSpec.Usage = ImageUsage::Storage;
		//	imageSpec.Format = ImageFormat::B10G11R11UFLOAT;
		//	imageSpec.DebugName = "DDGI Output Storage Image";
		//	m_DDGIOutputImage = Image2D::Create(imageSpec);
		//	m_DDGIOutputImage->Invalidate();

		//	ComputePassSpecification spec;
		//	spec.DebugName = "DDGI Irradiance";
		//	spec.Pipeline = m_DDGIIrradiancePipeline;
		//	m_DDGIIrradiancePass = ComputePass::Create(spec);

		//	m_DDGIIrradiancePass->SetInput("DDGIVolumes", m_SBDDGIConstants);
		//	m_DDGIIrradiancePass->SetInput("DDGIVolumeBindless", m_SBDDGIReourceIndices);
		//	m_DDGIIrradiancePass->SetInput("u_SceneData", m_UBSScene);
		//	m_DDGIIrradiancePass->SetInput("u_Camera", m_UBSCamera);
		//	m_DDGIIrradiancePass->SetInput("u_ScreenData", m_UBSScreenData);
		//	m_DDGIIrradiancePass->SetInput("Samplers", Renderer::GetBilinearSampler(), 0);
		//	m_DDGIIrradiancePass->SetInput("Samplers", Renderer::GetPointSampler(), 1);
		//	m_DDGIIrradiancePass->SetInput("Samplers", Renderer::GetAnisoSampler(), 2);
		//	m_DDGIIrradiancePass->SetInput("AlbedoTexture", m_AlbedoImage);
		//	m_DDGIIrradiancePass->SetInput("DepthTexture", m_HierarchicalDepthTexture.Texture);
		//	m_DDGIIrradiancePass->SetInput("NormalTexture", m_RaytracingNormalsImage);
		//	m_DDGIIrradiancePass->SetInput("OutputTexture", m_DDGIOutputImage);
		//	m_DDGIIrradiancePass->SetInput("DebugImage", m_DebugImage);
		//	BEY_CORE_VERIFY(m_DDGIIrradiancePass->Validate());
		//	m_DDGIIrradiancePass->Bake();
		//}

		// DDGI TexVis
		//{
		//	Ref<Shader>  shader = Renderer::GetShaderLibrary()->Get("DDGITexVis");
		//	m_DDGITexVisPipeline = PipelineCompute::Create(shader);

		//	ComputePassSpecification spec;
		//	spec.DebugName = "DDGI TexVis";
		//	spec.Pipeline = m_DDGITexVisPipeline;
		//	m_DDGITexVisPass = ComputePass::Create(spec);

		//	m_DDGITexVisPass->SetInput("DDGIVolumes", m_SBDDGIConstants);
		//	m_DDGITexVisPass->SetInput("DDGIVolumeBindless", m_SBDDGIReourceIndices);
		//	//m_DDGITexVisPass->SetInput("u_SceneData", m_UBSScene);
		//	//m_DDGITexVisPass->SetInput("u_Camera", m_UBSCamera);
		//	//m_DDGITexVisPass->SetInput("u_ScreenData", m_UBSScreenData);
		//	m_DDGITexVisPass->SetInput("Samplers", Renderer::GetBilinearSampler(), 0);
		//	m_DDGITexVisPass->SetInput("Samplers", Renderer::GetPointSampler(), 1);
		//	m_DDGITexVisPass->SetInput("Samplers", Renderer::GetAnisoSampler(), 2);
		//	m_DDGITexVisPass->SetInput("OutputTexture", m_DDGIOutputImage);
		//	//m_DDGITexVisPass->SetInput("DebugImage", m_DebugImage);
		//	BEY_CORE_VERIFY(m_DDGITexVisPass->Validate());
		//	m_DDGITexVisPass->Bake();
		//}

	}

	void SceneRenderer::Shutdown()
	{
		hdelete[] m_BoneTransformsData;
		for (auto& transformBuffer : m_SubmeshTransformBuffers)
			hdelete[] transformBuffer.Data;
	}

	void SceneRenderer::InitOptions()
	{
		//TODO: Deserialization?
		if (m_Options.EnableGTAO)
			*(int*)&m_Options.ReflectionOcclusionMethod |= (int)ShaderDef::AOMethod::GTAO;

		// OVERRIDE
		m_Options.ReflectionOcclusionMethod = ShaderDef::AOMethod::None;

		// Special macros are strictly starting with "__BEY_"
		Renderer::SetGlobalMacroInShaders("__BEY_REFLECTION_OCCLUSION_METHOD", fmt::format("{}", (int)m_Options.ReflectionOcclusionMethod));
		//Renderer::SetGlobalMacroInShaders("__BEY_GTAO_COMPUTE_BENT_NORMALS", fmt::format("{}", (int)m_Options.GTAOBentNormals));
	}

	void SceneRenderer::InsertGPUPerfMarker(Ref<RenderCommandBuffer> renderCommandBuffer, const eastl::string& label, const glm::vec4& markerColor)
	{
		Renderer::Submit([=]
		{
			Renderer::RT_InsertGPUPerfMarker(renderCommandBuffer, label, markerColor);
		});
	}

	void SceneRenderer::BeginGPUPerfMarker(Ref<RenderCommandBuffer> renderCommandBuffer, const eastl::string& label, const glm::vec4& markerColor)
	{
		Renderer::Submit([=]
		{
			Renderer::RT_BeginGPUPerfMarker(renderCommandBuffer, label, markerColor);
		});
	}

	void SceneRenderer::EndGPUPerfMarker(Ref<RenderCommandBuffer> renderCommandBuffer)
	{
		Renderer::Submit([=]
		{
			Renderer::RT_EndGPUPerfMarker(renderCommandBuffer);
		});
	}

	void SceneRenderer::SetScene(Ref<Scene> scene)
	{
		BEY_CORE_ASSERT(!m_Active, "Can't change scenes while rendering");
		m_Scene = scene;
	}

	void SceneRenderer::SetViewportSize(uint32_t width, uint32_t height)
	{
		width = (uint32_t)(width * m_Specification.Tiering.RendererScale);
		height = (uint32_t)(height * m_Specification.Tiering.RendererScale);

		if (m_TargetWidth != width || m_TargetHeight != height)
		{
			m_TargetWidth = width;
			m_TargetHeight = height;
			m_InvTargetWidth = 1.f / (float)width;
			m_InvTargetHeight = 1.f / (float)height;
			m_NeedsResize = true;
		}
	}

	// Some other settings are directly set in gui
	void SceneRenderer::UpdateGTAOData()
	{
		CBGTAOData& gtaoData = GTAODataCB;
		gtaoData.NDCToViewMul_x_PixelSize = { CameraDataUB.NDCToViewMul * (gtaoData.HalfRes ? m_ScreenDataUB.InvHalfResolution : m_ScreenDataUB.InvFullResolution) };
		gtaoData.HZBUVFactor = m_SSROptions.HZBUvFactor;
		gtaoData.ShadowTolerance = m_Options.AOShadowTolerance;
	}

	void SceneRenderer::PrepareDDGIVolumes()
	{
		Renderer::SetDDGIResources(m_SBDDGIConstants, m_SBDDGIReourceIndices);
		Renderer::InitDDGI(m_MainCommandBuffer, m_SceneData.SceneLightEnvironment.DDGIVolumes);
		Renderer::UpdateBindlessDescriptorSet(false);
	}

	void SceneRenderer::BeginScene(const SceneRendererCamera& camera, Timestep ts)
	{
		BEY_PROFILE_FUNC();

		BEY_CORE_ASSERT(m_Scene);
		BEY_CORE_ASSERT(!m_Active);
		m_Active = true;

		if (m_ResourcesCreatedGPU)
			m_ResourcesCreated = true;

		if (!m_ResourcesCreated)
			return;

		m_TimeStep = ts;

		m_GTAOFinalImage = m_Options.GTAODenoisePasses && m_Options.GTAODenoisePasses % 2 != 0 ? m_GTAODenoiseImage : m_GTAOOutputImage;
		// TODO: enable if shader uses this: m_SSRPass->SetInput("u_GTAOTex", m_GTAOFinalImage);

		m_SceneData.SceneCamera = camera;
		m_SceneData.SceneEnvironment = m_Scene->m_Environment;
		m_SceneData.SceneEnvironmentIntensity = m_Scene->m_EnvironmentIntensity;
		//m_SceneData.ActiveLight = m_Scene->m_Light;
		m_SceneData.SceneLightEnvironment = m_Scene->m_LightEnvironment;
		m_SceneData.SkyboxLod = m_Scene->m_SkyboxLod;

		m_GeometryPass->SetInput("u_EnvRadianceTex", m_SceneData.SceneEnvironment->RadianceMap);
		m_GeometryPass->SetInput("u_EnvIrradianceTex", m_SceneData.SceneEnvironment->IrradianceMap);

		m_GeometryAnimPass->SetInput("u_EnvRadianceTex", m_SceneData.SceneEnvironment->RadianceMap);
		m_GeometryAnimPass->SetInput("u_EnvIrradianceTex", m_SceneData.SceneEnvironment->IrradianceMap);

		if (VulkanContext::GetCurrentDevice()->IsRaytracingSupported())
		{
			m_RayTracingRenderPass->SetInput("u_EnvRadianceTex", m_SceneData.SceneEnvironment->RadianceMap);
			m_RayTracingRenderPass->SetInput("u_EnvIrradianceTex", m_SceneData.SceneEnvironment->IrradianceMap);

			m_PathTracingRenderPass->SetInput("u_EnvIrradianceTex", m_SceneData.SceneEnvironment->IrradianceMap);
			m_PathTracingRenderPass->SetInput("u_EnvRadianceTex", m_SceneData.SceneEnvironment->RadianceMap);

			m_RestirRenderPass->SetInput("u_EnvIrradianceTex", m_SceneData.SceneEnvironment->IrradianceMap);
			m_RestirRenderPass->SetInput("u_EnvRadianceTex", m_SceneData.SceneEnvironment->RadianceMap);

			m_RestirCompRenderPass->SetInput("u_EnvIrradianceTex", m_SceneData.SceneEnvironment->IrradianceMap);
			m_RestirCompRenderPass->SetInput("u_EnvRadianceTex", m_SceneData.SceneEnvironment->RadianceMap);

			uint32_t ddgVolumeCount = glm::max((uint32_t)m_SceneData.SceneLightEnvironment.DDGIVolumes.size(), 1u);

			m_SBDDGIConstants->RT_Resize(sizeof(rtxgi::DDGIVolumeDescGPU) * ddgVolumeCount * Renderer::GetConfig().FramesInFlight);
			m_SBDDGIReourceIndices->RT_Resize(sizeof(rtxgi::DDGIVolumeResourceIndices) * ddgVolumeCount * Renderer::GetConfig().FramesInFlight);
		}

		Renderer::UpdateBindlessDescriptorSet(false);

		if (m_NeedsResize)
		{
			m_NeedsResize = false;

			m_RaytracerReset = true;

			if (m_DLSSSettings.Enable)
			{
				auto optimal = m_DLSS->GetOptimalSettings(m_TargetWidth, m_TargetHeight, m_DLSSSettings.Mode);
				m_RenderWidth = optimal.x;
				m_RenderHeight = optimal.y;
				m_InvRenderWidth = 1.0f / (float)optimal.x;
				m_InvRenderHeight = 1.0f / (float)optimal.y;
			}
			else
			{
				m_RenderWidth = m_TargetWidth;
				m_RenderHeight = m_TargetHeight;
				m_InvRenderWidth = 1.0f / (float)m_TargetWidth;
				m_InvRenderHeight = 1.0f / (float)m_TargetHeight;
			}

			const glm::uvec2 renderSize = { m_RenderWidth, m_RenderHeight };
			const glm::uvec2 viewportSize = { m_TargetWidth, m_TargetHeight };

			if (m_DLSSSettings.Enable)
			{
				m_DLSSImage->Resize({ m_TargetWidth, m_TargetHeight });
				m_DLSS->CreateDLSS(nullptr, m_DLSSSettings);
			}

			m_ScreenSpaceProjectionMatrix = glm::ortho(0.0f, (float)m_RenderWidth, 0.0f, (float)m_RenderHeight);

			m_ScreenDataUB.FullResolution = { m_RenderWidth, m_RenderHeight };
			m_ScreenDataUB.InvFullResolution = { m_InvRenderWidth,  m_InvRenderHeight };
			m_ScreenDataUB.HalfResolution = glm::ivec2{ m_RenderWidth,  m_RenderHeight } / 2;
			m_ScreenDataUB.InvHalfResolution = { m_InvRenderWidth * 2.0f,  m_InvRenderHeight * 2.0f };

			// Both Pre-depth and geometry framebuffers need to be resized first.
			// Note the _Anim variants of these pipelines share the same framebuffer
			m_PreDepthPass->GetTargetFramebuffer()->Resize(m_RenderWidth, m_RenderHeight);
			m_PreDepthAnimPass->GetTargetFramebuffer()->Resize(m_RenderWidth, m_RenderHeight);
			m_CompositePass->GetTargetFramebuffer()->Resize(m_TargetWidth, m_TargetHeight); // Must be resized after pre-depth

			//m_PreDepthTransparentPass->GetTargetFramebuffer()->Resize(m_TargetWidth, m_TargetHeight);
			m_GeometryPass->GetTargetFramebuffer()->Resize(m_RenderWidth, m_RenderHeight);
			m_SkyboxPass->GetTargetFramebuffer()->Resize(m_RenderWidth, m_RenderHeight);
			m_GeometryAnimPass->GetTargetFramebuffer()->Resize(m_RenderWidth, m_RenderHeight);
			//m_SkyboxPass->GetTargetFramebuffer()->Resize(m_RenderWidth, m_RenderHeight);
			m_SelectedGeometryPass->GetTargetFramebuffer()->Resize(m_RenderWidth, m_RenderHeight);
			m_SelectedGeometryAnimPass->GetTargetFramebuffer()->Resize(m_RenderWidth, m_RenderHeight);
			//m_GeometryPassColorAttachmentImage->Resize({ m_RenderWidth, m_RenderHeight });
			if (VulkanContext::GetCurrentDevice()->IsRaytracingSupported())
			{
				m_RaytracingImage->Resize({ m_RenderWidth, m_RenderHeight });
				m_RaytracingNormalsImage->Resize({ m_RenderWidth, m_RenderHeight });
				m_PreviousPositionImage->Resize({ m_RenderWidth, m_RenderHeight });
				m_RaytracingMetalnessRoughnessImage->Resize({ m_RenderWidth, m_RenderHeight });
				m_RaytracingPrimaryHitT->Resize({ m_RenderWidth, m_RenderHeight });
				m_ExposureImage->Resize({ 1, 1 });
				m_AlbedoImage->Resize({ m_RenderWidth, m_RenderHeight });
				m_AccumulationImage->Resize({ m_RenderWidth, m_RenderHeight });
			}
			m_DebugImage->Resize({ m_RenderWidth, m_RenderHeight });
			//m_DDGIOutputImage->Resize({ m_RenderWidth, m_RenderHeight });

			// Dependent on Geometry 
			m_SSRCompositePass->GetTargetFramebuffer()->Resize(m_RenderWidth, m_RenderHeight);

			m_PreIntegrationVisibilityTexture.Texture->Resize(m_RenderWidth, m_RenderHeight);
			m_AOCompositePass->GetTargetFramebuffer()->Resize(m_RenderWidth, m_RenderHeight);

			//m_CompositingFramebuffer->Resize(m_TargetWidth, m_TargetHeight);
			//m_GridRenderPass->GetTargetFramebuffer()->Resize(m_TargetWidth, m_TargetHeight);

			if (m_JumpFloodCompositePass)
				m_JumpFloodCompositePass->GetTargetFramebuffer()->Resize(m_TargetWidth, m_TargetHeight);

			if (m_DOFPass)
				m_DOFPass->GetTargetFramebuffer()->Resize(m_TargetWidth, m_TargetHeight);

			if (m_ReadBackImage)
				m_ReadBackImage->Resize({ m_RenderWidth, m_RenderHeight });


			// HZB
			{
				//HZB size must be power of 2's
				const glm::uvec2 numMips = glm::ceil(glm::log2(glm::vec2(renderSize)));
				m_SSROptions.NumDepthMips = glm::max(numMips.x, numMips.y);

				const glm::uvec2 hzbSize = BIT(numMips);
				m_HierarchicalDepthTexture.Texture->Resize(hzbSize.x, hzbSize.y);

				const glm::vec2 hzbUVFactor = { (glm::vec2)renderSize / (glm::vec2)hzbSize };
				m_SSROptions.HZBUvFactor = hzbUVFactor;

				// Image Views (per-mip)
				ImageViewSpecification imageViewSpec;
				uint32_t mipCount = m_HierarchicalDepthTexture.Texture->GetMipLevelCount();
				m_HierarchicalDepthTexture.ImageViews.resize(mipCount);
				for (uint32_t mip = 0; mip < mipCount; mip++)
				{
					imageViewSpec.DebugName = fmt::eastl_format("HierarchicalDepthTexture-{}", mip);
					imageViewSpec.Image = m_HierarchicalDepthTexture.Texture->GetImage();
					imageViewSpec.Mip = mip;
					m_HierarchicalDepthTexture.ImageViews[mip] = ImageView::Create(imageViewSpec);
				}

				CreateHZBPassMaterials();
			}

			// Pre-Integration
			{
				// Image Views (per-mip)
				ImageViewSpecification imageViewSpec;
				uint32_t mipCount = m_PreIntegrationVisibilityTexture.Texture->GetMipLevelCount();
				m_PreIntegrationVisibilityTexture.ImageViews.resize(mipCount);
				for (uint32_t mip = 0; mip < mipCount - 1; mip++)
				{
					imageViewSpec.DebugName = fmt::eastl_format("PreIntegrationVisibilityTexture-{}", mip);
					imageViewSpec.Image = m_PreIntegrationVisibilityTexture.Texture->GetImage();
					imageViewSpec.Mip = mip + 1; // Start from mip 1 not 0
					m_PreIntegrationVisibilityTexture.ImageViews[mip] = ImageView::Create(imageViewSpec);
				}

				CreatePreIntegrationPassMaterials();
			}

			// Light culling
			{
				constexpr uint32_t TILE_SIZE = 16u;
				glm::uvec2 size = renderSize;
				size += TILE_SIZE - renderSize % TILE_SIZE;
				m_LightCullingWorkGroups = { size / TILE_SIZE, 1 };
				RendererDataUB.TilesCountX = m_LightCullingWorkGroups.x;

				m_SBSVisiblePointLightIndicesBuffer->Resize(m_LightCullingWorkGroups.x * m_LightCullingWorkGroups.y * 4 * 1024);
				m_SBSVisibleSpotLightIndicesBuffer->Resize(m_LightCullingWorkGroups.x * m_LightCullingWorkGroups.y * 4 * 1024);
			}

			// GTAO
			{
				glm::uvec2 gtaoSize = GTAODataCB.HalfRes ? (renderSize + 1u) / 2u : renderSize;
				glm::uvec2 denoiseSize = gtaoSize;
				const ImageFormat gtaoImageFormat = m_Options.GTAOBentNormals ? ImageFormat::RED32UI : ImageFormat::RED8UI;
				m_GTAOOutputImage->GetSpecification().Format = gtaoImageFormat;
				m_GTAODenoiseImage->GetSpecification().Format = gtaoImageFormat;

				m_GTAOOutputImage->Resize(gtaoSize);
				m_GTAODenoiseImage->Resize(gtaoSize);
				m_GTAOEdgesOutputImage->Resize(gtaoSize);

				constexpr uint32_t WORK_GROUP_SIZE = 16u;
				gtaoSize += WORK_GROUP_SIZE - gtaoSize % WORK_GROUP_SIZE;
				m_GTAOWorkGroups.x = gtaoSize.x / WORK_GROUP_SIZE;
				m_GTAOWorkGroups.y = gtaoSize.y / WORK_GROUP_SIZE;

				constexpr uint32_t DENOISE_WORK_GROUP_SIZE = 8u;
				denoiseSize += DENOISE_WORK_GROUP_SIZE - denoiseSize % DENOISE_WORK_GROUP_SIZE;
				m_GTAODenoiseWorkGroups.x = (denoiseSize.x + 2u * DENOISE_WORK_GROUP_SIZE - 1u) / (DENOISE_WORK_GROUP_SIZE * 2u); // 2 horizontal pixels at a time.
				m_GTAODenoiseWorkGroups.y = denoiseSize.y / DENOISE_WORK_GROUP_SIZE;
			}

			//SSR
			{
				constexpr uint32_t WORK_GROUP_SIZE = 8u;
				glm::uvec2 ssrSize = m_SSROptions.HalfRes ? (renderSize + 1u) / 2u : renderSize;
				m_SSRImage->Resize(ssrSize);

				ssrSize += WORK_GROUP_SIZE - ssrSize % WORK_GROUP_SIZE;
				m_SSRWorkGroups.x = ssrSize.x / WORK_GROUP_SIZE;
				m_SSRWorkGroups.y = ssrSize.y / WORK_GROUP_SIZE;

				// Pre-Convolution
				m_PreConvolutedTexture.Texture->Resize(ssrSize.x, ssrSize.y);

				// Image Views (per-mip)
				ImageViewSpecification imageViewSpec;
				imageViewSpec.DebugName = fmt::eastl_format("PreConvolutionCompute");
				uint32_t mipCount = m_PreConvolutedTexture.Texture->GetMipLevelCount();
				m_PreConvolutedTexture.ImageViews.resize(mipCount);
				for (uint32_t mip = 0; mip < mipCount; mip++)
				{
					imageViewSpec.Image = m_PreConvolutedTexture.Texture->GetImage();
					imageViewSpec.Mip = mip;
					m_PreConvolutedTexture.ImageViews[mip] = ImageView::Create(imageViewSpec);
				}

				// Re-setup materials with new image views
				CreatePreConvolutionPassMaterials();
			}

			// Bloom
			{
				glm::uvec2 bloomSize = (viewportSize + 1u) / 2u;
				bloomSize += m_BloomComputeWorkgroupSize - bloomSize % m_BloomComputeWorkgroupSize;

				m_BloomComputeTextures[0].Texture->Resize(bloomSize);
				m_BloomComputeTextures[1].Texture->Resize(bloomSize);
				m_BloomComputeTextures[2].Texture->Resize(bloomSize);

				// Image Views (per-mip)
				ImageViewSpecification imageViewSpec;
				for (int i = 0; i < 3; i++)
				{
					imageViewSpec.DebugName = fmt::eastl_format("BloomCompute-{}", i);
					uint32_t mipCount = m_BloomComputeTextures[i].Texture->GetMipLevelCount();
					m_BloomComputeTextures[i].ImageViews.resize(mipCount);
					for (uint32_t mip = 0; mip < mipCount; mip++)
					{
						imageViewSpec.Image = m_BloomComputeTextures[i].Texture->GetImage();
						imageViewSpec.Mip = mip;
						m_BloomComputeTextures[i].ImageViews[mip] = ImageView::Create(imageViewSpec);
					}
				}

				// Re-setup materials with new image views
				CreateBloomPassMaterials();
			}

			for (auto& tempFB : m_TempFramebuffers)
				tempFB->Resize(m_TargetWidth, m_TargetHeight);

			{ // A workaround to prevent device lost error because dercriptor set manager not always updating descriptors

				m_LightCullingPass->Bake();
				m_GeometryPass->Bake();
				m_GeometryAnimPass->Bake();
				m_JumpFloodInitPass->Bake();
				m_JumpFloodPass[0]->Bake();
				m_JumpFloodPass[1]->Bake();
				m_JumpFloodCompositePass->Bake();
				m_CompositePass->Bake();
				m_GTAOComputePass->Bake();
				m_GTAODenoisePass[0]->Bake();
				m_GTAODenoisePass[1]->Bake();
			}

			// TODO: if (m_ExternalCompositeRenderPass)
			// TODO: 	m_ExternalCompositeRenderPass->GetSpecification().TargetFramebuffer->Resize(m_RenderWidth, m_RenderHeight);
		}


		// Update uniform buffers
		UBCamera& cameraData = CameraDataUB;
		UBScene& sceneData = SceneDataUB;
		UBShadow& shadowData = ShadowData;
		UBRendererData& rendererData = RendererDataUB;
		UBPointLights& pointLightData = PointLightsUB;
		UBScreenData& screenData = m_ScreenDataUB;
		UBSpotLights& spotLightData = SpotLightUB;
		UBSpotShadowData& spotShadowData = SpotShadowDataUB;

		auto& sceneCamera = m_SceneData.SceneCamera;

		if (!m_DLSSSettings.UseQuadrants)
		{
			// Only use projectionMat here, because it's jittered outside the camera!!!!!!!!!!!!!!
			uint32_t numPhases = glm::max(1u, (uint32_t)(m_DLSSSettings.BasePhases * glm::pow(((float)m_TargetWidth + 1.0f) / (float)m_RenderWidth, 2.0f)));
			m_CurrentJitter = m_DLSSSettings.EnableJitter && m_DLSSSettings.Enable ? glm::vec2(GetCurrentPixelOffset(m_AccumulatedFrames % numPhases)) : glm::vec2();
		}
		else
		{
			glm::vec2 quadrants[4] = { {-0.25f, -0.25f}, {0.25f, -0.25f}, {-0.25f, 0.25f}, {0.25f, 0.25f} };
			uint32_t index = m_DLSSSettings.Quadrant < 4 ? m_DLSSSettings.Quadrant : m_AccumulatedFrames;
			m_CurrentJitter = quadrants[index % 4];
		}

		glm::vec3 translationVec(
			2.0f * m_CurrentJitter.x / (m_ScreenDataUB.FullResolution.x),
			2.0f * m_CurrentJitter.y / (m_ScreenDataUB.FullResolution.y),
			0.0f
		);
		const glm::mat4& projectionMat = glm::translate(glm::mat4(1.0f), translationVec) * sceneCamera.Camera->GetProjectionMatrix();

		const auto viewProjection = projectionMat * sceneCamera.ViewMatrix;
		const glm::mat4 viewInverse = glm::inverse(sceneCamera.ViewMatrix);
		const glm::mat4 projectionInverse = glm::inverse(projectionMat);
		const glm::vec3 cameraPosition = viewInverse[3];
		cameraData.PrevViewProjection = std::exchange(cameraData.ViewProjection, viewProjection);
		//cameraData.ViewProjection = viewProjection;
		cameraData.Projection = projectionMat;
		cameraData.InverseProjection = projectionInverse;
		cameraData.PrevView = std::exchange(cameraData.View, sceneCamera.ViewMatrix);
		//cameraData.View = sceneCamera.ViewMatrix;
		cameraData.InverseView = viewInverse;
		cameraData.InverseViewProjection = viewInverse * cameraData.InverseProjection;
		cameraData.ReprojectionMatrix = cameraData.ViewProjection * (viewInverse * projectionInverse);
		cameraData.PreviousJitter = std::exchange(cameraData.CurrentJitter, m_CurrentJitter);
		cameraData.ClipToRenderTargetScale = glm::vec2(0.5f * m_ScreenDataUB.FullResolution.x, -0.5f * m_ScreenDataUB.FullResolution.y);
		m_PreviousInvViewProjection = cameraData.InverseViewProjection;

		float depthLinearizeMul = (-cameraData.Projection[3][2]);     // float depthLinearizeMul = ( clipFar * clipNear ) / ( clipFar - clipNear );
		float depthLinearizeAdd = (cameraData.Projection[2][2]);     // float depthLinearizeAdd = clipFar / ( clipFar - clipNear );
		// correct the handedness issue.
		if (depthLinearizeMul * depthLinearizeAdd < 0)
			depthLinearizeAdd = -depthLinearizeAdd;
		cameraData.DepthUnpackConsts = { depthLinearizeMul, depthLinearizeAdd };
		const float* P = glm::value_ptr(projectionMat);
		const glm::vec4 projInfoPerspective = {
				 2.0f / (P[4 * 0 + 0]),                  // (x) * (R - L)/N
				 2.0f / (P[4 * 1 + 1]),                  // (y) * (T - B)/N
				-(1.0f - P[4 * 2 + 0]) / P[4 * 0 + 0],  // L/N
				-(1.0f + P[4 * 2 + 1]) / P[4 * 1 + 1],  // B/N
		};
		float tanHalfFOVY = 1.0f / cameraData.Projection[1][1];    // = tanf( drawContext.Camera.GetYFOV( ) * 0.5f );
		float tanHalfFOVX = 1.0f / cameraData.Projection[0][0];    // = tanHalfFOVY * drawContext.Camera.GetAspect( );
		cameraData.CameraTanHalfFOV = { tanHalfFOVX, tanHalfFOVY };
		cameraData.NDCToViewMul = { projInfoPerspective[0], projInfoPerspective[1] };
		cameraData.NDCToViewAdd = { projInfoPerspective[2], projInfoPerspective[3] };

		Ref<SceneRenderer> instance = this;
		Renderer::Submit([instance, cameraData]() mutable
		{
			instance->m_UBSCamera->RT_Get()->RT_SetData(&cameraData, sizeof(cameraData));
		});

		const auto& lightEnvironment = m_SceneData.SceneLightEnvironment;
		const std::vector<PointLight>& pointLightsVec = lightEnvironment.PointLights;
		pointLightData.Count = int(pointLightsVec.size());
		std::memcpy(pointLightData.PointLights, pointLightsVec.data(), lightEnvironment.GetPointLightsSize()); //(Karim) Do we really have to copy that?
		Renderer::Submit([instance, &pointLightData]() mutable
		{
			Ref<UniformBuffer> uniformBuffer = instance->m_UBSPointLights->RT_Get();
			uniformBuffer->RT_SetData(&pointLightData, 16ull + sizeof(PointLight) * pointLightData.Count);
		});

		const std::vector<SpotLight>& spotLightsVec = lightEnvironment.SpotLights;
		spotLightData.Count = int(spotLightsVec.size());
		std::memcpy(spotLightData.SpotLights, spotLightsVec.data(), lightEnvironment.GetSpotLightsSize()); //(Karim) Do we really have to copy that?
		Renderer::Submit([instance, &spotLightData]() mutable
		{
			Ref<UniformBuffer> uniformBuffer = instance->m_UBSSpotLights->RT_Get();
			uniformBuffer->RT_SetData(&spotLightData, 16ull + sizeof(SpotLight) * spotLightData.Count);
		});

		for (size_t i = 0; i < spotLightsVec.size(); ++i)
		{
			auto& light = spotLightsVec[i];
			if (!light.CastsShadows)
				continue;

			glm::mat4 projection = glm::perspective(glm::radians(light.Angle), 1.f, 0.1f, light.Range);
			// NOTE: ShadowMatrices[0] because we only support ONE shadow casting spot light at the moment and it MUST be index 0
			spotShadowData.ShadowMatrices[0] = projection * glm::lookAt(light.Position, light.Position - light.Direction, glm::vec3(0.0f, 1.0f, 0.0f));
		}

		Renderer::Submit([instance, spotShadowData, spotLightsVec]() mutable
		{
			Ref<UniformBuffer> uniformBuffer = instance->m_UBSSpotShadowData->RT_Get();
			uniformBuffer->RT_SetData(&spotShadowData, (uint32_t)(sizeof(glm::mat4) * spotLightsVec.size()));
		});

		const auto& directionalLight = m_SceneData.SceneLightEnvironment.DirectionalLights[0];
		sceneData.Lights = directionalLight;

		sceneData.PrevCameraPosition = std::exchange(sceneData.CameraPosition, cameraPosition);
		sceneData.FrameIndex = m_AccumulatedFrames;
		sceneData.MipBias = 0.0f + glm::log2((float)m_RenderWidth / (float)m_TargetWidth) - 1.0f + glm::epsilon<float>();
		//sceneData.CameraPosition = cameraPosition;
		sceneData.EnvironmentMapLod = m_SceneData.SkyboxLod;
		sceneData.EnvironmentMapIntensity = m_SceneData.SceneEnvironmentIntensity;
		Renderer::Submit([instance, sceneData]() mutable
		{
			instance->m_UBSScene->RT_Get()->RT_SetData(&sceneData, sizeof(sceneData));
		});

		if (m_Options.EnableGTAO)
			UpdateGTAOData();

		Renderer::Submit([instance, screenData]() mutable
		{
			instance->m_UBSScreenData->RT_Get()->RT_SetData(&screenData, sizeof(screenData));
		});

		CascadeData cascades[4];
		if (m_UseManualCascadeSplits)
			CalculateCascadesManualSplit(cascades, sceneCamera, directionalLight.Direction);
		else
			CalculateCascades(cascades, sceneCamera, directionalLight.Direction);


		// TODO: four cascades for now
		for (int i = 0; i < 4; i++)
		{
			CascadeSplits[i] = cascades[i].SplitDepth;
			shadowData.ViewProjection[i] = cascades[i].ViewProj;
		}
		Renderer::Submit([instance, shadowData]() mutable
		{
			instance->m_UBSShadow->RT_Get()->RT_SetData(&shadowData, sizeof(shadowData));
		});

		rendererData.CascadeSplits = CascadeSplits;
		Renderer::Submit([instance, rendererData]() mutable
		{
			instance->m_UBSRendererData->RT_Get()->RT_SetData(&rendererData, sizeof(rendererData));
		});
	}

	void SceneRenderer::EndScene()
	{
		BEY_PROFILE_FUNC();

		BEY_CORE_ASSERT(m_Active);
#if MULTI_THREAD
		Ref<SceneRenderer> instance = this;
		s_ThreadPool.emplace_back(([instance]() mutable
		{
			instance->FlushDrawList();
		}));
#else 
		FlushDrawList();
#endif

		m_Active = false;
	}

	void SceneRenderer::WaitForThreads()
	{
		for (uint32_t i = 0; i < s_ThreadPool.size(); i++)
			s_ThreadPool[i].join();

		s_ThreadPool.clear();
	}

	void SceneRenderer::SubmitToRaytracer(const DrawCommand& dc, const MaterialAsset* material, const glm::mat3x4& transform)
	{
		if (VulkanContext::GetCurrentDevice()->IsRaytracingSupported())
			m_MainRaytracer->AddDrawCommand(dc, material, transform);
	}

	void SceneRenderer::SubmitToRaytracer(const StaticDrawCommand& dc, const MaterialAsset* material, const glm::mat3x4& transform)
	{
		if (VulkanContext::GetCurrentDevice()->IsRaytracingSupported())
			m_MainRaytracer->AddDrawCommand(dc, material, transform);
	}

	void SceneRenderer::SubmitMesh(Ref<Mesh> mesh, uint32_t submeshIndex, Ref<MaterialTable> materialTable, const glm::mat4& transform, const std::vector<glm::mat4>& boneTransforms, Ref<Material> overrideMaterial)
	{
		BEY_PROFILE_FUNC();
		BEY_SCOPE_PERF("SceneRenderer::SubmitMesh");
		// TODO: Culling, sorting, etc.

		const auto meshSource = mesh->GetMeshSource().Raw();
		if (!mesh->IsReady())
			return;

		const auto& submeshes = meshSource->GetSubmeshes();
		const auto& submesh = submeshes[submeshIndex];
		uint32_t materialIndex = submesh.MaterialIndex;
		bool isRigged = submesh.IsRigged;

		AssetHandle materialHandle = materialTable->HasMaterial(materialIndex) ? materialTable->GetMaterial(materialIndex) : mesh->GetMaterials()->GetMaterial(materialIndex);
		const Ref<MaterialAsset>& material = AssetManager::GetAsset<MaterialAsset>(materialHandle);

		MeshKey meshKey = { mesh->Handle, materialHandle, submeshIndex, false };

		TransformMapData& meshTransform = m_MeshTransformMap[meshKey];
		TransformVertexData& transformStorage = meshTransform.Transforms.emplace_back();

		transformStorage.MRow[0] = { transform[0][0], transform[1][0], transform[2][0], transform[3][0] };
		transformStorage.MRow[1] = { transform[0][1], transform[1][1], transform[2][1], transform[3][1] };
		transformStorage.MRow[2] = { transform[0][2], transform[1][2], transform[2][2], transform[3][2] };

		if (isRigged)
		{
			CopyToBoneTransformStorage(meshKey, meshSource, boneTransforms);
		}
		// Main geo
		bool isTransparent = material->IsBlended();
		auto& destDrawList = !isTransparent ? m_DrawList : m_TransparentDrawList;
		auto& dc = destDrawList[meshKey];
		{
			dc.Mesh = mesh.Raw();
			dc.SubmeshIndex = submeshIndex;
			dc.MaterialTable = materialTable.Raw();
			dc.OverrideMaterial = overrideMaterial.Raw();
			dc.InstanceCount++;
			dc.IsRigged = isRigged;  // TODO: would it be better to have separate draw list for rigged meshes, or this flag is OK?
		}

		// Shadow pass
		if (material->IsShadowCasting())
		{
			auto& dc = m_ShadowPassDrawList[meshKey];
			dc.Mesh = mesh.Raw();
			dc.SubmeshIndex = submeshIndex;
			dc.MaterialTable = materialTable.Raw();
			dc.OverrideMaterial = overrideMaterial.Raw();
			dc.InstanceCount++;
			dc.IsRigged = isRigged;
		}
		SubmitToRaytracer(dc, material.Raw(), *reinterpret_cast<glm::mat3x4*>(&transformStorage));
	}

	void SceneRenderer::SubmitStaticMesh(Ref<StaticMesh> staticMesh, Ref<MaterialTable> materialTable, const glm::mat4& transform, Ref<Material> overrideMaterial)
	{
		BEY_PROFILE_FUNC();
		BEY_SCOPE_PERF("SceneRenderer::SubmitStaticMesh");

		const auto meshSource = staticMesh->GetMeshSource().Raw();
		if (!staticMesh->IsReady())
			return;
		const auto& submeshData = meshSource->GetSubmeshes();
		for (uint32_t submeshIndex : staticMesh->GetSubmeshes())
		{
			glm::mat4 submeshTransform = transform * submeshData[submeshIndex].Transform;

			const auto& submeshes = staticMesh->GetMeshSource()->GetSubmeshes();
			uint32_t materialIndex = submeshes[submeshIndex].MaterialIndex;

			AssetHandle materialHandle = materialTable->HasMaterial(materialIndex) ? materialTable->GetMaterial(materialIndex) : staticMesh->GetMaterials()->GetMaterial(materialIndex);
			BEY_CORE_VERIFY(materialHandle);
			Ref<MaterialAsset> material = AssetManager::GetAsset<MaterialAsset>(materialHandle);

			MeshKey meshKey = { staticMesh->Handle, materialHandle, submeshIndex, false };
			TransformMapData& meshTransform = m_MeshTransformMap[meshKey];
			TransformVertexData& transformStorage = meshTransform.Transforms.emplace_back();

			transformStorage.MRow[0] = { submeshTransform[0][0], submeshTransform[1][0], submeshTransform[2][0], submeshTransform[3][0] };
			transformStorage.MRow[1] = { submeshTransform[0][1], submeshTransform[1][1], submeshTransform[2][1], submeshTransform[3][1] };
			transformStorage.MRow[2] = { submeshTransform[0][2], submeshTransform[1][2], submeshTransform[2][2], submeshTransform[3][2] };


			// Main geo
			bool isTransparent = material->IsBlended();
			auto& destDrawList = !isTransparent ? m_StaticMeshDrawList : m_TransparentStaticMeshDrawList;
			auto& dc = destDrawList[meshKey];
			{
				dc.StaticMesh = staticMesh;
				dc.SubmeshIndex = submeshIndex;
				dc.MaterialTable = materialTable;
				dc.OverrideMaterial = overrideMaterial;
				dc.InstanceCount++;
			}

			// Shadow pass
			if (material->IsShadowCasting())
			{
				auto& dc = m_StaticMeshShadowPassDrawList[meshKey];
				dc.StaticMesh = staticMesh;
				dc.SubmeshIndex = submeshIndex;
				dc.MaterialTable = materialTable;
				dc.OverrideMaterial = overrideMaterial;
				dc.InstanceCount++;
			}


			SubmitToRaytracer(dc, material.Raw(), *reinterpret_cast<glm::mat3x4*>(&transformStorage));
		}

	}

	void SceneRenderer::SubmitSelectedMesh(Ref<Mesh> mesh, uint32_t submeshIndex, Ref<MaterialTable> materialTable, const glm::mat4& transform, const std::vector<glm::mat4>& boneTransforms, Ref<Material> overrideMaterial)
	{
		BEY_PROFILE_FUNC();
		BEY_SCOPE_PERF("SceneRenderer::SubmitStatSubmitSelectedMeshicMesh");

		// TODO: Culling, sorting, etc.

		const auto meshSource = mesh->GetMeshSource();
		if (!mesh->IsReady())
			return;
		const auto& submeshes = meshSource->GetSubmeshes();
		const auto& submesh = submeshes[submeshIndex];
		uint32_t materialIndex = submesh.MaterialIndex;
		bool isRigged = submesh.IsRigged;

		AssetHandle materialHandle = materialTable->HasMaterial(materialIndex) ? materialTable->GetMaterial(materialIndex) : mesh->GetMaterials()->GetMaterial(materialIndex);
		BEY_CORE_VERIFY(materialHandle);
		Ref<MaterialAsset> material = AssetManager::GetAsset<MaterialAsset>(materialHandle);

		MeshKey meshKey = { mesh->Handle, materialHandle, submeshIndex, true };
		TransformMapData& meshTransform = m_MeshTransformMap[meshKey];
		TransformVertexData& transformStorage = meshTransform.Transforms.emplace_back();

		transformStorage.MRow[0] = { transform[0][0], transform[1][0], transform[2][0], transform[3][0] };
		transformStorage.MRow[1] = { transform[0][1], transform[1][1], transform[2][1], transform[3][1] };
		transformStorage.MRow[2] = { transform[0][2], transform[1][2], transform[2][2], transform[3][2] };

		if (isRigged)
		{
			CopyToBoneTransformStorage(meshKey, meshSource, boneTransforms);
		}

		uint32_t instanceIndex = 0;

		// Main geo
		bool isTransparent = material->IsBlended();
		auto& destDrawList = !isTransparent ? m_DrawList : m_TransparentDrawList;
		{
			auto& dc = destDrawList[meshKey];
			dc.Mesh = mesh.Raw();
			dc.SubmeshIndex = submeshIndex;
			dc.MaterialTable = materialTable.Raw();
			dc.OverrideMaterial = overrideMaterial.Raw();
			instanceIndex = dc.InstanceCount;
			dc.InstanceCount++;
			dc.IsRigged = isRigged;
		}

		// Selected mesh list
		{
			auto& dc = m_SelectedMeshDrawList[meshKey];
			dc.Mesh = mesh.Raw();
			dc.SubmeshIndex = submeshIndex;
			dc.MaterialTable = materialTable.Raw();
			dc.OverrideMaterial = overrideMaterial.Raw();
			dc.InstanceCount++;
			dc.InstanceOffset = instanceIndex;
			dc.IsRigged = isRigged;
		}

		// Shadow pass
		if (material->IsShadowCasting())
		{
			auto& dc = m_ShadowPassDrawList[meshKey];
			dc.Mesh = mesh.Raw();
			dc.SubmeshIndex = submeshIndex;
			dc.MaterialTable = materialTable.Raw();
			dc.OverrideMaterial = overrideMaterial.Raw();
			dc.InstanceCount++;
			dc.IsRigged = isRigged;
		}

		auto& dc = destDrawList[meshKey];
		SubmitToRaytracer(dc, material.Raw(), *reinterpret_cast<glm::mat3x4*>(&transformStorage));
	}

	void SceneRenderer::SubmitSelectedStaticMesh(Ref<StaticMesh> staticMesh, Ref<MaterialTable> materialTable, const glm::mat4& transform, Ref<Material> overrideMaterial)
	{
		BEY_PROFILE_FUNC();

		Ref<MeshSource> meshSource = staticMesh->GetMeshSource();
		if (!staticMesh->IsReady())
			return;
		const auto& submeshData = meshSource->GetSubmeshes();
		for (uint32_t submeshIndex : staticMesh->GetSubmeshes())
		{
			glm::mat4 submeshTransform = transform * submeshData[submeshIndex].Transform;

			const auto& submeshes = staticMesh->GetMeshSource()->GetSubmeshes();
			uint32_t materialIndex = submeshes[submeshIndex].MaterialIndex;

			AssetHandle materialHandle = materialTable->HasMaterial(materialIndex) ? materialTable->GetMaterial(materialIndex) : staticMesh->GetMaterials()->GetMaterial(materialIndex);
			BEY_CORE_VERIFY(materialHandle);
			Ref<MaterialAsset> material = AssetManager::GetAsset<MaterialAsset>(materialHandle);

			MeshKey meshKey = { staticMesh->Handle, materialHandle, submeshIndex, true };
			TransformMapData& meshTransform = m_MeshTransformMap[meshKey];
			TransformVertexData& transformStorage = meshTransform.Transforms.emplace_back();

			transformStorage.MRow[0] = { submeshTransform[0][0], submeshTransform[1][0], submeshTransform[2][0], submeshTransform[3][0] };
			transformStorage.MRow[1] = { submeshTransform[0][1], submeshTransform[1][1], submeshTransform[2][1], submeshTransform[3][1] };
			transformStorage.MRow[2] = { submeshTransform[0][2], submeshTransform[1][2], submeshTransform[2][2], submeshTransform[3][2] };

			// Main geo
			bool isTransparent = material->IsBlended();
			auto& destDrawList = !isTransparent ? m_StaticMeshDrawList : m_TransparentStaticMeshDrawList;
			{
				auto& dc = destDrawList[meshKey];
				dc.StaticMesh = staticMesh;
				dc.SubmeshIndex = submeshIndex;
				dc.MaterialTable = materialTable;
				dc.OverrideMaterial = overrideMaterial;
				dc.InstanceCount++;
			}

			// Selected mesh list
			{
				auto& dc = m_SelectedStaticMeshDrawList[meshKey];
				dc.StaticMesh = staticMesh;
				dc.SubmeshIndex = submeshIndex;
				dc.MaterialTable = materialTable;
				dc.OverrideMaterial = overrideMaterial;
				dc.InstanceCount++;
			}

			// Shadow pass
			if (material->IsShadowCasting())
			{
				auto& dc = m_StaticMeshShadowPassDrawList[meshKey];
				dc.StaticMesh = staticMesh;
				dc.SubmeshIndex = submeshIndex;
				dc.MaterialTable = materialTable;
				dc.OverrideMaterial = overrideMaterial;
				dc.InstanceCount++;
			}

			auto& dc = destDrawList[meshKey];
			SubmitToRaytracer(dc, material.Raw(), *reinterpret_cast<glm::mat3x4*>(&transformStorage));
		}
	}

	void SceneRenderer::SubmitPhysicsDebugMesh(Ref<Mesh> mesh, uint32_t submeshIndex, const glm::mat4& transform)
	{
		BEY_CORE_VERIFY(mesh->Handle);

		Ref<MeshSource> meshSource = mesh->GetMeshSource();
		if (!mesh->IsReady())
			return;
		const auto& submeshData = meshSource->GetSubmeshes();
		glm::mat4 submeshTransform = transform * submeshData[submeshIndex].Transform;

		MeshKey meshKey = { mesh->Handle, 5, submeshIndex, false };
		auto& transformStorage = m_MeshTransformMap[meshKey].Transforms.emplace_back();

		transformStorage.MRow[0] = { transform[0][0], transform[1][0], transform[2][0], transform[3][0] };
		transformStorage.MRow[1] = { transform[0][1], transform[1][1], transform[2][1], transform[3][1] };
		transformStorage.MRow[2] = { transform[0][2], transform[1][2], transform[2][2], transform[3][2] };

		{
			auto& dc = m_ColliderDrawList[meshKey];
			dc.Mesh = mesh.Raw();
			dc.SubmeshIndex = submeshIndex;
			dc.InstanceCount++;
		}
	}

	void SceneRenderer::SubmitPhysicsStaticDebugMesh(Ref<StaticMesh> staticMesh, const glm::mat4& transform, const bool isPrimitiveCollider)
	{
		BEY_CORE_VERIFY(staticMesh->Handle);
		Ref<MeshSource> meshSource = staticMesh->GetMeshSource();
		if (!staticMesh->IsReady())
			return;
		const auto& submeshData = meshSource->GetSubmeshes();
		for (uint32_t submeshIndex : staticMesh->GetSubmeshes())
		{
			glm::mat4 submeshTransform = transform * submeshData[submeshIndex].Transform;

			MeshKey meshKey = { staticMesh->Handle, 5, submeshIndex, false };
			auto& transformStorage = m_MeshTransformMap[meshKey].Transforms.emplace_back();

			transformStorage.MRow[0] = { submeshTransform[0][0], submeshTransform[1][0], submeshTransform[2][0], submeshTransform[3][0] };
			transformStorage.MRow[1] = { submeshTransform[0][1], submeshTransform[1][1], submeshTransform[2][1], submeshTransform[3][1] };
			transformStorage.MRow[2] = { submeshTransform[0][2], submeshTransform[1][2], submeshTransform[2][2], submeshTransform[3][2] };

			{
				auto& dc = m_StaticColliderDrawList[meshKey];
				dc.StaticMesh = staticMesh;
				dc.SubmeshIndex = submeshIndex;
				dc.OverrideMaterial = isPrimitiveCollider ? m_SimpleColliderMaterial : m_ComplexColliderMaterial;
				dc.InstanceCount++;
			}

		}
	}

	void SceneRenderer::ClearPass(Ref<RenderPass> renderPass, bool explicitClear)
	{
		BEY_PROFILE_FUNC();
		Renderer::BeginRenderPass(m_MainCommandBuffer, renderPass, explicitClear);
		Renderer::EndRenderPass(m_MainCommandBuffer);
	}

	void SceneRenderer::ShadowMapPass()
	{
		BEY_PROFILE_FUNC();
		if (m_RaytracingSettings.Mode != RaytracingMode::None && m_RaytracingSettings.Mode != RaytracingMode::Raytracing)
			return;

		uint32_t frameIndex = Renderer::GetCurrentFrameIndex();
		m_GPUTimeQueries.DirShadowMapPassQuery = m_MainCommandBuffer->BeginTimestampQuery();

		auto& directionalLights = m_SceneData.SceneLightEnvironment.DirectionalLights;
		if (directionalLights[0].Intensity == 0.0f || !directionalLights[0].CastShadows)
		{
			// Clear shadow maps
			for (uint32_t i = 0; i < m_Specification.NumShadowCascades; i++)
				ClearPass(m_DirectionalShadowMapPass[i]);

			return;
		}

		for (uint32_t i = 0; i < m_Specification.NumShadowCascades; i++)
		{
			Renderer::BeginRenderPass(m_MainCommandBuffer, m_DirectionalShadowMapPass[i]);
			if (m_RaytracingSettings.Mode == RaytracingMode::None)
			{
				// Render entities
				const Buffer cascade(&i, sizeof(uint32_t));
				for (auto& [mk, dc] : m_StaticMeshShadowPassDrawList)
				{
					BEY_CORE_VERIFY(m_MeshTransformMap.find(mk) != m_MeshTransformMap.end());
					const auto& transformData = m_MeshTransformMap.at(mk);
					Renderer::RenderStaticMeshWithMaterial(m_MainCommandBuffer, m_ShadowPassPipelines[i], dc.StaticMesh, dc.SubmeshIndex, m_ShadowPassMaterial, transformData.TransformIndex, dc.InstanceCount, cascade);
				}
				for (auto& [mk, dc] : m_ShadowPassDrawList)
				{
					BEY_CORE_VERIFY(m_MeshTransformMap.find(mk) != m_MeshTransformMap.end());
					const auto& transformData = m_MeshTransformMap.at(mk);
					if (!dc.IsRigged)
						Renderer::RenderMeshWithMaterial(m_MainCommandBuffer, m_ShadowPassPipelines[i], dc.Mesh, dc.SubmeshIndex, 0, transformData.TransformIndex, dc.InstanceCount, m_ShadowPassMaterial, cascade);
				}
			}
			Renderer::EndRenderPass(m_MainCommandBuffer);
		}

		for (uint32_t i = 0; i < m_Specification.NumShadowCascades; i++)
		{
			Renderer::BeginRenderPass(m_MainCommandBuffer, m_DirectionalShadowMapAnimPass[i]);
			if (m_RaytracingSettings.Mode == RaytracingMode::None)
			{
				// Render entities
				const Buffer cascade(&i, sizeof(uint32_t));
				for (auto& [mk, dc] : m_ShadowPassDrawList)
				{
					BEY_CORE_VERIFY(m_MeshTransformMap.find(mk) != m_MeshTransformMap.end());
					const auto& transformData = m_MeshTransformMap.at(mk);
					if (dc.IsRigged)
					{
						const auto& boneTransformsData = m_MeshBoneTransformsMap.at(mk);
						Renderer::RenderMeshWithMaterial(m_MainCommandBuffer, m_ShadowPassPipelinesAnim[i], dc.Mesh, dc.SubmeshIndex, boneTransformsData.BoneTransformsBaseIndex, transformData.TransformIndex, dc.InstanceCount, m_ShadowPassMaterial, cascade);
					}
				}
			}
			Renderer::EndRenderPass(m_MainCommandBuffer);
		}

		m_MainCommandBuffer->EndTimestampQuery(m_GPUTimeQueries.DirShadowMapPassQuery);
	}

	void SceneRenderer::SpotShadowMapPass()
	{
		BEY_PROFILE_FUNC();
		if (m_RaytracingSettings.Mode != RaytracingMode::None && m_RaytracingSettings.Mode != RaytracingMode::Raytracing)
			return;
		uint32_t frameIndex = Renderer::GetCurrentFrameIndex();
		m_GPUTimeQueries.SpotShadowMapPassQuery = m_MainCommandBuffer->BeginTimestampQuery();

		// Spot shadow maps
		Renderer::BeginRenderPass(m_MainCommandBuffer, m_SpotShadowPass);
		if (m_RaytracingSettings.Mode == RaytracingMode::None)
		{
			for (uint32_t i = 0; i < 1; i++)
			{
				const Buffer lightIndex(&i, sizeof(uint32_t));
				for (auto& [mk, dc] : m_StaticMeshShadowPassDrawList)
				{
					BEY_CORE_VERIFY(m_MeshTransformMap.find(mk) != m_MeshTransformMap.end());
					const auto& transformData = m_MeshTransformMap.at(mk);
					Renderer::RenderStaticMeshWithMaterial(m_MainCommandBuffer, m_SpotShadowPassPipeline, dc.StaticMesh, dc.SubmeshIndex, m_SpotShadowPassMaterial, transformData.TransformIndex, dc.InstanceCount, lightIndex);
				}
				for (auto& [mk, dc] : m_ShadowPassDrawList)
				{
					BEY_CORE_VERIFY(m_MeshTransformMap.find(mk) != m_MeshTransformMap.end());
					const auto& transformData = m_MeshTransformMap.at(mk);
					if (dc.IsRigged)
					{
						const auto& boneTransformsData = m_MeshBoneTransformsMap.at(mk);
						Renderer::RenderMeshWithMaterial(m_MainCommandBuffer, m_SpotShadowPassAnimPipeline, dc.Mesh, dc.SubmeshIndex, boneTransformsData.BoneTransformsBaseIndex, transformData.TransformIndex, dc.InstanceCount, m_SpotShadowPassMaterial, lightIndex);
					}
					else
					{
						Renderer::RenderMeshWithMaterial(m_MainCommandBuffer, m_SpotShadowPassPipeline, dc.Mesh, dc.SubmeshIndex, 0, transformData.TransformIndex, dc.InstanceCount, m_SpotShadowPassMaterial, lightIndex);
					}
				}
			}
		}
		Renderer::EndRenderPass(m_MainCommandBuffer);
		m_MainCommandBuffer->EndTimestampQuery(m_GPUTimeQueries.SpotShadowMapPassQuery);
	}

	void SceneRenderer::PreDepthPass()
	{
		BEY_PROFILE_FUNC();

		//if (m_RaytracingSettings.Mode != RaytracingMode::None && m_RaytracingSettings.Mode != RaytracingMode::Raytracing)
			//return;

		uint32_t frameIndex = Renderer::GetCurrentFrameIndex();
		m_GPUTimeQueries.DepthPrePassQuery = m_MainCommandBuffer->BeginTimestampQuery();
		SceneRenderer::BeginGPUPerfMarker(m_MainCommandBuffer, "PreDepthPass");
		Renderer::BeginRenderPass(m_MainCommandBuffer, m_PreDepthPass);
		for (auto& [mk, dc] : m_StaticMeshDrawList)
		{
			const auto& transformData = m_MeshTransformMap.at(mk);
			Renderer::RenderStaticMeshWithMaterial(m_MainCommandBuffer, m_PreDepthPipeline, dc.StaticMesh, dc.SubmeshIndex, m_PreDepthMaterial, transformData.TransformIndex, dc.InstanceCount);
		}
		for (auto& [mk, dc] : m_DrawList)
		{
			if (!dc.IsRigged)
			{
				const auto& transformData = m_MeshTransformMap.at(mk);
				Renderer::RenderMeshWithMaterial(m_MainCommandBuffer, m_PreDepthPipeline, dc.Mesh, dc.SubmeshIndex, 0, transformData.TransformIndex, dc.InstanceCount, m_PreDepthMaterial);
			}
		}

		Renderer::EndRenderPass(m_MainCommandBuffer);

		Renderer::BeginRenderPass(m_MainCommandBuffer, m_PreDepthAnimPass);
		for (auto& [mk, dc] : m_DrawList)
		{
			if (dc.IsRigged)
			{
				const auto& transformData = m_MeshTransformMap.at(mk);
				const auto& boneTransformsData = m_MeshBoneTransformsMap.at(mk);
				Renderer::RenderMeshWithMaterial(m_MainCommandBuffer, m_PreDepthPipelineAnim, dc.Mesh, dc.SubmeshIndex, boneTransformsData.BoneTransformsBaseIndex, transformData.TransformIndex, dc.InstanceCount, m_PreDepthMaterial);
			}
		}

		Renderer::EndRenderPass(m_MainCommandBuffer);

#if 1
		Renderer::BeginRenderPass(m_MainCommandBuffer, m_PreDepthTransparentPass);
		for (auto& [mk, dc] : m_TransparentStaticMeshDrawList)
		{
			const auto& transformData = m_MeshTransformMap.at(mk);
			Renderer::RenderMeshWithMaterial(m_MainCommandBuffer, m_PreDepthTransparentPipeline, dc.StaticMesh, dc.SubmeshIndex, 0, transformData.TransformIndex, dc.InstanceCount, m_PreDepthMaterial);
		}
		for (auto& [mk, dc] : m_TransparentDrawList)
		{
			if (!dc.IsRigged)
			{
				const auto& transformData = m_MeshTransformMap.at(mk);
				Renderer::RenderMeshWithMaterial(m_MainCommandBuffer, m_PreDepthPipeline, dc.Mesh, dc.SubmeshIndex, 0, transformData.TransformIndex, dc.InstanceCount, m_PreDepthMaterial);
			}
		}
		Renderer::EndRenderPass(m_MainCommandBuffer);
#endif

		Renderer::BlitDepthImage(m_MainCommandBuffer, m_PreDepthPass->GetDepthOutput(), m_CompositePass->GetDepthOutput());
		SceneRenderer::EndGPUPerfMarker(m_MainCommandBuffer);
		m_MainCommandBuffer->EndTimestampQuery(m_GPUTimeQueries.DepthPrePassQuery);

	}

	void SceneRenderer::HZBCompute()
	{
		BEY_PROFILE_FUNC();

		if (!m_Options.EnableGTAO && !m_Options.EnableSSR)
			return;

		m_GPUTimeQueries.HierarchicalDepthQuery = m_MainCommandBuffer->BeginTimestampQuery();

		constexpr uint32_t maxMipBatchSize = 4;
		const uint32_t hzbMipCount = m_HierarchicalDepthTexture.Texture->GetMipLevelCount();

		Renderer::BeginGPUPerfMarker(m_MainCommandBuffer, "HZB");
		Renderer::BeginComputePass(m_MainCommandBuffer, m_HierarchicalDepthPass);

		auto ReduceHZB = [commandBuffer = m_MainCommandBuffer, hierarchicalDepthPass = m_HierarchicalDepthPass, hierarchicalDepthTexture = m_HierarchicalDepthTexture.Texture, hzbMaterials = m_HZBMaterials]
		(const uint32_t startDestMip, const uint32_t parentMip, const glm::vec2& DispatchThreadIdToBufferUV, const glm::vec2& InputViewportMaxBound, const bool isFirstPass)
		{
			struct HierarchicalZComputePushConstants
			{
				glm::vec2 DispatchThreadIdToBufferUV;
				glm::vec2 InputViewportMaxBound;
				glm::vec2 InvSize;
				int FirstLod;
				bool IsFirstPass;
				char Padding[3]{ 0, 0, 0 };
			} hierarchicalZComputePushConstants;

			hierarchicalZComputePushConstants.IsFirstPass = isFirstPass;
			hierarchicalZComputePushConstants.FirstLod = (int)startDestMip;
			hierarchicalZComputePushConstants.DispatchThreadIdToBufferUV = DispatchThreadIdToBufferUV;
			hierarchicalZComputePushConstants.InputViewportMaxBound = InputViewportMaxBound;

			const glm::ivec2 srcSize(Math::DivideAndRoundUp(hierarchicalDepthTexture->GetSize(), 1u << parentMip));
			const glm::ivec2 dstSize(Math::DivideAndRoundUp(hierarchicalDepthTexture->GetSize(), 1u << startDestMip));
			hierarchicalZComputePushConstants.InvSize = glm::vec2{ 1.0f / (float)srcSize.x, 1.0f / (float)srcSize.y };

			glm::uvec3 workGroups(Math::DivideAndRoundUp(dstSize.x, 8), Math::DivideAndRoundUp(dstSize.y, 8), 1);
			Renderer::DispatchCompute(commandBuffer, hierarchicalDepthPass, hzbMaterials[startDestMip / 4], workGroups, Buffer(&hierarchicalZComputePushConstants, sizeof(hierarchicalZComputePushConstants)));
		};

		Renderer::BeginGPUPerfMarker(m_MainCommandBuffer, "HZB-FirstPass");

		// Reduce first 4 mips
		glm::ivec2 srcSize = m_PreDepthPass->GetDepthOutput()->GetSize();
		ReduceHZB(0, 0, { 1.0f / glm::vec2{ srcSize } }, { (glm::vec2{ srcSize } - 0.5f) / glm::vec2{ srcSize } }, true);
		Renderer::EndGPUPerfMarker(m_MainCommandBuffer);

		// Reduce the next mips
		for (uint32_t startDestMip = maxMipBatchSize; startDestMip < hzbMipCount; startDestMip += maxMipBatchSize)
		{
			Renderer::BeginGPUPerfMarker(m_MainCommandBuffer, fmt::eastl_format("HZB-Pass-({})", startDestMip));
			srcSize = Math::DivideAndRoundUp(m_HierarchicalDepthTexture.Texture->GetSize(), 1u << uint32_t(startDestMip - 1));
			ReduceHZB(startDestMip, startDestMip - 1, { 2.0f / glm::vec2{ srcSize } }, glm::vec2{ 1.0f }, false);
			Renderer::EndGPUPerfMarker(m_MainCommandBuffer);
		}

		Renderer::EndGPUPerfMarker(m_MainCommandBuffer);

		Renderer::EndComputePass(m_MainCommandBuffer, m_HierarchicalDepthPass);
		m_MainCommandBuffer->EndTimestampQuery(m_GPUTimeQueries.HierarchicalDepthQuery);
	}

	void SceneRenderer::MotionVectorsCompute()
	{
		BEY_PROFILE_FUNC();

		//if (m_RaytracingSettings.Mode != RaytracingMode::None && m_RaytracingSettings.Mode != RaytracingMode::Raytracing)
			//return;
		//return;
		m_GPUTimeQueries.MotionVectorsQuery = m_MainCommandBuffer->BeginTimestampQuery();
		Renderer::BeginComputePass(m_MainCommandBuffer, m_ExposurePass);
		struct ExposurePushConst
		{
			float Exposure;
		} exposurePushConst;
		exposurePushConst.Exposure = m_SceneData.SceneCamera.Camera->GetExposure();
		Renderer::DispatchCompute(m_MainCommandBuffer, m_ExposurePass, nullptr, { 1, 1, 1 }, Buffer(&exposurePushConst, sizeof(ExposurePushConst)));

		Renderer::EndComputePass(m_MainCommandBuffer, m_ExposurePass);
		m_MainCommandBuffer->EndTimestampQuery(m_GPUTimeQueries.MotionVectorsQuery);

	}

	void SceneRenderer::PreIntegration()
	{
		BEY_PROFILE_FUNC();

		if (m_RaytracingSettings.Mode != RaytracingMode::None && m_RaytracingSettings.Mode != RaytracingMode::Raytracing)
			return;

		m_GPUTimeQueries.PreIntegrationQuery = m_MainCommandBuffer->BeginTimestampQuery();
		glm::vec2 projectionParams = { m_SceneData.SceneCamera.Far, m_SceneData.SceneCamera.Near }; // Reversed 

		Ref<Texture2D> visibilityTexture = m_PreIntegrationVisibilityTexture.Texture;

		ImageClearValue clearValue = { glm::vec4(1.0f) };
		ImageSubresourceRange subresourceRange{};
		subresourceRange.BaseMip = 0;
		subresourceRange.MipCount = 1;
		Renderer::ClearImage(m_MainCommandBuffer, visibilityTexture->GetImage(), clearValue, subresourceRange);

		struct PreIntegrationComputePushConstants
		{
			glm::vec2 HZBResFactor;
			glm::vec2 ResFactor;
			glm::vec2 ProjectionParams; //(x) = Near plane, (y) = Far plane
			int PrevLod = 0;
		} preIntegrationComputePushConstants;

		Renderer::BeginGPUPerfMarker(m_MainCommandBuffer, "PreIntegration");

		Renderer::BeginComputePass(m_MainCommandBuffer, m_PreIntegrationPass);

		for (uint32_t mip = 1; mip < visibilityTexture->GetMipLevelCount(); mip++)
		{
			Renderer::BeginGPUPerfMarker(m_MainCommandBuffer, fmt::eastl_format("PreIntegration-Pass({})", mip));
			auto [mipWidth, mipHeight] = visibilityTexture->GetMipSize(mip);
			glm::uvec3 workGroups = { (uint32_t)glm::ceil((float)mipWidth / 8.0f), (uint32_t)glm::ceil((float)mipHeight / 8.0f), 1 };

			auto [width, height] = visibilityTexture->GetMipSize(mip);
			glm::vec2 resFactor = 1.0f / glm::vec2{ width, height };
			preIntegrationComputePushConstants.HZBResFactor = resFactor * m_SSROptions.HZBUvFactor;
			preIntegrationComputePushConstants.ResFactor = resFactor;
			preIntegrationComputePushConstants.ProjectionParams = projectionParams;
			preIntegrationComputePushConstants.PrevLod = (int)mip - 1;

			Buffer pushConstants(&preIntegrationComputePushConstants, sizeof(PreIntegrationComputePushConstants));
			Renderer::DispatchCompute(m_MainCommandBuffer, m_PreIntegrationPass, m_PreIntegrationMaterials[mip - 1], workGroups, pushConstants);

			Renderer::EndGPUPerfMarker(m_MainCommandBuffer);
		}
		Renderer::EndComputePass(m_MainCommandBuffer, m_PreIntegrationPass);
		Renderer::EndGPUPerfMarker(m_MainCommandBuffer);

		m_MainCommandBuffer->EndTimestampQuery(m_GPUTimeQueries.PreIntegrationQuery);
	}

	void SceneRenderer::LightCullingPass()
	{
		if (m_RaytracingSettings.Mode != RaytracingMode::None && m_RaytracingSettings.Mode != RaytracingMode::Raytracing)
			return;

		m_GPUTimeQueries.LightCullingPassQuery = m_MainCommandBuffer->BeginTimestampQuery();
		SceneRenderer::BeginGPUPerfMarker(m_MainCommandBuffer, "LightCulling", { 0.75f, 0.24f, 1.0f, 1.0f });

		Renderer::BeginComputePass(m_MainCommandBuffer, m_LightCullingPass);
		Renderer::DispatchCompute(m_MainCommandBuffer, m_LightCullingPass, nullptr, m_LightCullingWorkGroups);
		Renderer::EndComputePass(m_MainCommandBuffer, m_LightCullingPass);

		// NOTE: ideally this would be done automatically by RenderPass/ComputePass system
		Ref<PipelineCompute> pipeline = m_LightCullingPass->GetPipeline();
		pipeline->BufferMemoryBarrier(m_MainCommandBuffer, m_SBSVisiblePointLightIndicesBuffer->Get(),
			PipelineStage::ComputeShader, ResourceAccessFlags::ShaderWrite,
			(PipelineStage)((int)PipelineStage::FragmentShader | (int)PipelineStage::RaytracingShader), ResourceAccessFlags::ShaderRead);
		pipeline->BufferMemoryBarrier(m_MainCommandBuffer, m_SBSVisibleSpotLightIndicesBuffer->Get(),
			PipelineStage::ComputeShader, ResourceAccessFlags::ShaderWrite,
			(PipelineStage)((int)PipelineStage::FragmentShader | (int)PipelineStage::RaytracingShader), ResourceAccessFlags::ShaderRead);

		SceneRenderer::EndGPUPerfMarker(m_MainCommandBuffer);
		m_MainCommandBuffer->EndTimestampQuery(m_GPUTimeQueries.LightCullingPassQuery);
	}

	void SceneRenderer::SkyboxPass()
	{
		BEY_PROFILE_FUNC();
		if (m_RaytracingSettings.Mode != RaytracingMode::None && m_RaytracingSettings.Mode != RaytracingMode::Raytracing)
			return;

		Renderer::BeginRenderPass(m_MainCommandBuffer, m_SkyboxPass);

		// Skybox
		m_SkyboxMaterial->Set("u_Uniforms.TextureLod", m_SceneData.SkyboxLod);
		m_SkyboxMaterial->Set("u_Uniforms.Intensity", m_SceneData.SceneEnvironmentIntensity);

		const Ref<TextureCube> radianceMap = m_SceneData.SceneEnvironment ? m_SceneData.SceneEnvironment->RadianceMap : Renderer::GetBlackCubeTexture();
		m_SkyboxMaterial->Set("u_Texture", radianceMap);

		SceneRenderer::BeginGPUPerfMarker(m_MainCommandBuffer, "Skybox", { 0.3f, 0.0f, 1.0f, 1.0f });
		Renderer::SubmitFullscreenQuad(m_MainCommandBuffer, m_SkyboxPipeline, m_SkyboxMaterial);
		SceneRenderer::EndGPUPerfMarker(m_MainCommandBuffer);
		Renderer::EndRenderPass(m_MainCommandBuffer);
	}

	void SceneRenderer::GeometryPass()
	{
		BEY_PROFILE_FUNC();

		uint32_t frameIndex = Renderer::GetCurrentFrameIndex();

		m_GPUTimeQueries.GeometryPassQuery = m_MainCommandBuffer->BeginTimestampQuery();

		Renderer::BeginRenderPass(m_MainCommandBuffer, m_SelectedGeometryPass);
		for (auto& [mk, dc] : m_SelectedStaticMeshDrawList)
		{
			const auto& transformData = m_MeshTransformMap.at(mk);
			Renderer::RenderStaticMeshWithMaterial(m_MainCommandBuffer, m_SelectedGeometryPass->GetSpecification().Pipeline, dc.StaticMesh, dc.SubmeshIndex, m_SelectedGeometryMaterial, transformData.TransformIndex, dc.InstanceCount);
		}
		for (auto& [mk, dc] : m_SelectedMeshDrawList)
		{
			const auto& transformData = m_MeshTransformMap.at(mk);
			if (!dc.IsRigged)
				Renderer::RenderMeshWithMaterial(m_MainCommandBuffer, m_SelectedGeometryPass->GetPipeline(), dc.Mesh, dc.SubmeshIndex, 0, transformData.TransformIndex, dc.InstanceCount, m_SelectedGeometryMaterial);
		}
		Renderer::EndRenderPass(m_MainCommandBuffer);

		Renderer::BeginRenderPass(m_MainCommandBuffer, m_SelectedGeometryAnimPass);
		for (auto& [mk, dc] : m_SelectedMeshDrawList)
		{
			const auto& transformData = m_MeshTransformMap.at(mk);
			if (dc.IsRigged)
			{
				const auto& boneTransformsData = m_MeshBoneTransformsMap.at(mk);
				Renderer::RenderMeshWithMaterial(m_MainCommandBuffer, m_SelectedGeometryAnimPass->GetPipeline(), dc.Mesh, dc.SubmeshIndex, boneTransformsData.BoneTransformsBaseIndex + dc.InstanceOffset, transformData.TransformIndex, dc.InstanceCount, m_SelectedGeometryMaterial);
			}
		}
		Renderer::EndRenderPass(m_MainCommandBuffer);

		if (m_RaytracingSettings.Mode == RaytracingMode::None)
		{
			Renderer::BeginRenderPass(m_MainCommandBuffer, m_GeometryPass);

			// Render static meshes
			SceneRenderer::BeginGPUPerfMarker(m_MainCommandBuffer, "Static Meshes");
			for (auto& [mk, dc] : m_StaticMeshDrawList)
			{
				const auto& transformData = m_MeshTransformMap.at(mk);
				Renderer::RenderStaticMesh(m_MainCommandBuffer, m_GeometryPipeline, dc.StaticMesh, dc.SubmeshIndex, dc.MaterialTable ? dc.MaterialTable : dc.StaticMesh->GetMaterials(), transformData.TransformIndex, dc.InstanceCount);
			}
			SceneRenderer::EndGPUPerfMarker(m_MainCommandBuffer);

			// Render dynamic meshes
			SceneRenderer::BeginGPUPerfMarker(m_MainCommandBuffer, "Dynamic Meshes");
			for (auto& [mk, dc] : m_DrawList)
			{
				const auto& transformData = m_MeshTransformMap.at(mk);
				if (!dc.IsRigged)
					Renderer::RenderSubmeshInstanced(m_MainCommandBuffer, m_GeometryPipeline, dc.Mesh, dc.SubmeshIndex, dc.MaterialTable ? dc.MaterialTable : dc.Mesh->GetMaterials(), 0, transformData.TransformIndex, dc.InstanceCount);
			}
			SceneRenderer::EndGPUPerfMarker(m_MainCommandBuffer);

#if 1
			{
				// Render static meshes
				SceneRenderer::BeginGPUPerfMarker(m_MainCommandBuffer, "Static Transparent Meshes");
				for (auto& [mk, dc] : m_TransparentStaticMeshDrawList)
				{
					const auto& transformData = m_MeshTransformMap.at(mk);
					Renderer::RenderStaticMesh(m_MainCommandBuffer, m_TransparentGeometryPipeline, dc.StaticMesh, dc.SubmeshIndex, dc.MaterialTable ? dc.MaterialTable : dc.StaticMesh->GetMaterials(), transformData.TransformIndex, dc.InstanceCount);
				}
				SceneRenderer::EndGPUPerfMarker(m_MainCommandBuffer);

				// Render dynamic meshes
				SceneRenderer::BeginGPUPerfMarker(m_MainCommandBuffer, "Dynamic Transparent Meshes");
				for (auto& [mk, dc] : m_TransparentDrawList)
				{
					const auto& transformData = m_MeshTransformMap.at(mk);
					//Renderer::RenderSubmesh(m_MainCommandBuffer, m_GeometryPipeline, m_UniformBufferSet, m_StorageBufferSet, dc.Mesh, dc.SubmeshIndex, dc.MaterialTable ? dc.MaterialTable : dc.Mesh->GetMaterials(), dc.Transform);
					Renderer::RenderSubmeshInstanced(m_MainCommandBuffer, m_TransparentGeometryPipeline, dc.Mesh, dc.SubmeshIndex, dc.MaterialTable ? dc.MaterialTable : dc.Mesh->GetMaterials(), 0, transformData.TransformIndex, dc.InstanceCount);
				}
				SceneRenderer::EndGPUPerfMarker(m_MainCommandBuffer);
			}
#endif
			Renderer::EndRenderPass(m_MainCommandBuffer);
		}

		if (m_RaytracingSettings.Mode == RaytracingMode::None)
		{
			Renderer::BeginRenderPass(m_MainCommandBuffer, m_GeometryAnimPass);
			for (auto& [mk, dc] : m_DrawList)
			{
				const auto& transformData = m_MeshTransformMap.at(mk);
				if (dc.IsRigged)
				{
					const auto& boneTransformsData = m_MeshBoneTransformsMap.at(mk);
					Renderer::RenderSubmeshInstanced(m_MainCommandBuffer, m_GeometryPipelineAnim, dc.Mesh, dc.SubmeshIndex, dc.MaterialTable ? dc.MaterialTable : dc.Mesh->GetMaterials(), boneTransformsData.BoneTransformsBaseIndex, transformData.TransformIndex, dc.InstanceCount);
				}
			}

			Renderer::EndRenderPass(m_MainCommandBuffer);
		}

		m_MainCommandBuffer->EndTimestampQuery(m_GPUTimeQueries.GeometryPassQuery);
	}

	void SceneRenderer::PathTracingPass()
	{
		Ref<RenderCommandBuffer> raytracingCmdBuffer = m_MainCommandBuffer;

		if (m_RaytracingSettings.Mode != RaytracingMode::None)
		{
			auto Raytrace = [&](Ref<RaytracingPass> pass, Ref<Material> material, Buffer pushConstant, ShaderStage pushConstantStages)
			{
				//Renderer::ClearImage(raytracingCmdBuffer, m_RaytracingImage, { {0.0f, 1.0f, 0.0f, 0.0f } });
				m_GPUTimeQueries.RaytracingQuery = raytracingCmdBuffer->BeginTimestampQuery();
				Renderer::BeginRaytracingPass(raytracingCmdBuffer, pass);
				BeginGPUPerfMarker(raytracingCmdBuffer, "Ray tracing");
				Renderer::SetPushConstant(pass, pushConstant, pushConstantStages);

				Renderer::DispatchRays(raytracingCmdBuffer, pass, material, m_RenderWidth, m_RenderHeight, 1);

				EndGPUPerfMarker(raytracingCmdBuffer);
				Renderer::EndRaytracingPass(raytracingCmdBuffer, pass);
				raytracingCmdBuffer->EndTimestampQuery(m_GPUTimeQueries.RaytracingQuery);
			};

			const Ref<TextureCube> radianceMap = m_SceneData.SceneEnvironment ? m_SceneData.SceneEnvironment->RadianceMap : Renderer::GetBlackCubeTexture();
			if (m_RaytracingSettings.Mode == RaytracingMode::Pathtracing)
			{
				if (m_SceneData.SceneCamera.Camera->IsMoving() || PanelManager::HasAnyChanged() || Application::HasChanged() || m_RaytracerReset)
				{
					m_AccumulatedPathtracingFrames = 0;
					m_RaytracerReset = false;
				}

				{
					// Push constant structure for the ray tracer
					struct
					{
						uint32_t FrameIndex;
						uint32_t PathtracingFrameIndex;
						uint32_t EnableRussianRoulette;
					} pushConstant;
					pushConstant.EnableRussianRoulette = m_RaytracingSettings.EnableRussianRoulette;
					pushConstant.FrameIndex = m_AccumulatedFrames;
					pushConstant.PathtracingFrameIndex = m_AccumulatedPathtracingFrames;

					Raytrace(m_PathTracingRenderPass, m_PathtracingMaterial, Buffer(&pushConstant, sizeof(pushConstant)), ShaderStage::RayGen
						| ShaderStage::RayAnyHit
					);
				}
			}
			else if (m_RaytracingSettings.Mode == RaytracingMode::Restir)
			{
				if (m_SceneData.SceneCamera.Camera->IsMoving() || PanelManager::HasAnyChanged() || Application::HasChanged() || m_RaytracerReset)
				{
					m_AccumulatedPathtracingFrames = 0;
					m_RaytracerReset = false;
				}
				{
					// Push constant structure for the ray tracer
					struct
					{
						uint32_t FrameIndex;
						uint32_t PathtracingFrameIndex;
						uint32_t EnableRussianRoulette;
					} pushConstant;
					pushConstant.EnableRussianRoulette = m_RaytracingSettings.EnableRussianRoulette;
					pushConstant.FrameIndex = m_AccumulatedFrames;
					pushConstant.PathtracingFrameIndex = m_AccumulatedPathtracingFrames;

					Raytrace(m_RestirRenderPass, m_RestirMaterial, Buffer(&pushConstant, sizeof(pushConstant)), ShaderStage::RayGen
						| ShaderStage::RayAnyHit
					);
				}
			}
			else if (m_RaytracingSettings.Mode == RaytracingMode::RestirComp)
			{
				if (m_SceneData.SceneCamera.Camera->IsMoving() || PanelManager::HasAnyChanged() || Application::HasChanged() || m_RaytracerReset)
				{
					m_AccumulatedPathtracingFrames = 0;
					m_RaytracerReset = false;
				}
				{
					// Push constant structure for the ray tracer
					struct
					{
						uint32_t FrameIndex;
						uint32_t PathtracingFrameIndex;
						uint32_t EnableRussianRoulette;
					} pushConstant;
					pushConstant.EnableRussianRoulette = m_RaytracingSettings.EnableRussianRoulette;
					pushConstant.FrameIndex = m_AccumulatedFrames;
					pushConstant.PathtracingFrameIndex = m_AccumulatedPathtracingFrames;
					//Renderer::ClearImage(raytracingCmdBuffer, m_RaytracingImage, { {0.0f, 1.0f, 0.0f, 0.0f } });


					m_GPUTimeQueries.RaytracingQuery = raytracingCmdBuffer->BeginTimestampQuery();
					Renderer::BeginComputePass(raytracingCmdBuffer, m_RestirCompRenderPass);
					BeginGPUPerfMarker(raytracingCmdBuffer, "Ray tracing");

					// Compute the number of workgroups needed in each dimension
					glm::uvec3 workGroups = glm::uvec3(glm::ivec2((glm::ivec2(m_ScreenDataUB.FullResolution) + m_RaytracingSettings.WorkGroupSize - 1) / m_RaytracingSettings.WorkGroupSize), 1u); // Round up

					Renderer::DispatchCompute(raytracingCmdBuffer, m_RestirCompRenderPass, m_RestirCompMaterial, workGroups, Buffer(&pushConstant, sizeof(pushConstant)));

					EndGPUPerfMarker(raytracingCmdBuffer);
					Renderer::EndComputePass(raytracingCmdBuffer, m_RestirCompRenderPass);
					raytracingCmdBuffer->EndTimestampQuery(m_GPUTimeQueries.RaytracingQuery);
				}
			}
			else if (m_RaytracingSettings.Mode == RaytracingMode::Raytracing)
			{
				// Push constant structure for the ray tracer
				struct
				{
					uint32_t Frame;
				} pushConstant;
				pushConstant.Frame = m_AccumulatedFrames;

				Raytrace(m_RayTracingRenderPass, m_RaytracingMaterial, Buffer(&pushConstant, sizeof(pushConstant)), ShaderStage::RayGen
#if 0
					| ShaderStage::RayClosestHit
#endif
				);
			}

			if (m_AccumulatedPathtracingFrames > m_RaytracingSettings.MaxFrames)
				m_AccumulatedPathtracingFrames--;

			Renderer::CopyImage(m_MainCommandBuffer, m_RaytracingImage, m_SkyboxPass->GetOutput(0));
			Renderer::CopyImage(m_MainCommandBuffer, m_RaytracingNormalsImage, m_GeometryPass->GetOutput(1));
			GTAODataCB.NoiseIndex = m_AccumulatedPathtracingFrames % 64;
			//Renderer::CopyImage(m_MainCommandBuffer, m_RaytracingMetalnessRoughnessImage, m_GeometryPass->GetOutput(2));
		}

		Renderer::Submit([instance = Ref(this)]() mutable
		{
			instance->m_MainRaytracer->Clear();
		});
	}

	void SceneRenderer::DLSSPass()
	{
		m_GPUTimeQueries.DLSSPassQuery = m_MainCommandBuffer->BeginTimestampQuery();
		SceneRenderer::BeginGPUPerfMarker(m_MainCommandBuffer, "DLSS-Evaluate", { 0.0f, 1.0f, 0.1f, 1.0f });
		Ref<Image2D> hitTImage = m_RaytracingSettings.Mode == RaytracingMode::Pathtracing ? m_RaytracingPrimaryHitT : nullptr;
		if (m_DLSSSettings.Enable)
			m_DLSS->Evaluate(m_MainCommandBuffer, m_AccumulatedFrames, m_CurrentJitter, m_TimeStep, m_DLSSImage, m_GeometryPass->GetOutput(0), hitTImage, m_ExposureImage,
				m_PreDepthPass->GetOutput(0), m_PreDepthPass->GetDepthOutput(), m_AlbedoImage, m_RaytracingMetalnessRoughnessImage, m_RaytracingNormalsImage);
		SceneRenderer::EndGPUPerfMarker(m_MainCommandBuffer);
		m_MainCommandBuffer->EndTimestampQuery(m_GPUTimeQueries.DLSSPassQuery);
	}

	void SceneRenderer::DDGIRaytracing()
	{
		auto& volumes = Renderer::GetDDGIVolumes();

		Renderer::SetDDGIStorage(m_SBDDGIConstants, m_SBDDGIReourceIndices);
		if (m_RaytracingSettings.Mode != RaytracingMode::Pathtracing && (m_RaytracingSettings.Mode != RaytracingMode::Restir) && m_DDGISettings.Enable)
		{
			if (!volumes.empty())
			{
				m_DDGIRayTracingRenderPass->SetInput("DDGIVolumeBindless", m_SBDDGIReourceIndices);
				m_DDGIRayTracingRenderPass->SetInput("DDGIVolumes", m_SBDDGIConstants);
			}

			struct
			{
				uint volumeIndex = 0;
				uint volumeConstantsIndex = 0;
				uint volumeResourceIndicesIndex = 0;
				// Split uint3 into three uints to prevent internal padding
				// while keeping these values at the end of the struct
				uint  reductionInputSizeX;
				uint  reductionInputSizeY;
				uint  reductionInputSizeZ;
			} pushConstant;

			for (const auto& volume : volumes)
			{
				Renderer::BeginRaytracingPass(m_MainCommandBuffer, m_DDGIRayTracingRenderPass);
				uint32_t width, height, depth;
				volume->GetRayDispatchDimensions(width, height, depth);
				Renderer::SetPushConstant(m_DDGIRayTracingRenderPass, { &pushConstant, sizeof(pushConstant) }, ShaderStage::RayGen);
				Renderer::DispatchRays(m_MainCommandBuffer, m_DDGIRayTracingRenderPass, nullptr, width, height, depth);

				Renderer::EndRaytracingPass(m_MainCommandBuffer, m_DDGIRayTracingRenderPass);
			}

			if (volumes.size())
				Renderer::UpdateDDGIVolumes(m_MainCommandBuffer);

		}
	}

	void SceneRenderer::DDGIVis()
	{
		if (m_RaytracingSettings.Mode != RaytracingMode::Pathtracing && m_RaytracingSettings.Mode != RaytracingMode::Restir && m_DDGISettings.ProbeVis && m_DDGISettings.Enable)
		{
			const auto& volumes = Renderer::GetDDGIVolumes();

			if (!volumes.empty())
			{
				for (const auto& volume : volumes)
				{
					StaticDrawCommand dc{};
					dc.InstanceCount = (uint32_t)volume->GetNumProbes();
					dc.StaticMesh = m_SphereMesh;
					dc.MaterialTable = Ref<MaterialTable>::Create(1);

					m_DDGIVisRaytracer->AddInstancedDrawCommand(dc, m_DDGIVisRenderPass, glm::mat4x4(1));

					Renderer::Submit([inst = Ref(this), numProbes = volume->GetNumProbes()]() mutable
					{
						const uint32_t size = glm::max(uint(numProbes * sizeof(VkAccelerationStructureInstanceKHR)), 8u);
						inst->m_SBSDDGIProbeInstances->RT_Get()->RT_Resize(size);
						inst->m_SBSDDGIProbeInstances->RT_Get()->RT_SetData(inst->m_DDGIVisRaytracer->GetVulkanInstances().data(), size);
					});
				}

				for (const auto& volume : volumes)
				{
					struct
					{
						uint VolumeIndex = 0;
						float ProbeRadius = 0.3f;
						uint InstanceOffset = 0;
						uint VolumeConstantsIndex = 0;
						uint VolumeResourceIndicesIndex = 0;
					} pushConstant;

					pushConstant.VolumeIndex = volume->GetIndex();

					// Dispatch the compute shader
					const float groupSize = 32.f;
					const uint32_t numProbes = static_cast<uint32_t>(volume->GetNumProbes());
					glm::uvec3 workGroups((uint32_t)ceil((float)numProbes / groupSize), 1, 1);

					Renderer::BeginComputePass(m_MainCommandBuffer, m_DDGIProbeUpdatePass);
					Renderer::DispatchCompute(m_MainCommandBuffer, m_DDGIProbeUpdatePass, nullptr, workGroups, { &pushConstant, sizeof(pushConstant) });
					Renderer::EndComputePass(m_MainCommandBuffer, m_DDGIProbeUpdatePass);
				}


				//m_DDGIVisRaytracer->BuildTlas(m_MainCommandBuffer, m_SBSDDGIProbeInstances->Get());

				m_DDGIVisRenderPass->SetInput("DDGIVolumeBindless", m_SBDDGIReourceIndices);
				m_DDGIVisRenderPass->SetInput("DDGIVolumes", m_SBDDGIConstants);

				//struct
				//{
				//	uint volumeIndex = 0;
				//	uint volumeConstantsIndex = 0;
				//	uint volumeResourceIndicesIndex = 0;
				//	// Split uint3 into three uints to prevent internal padding
				//	// while keeping these values at the end of the struct
				//	uint  reductionInputSizeX;
				//	uint  reductionInputSizeY;
				//	uint  reductionInputSizeZ;
				//} pushConstant;

				Renderer::BeginRaytracingPass(m_MainCommandBuffer, m_DDGIVisRenderPass);
				for (const auto& volume : volumes)
				{
					//pushConstant.volumeIndex = volume->GetIndex();
					Renderer::DispatchRays(m_MainCommandBuffer, m_DDGIVisRenderPass, nullptr, m_RenderWidth, m_RenderHeight, 1/*, {}, { &pushConstant, sizeof(pushConstant) }*/);

				}
				Renderer::EndRaytracingPass(m_MainCommandBuffer, m_DDGIVisRenderPass);
				Renderer::CopyImage(m_MainCommandBuffer, m_RaytracingImage, m_SkyboxPass->GetOutput(0));


			}

		}
		Renderer::Submit([instance = Ref(this)]() mutable
		{
			instance->m_DDGIVisRaytracer->Clear();
		});

	}



	void SceneRenderer::DDGIIrradiance()
	{
		if (m_RaytracingSettings.Mode != RaytracingMode::Pathtracing && m_RaytracingSettings.Mode != RaytracingMode::Restir && !Renderer::GetDDGIVolumes().empty() && m_DDGISettings.Enable)
		{
			const bool rasterized = m_RaytracingSettings.Mode == RaytracingMode::None;
			m_DDGIIrradiancePass->SetInput("NormalTexture", rasterized ? m_GeometryPass->GetOutput(1) : m_RaytracingNormalsImage);
			m_DDGIIrradiancePass->SetInput("AlbedoTexture", rasterized ? m_GeometryPass->GetOutput(3) : m_AlbedoImage);
			//m_DDGIIrradiancePass->SetInput("OutputTexture", );

			glm::vec2 HZBUVFactor = m_SSROptions.HZBUvFactor;

			auto DivRoundUp = [](uint32_t x, uint32_t y) -> uint32_t
			{
				if (x % y) return 1 + x / y;
				else return x / y;
			};

			Renderer::CopyImage(m_MainCommandBuffer, rasterized ? m_SkyboxPass->GetOutput(0) : m_RaytracingImage, m_DDGIOutputImage);

			uint32_t groupsX = DivRoundUp(m_RenderWidth, 8);
			uint32_t groupsY = DivRoundUp(m_RenderHeight, 4);

			Renderer::BeginComputePass(m_MainCommandBuffer, m_DDGIIrradiancePass);
			Renderer::DispatchCompute(m_MainCommandBuffer, m_DDGIIrradiancePass, nullptr, glm::uvec3(groupsX, groupsY, 1), { &HZBUVFactor, sizeof(HZBUVFactor) });
			Renderer::EndComputePass(m_MainCommandBuffer, m_DDGIIrradiancePass);

			if (m_DDGISettings.TextureVis)
			{
				Renderer::BeginComputePass(m_MainCommandBuffer, m_DDGITexVisPass);
				Renderer::DispatchCompute(m_MainCommandBuffer, m_DDGITexVisPass, nullptr, glm::uvec3(groupsX, groupsY, 1), { &m_DDGITextureVisSettings, sizeof(m_DDGITextureVisSettings) });
				Renderer::EndComputePass(m_MainCommandBuffer, m_DDGITexVisPass);

			}
			Renderer::CopyImage(m_MainCommandBuffer, m_DDGIOutputImage, m_SkyboxPass->GetOutput(0));
		}
	}

	void SceneRenderer::PreConvolutionCompute()
	{
		BEY_PROFILE_FUNC();

		// TODO: Other techniques might need it in the future
		if (!m_Options.EnableSSR)
			return;

		struct PreConvolutionComputePushConstants
		{
			int PrevLod = 0;
			int Mode = 0; // 0 = Copy, 1 = GaussianHorizontal, 2 = GaussianVertical
		} preConvolutionComputePushConstants;

		// Might change to be maximum res used by other techniques other than SSR
		int halfRes = int(m_SSROptions.HalfRes);

		m_GPUTimeQueries.PreConvolutionQuery = m_MainCommandBuffer->BeginTimestampQuery();

		glm::uvec3 workGroups(0);

		Renderer::BeginComputePass(m_MainCommandBuffer, m_PreConvolutionComputePass);

		auto inputImage = m_SkyboxPass->GetOutput(0);
		workGroups = { (uint32_t)glm::ceil((float)inputImage->GetWidth() / 16.0f), (uint32_t)glm::ceil((float)inputImage->GetHeight() / 16.0f), 1 };
		Renderer::DispatchCompute(m_MainCommandBuffer, m_PreConvolutionComputePass, m_PreConvolutionMaterials[0], workGroups, Buffer(&preConvolutionComputePushConstants, sizeof(preConvolutionComputePushConstants)));

		const uint32_t mipCount = m_PreConvolutedTexture.Texture->GetMipLevelCount();
		for (uint32_t mip = 1; mip < mipCount; mip++)
		{
			Renderer::BeginGPUPerfMarker(m_MainCommandBuffer, fmt::eastl_format("Pre-Convolution-Mip({})", mip));

			auto [mipWidth, mipHeight] = m_PreConvolutedTexture.Texture->GetMipSize(mip);
			workGroups = { (uint32_t)glm::ceil((float)mipWidth / 16.0f), (uint32_t)glm::ceil((float)mipHeight / 16.0f), 1 };
			preConvolutionComputePushConstants.PrevLod = (int)mip - 1;

			auto blur = [&](const uint32_t mip, const int mode)
			{
				Renderer::BeginGPUPerfMarker(m_MainCommandBuffer, fmt::eastl_format("Pre-Convolution-Mode({})", mode));
				preConvolutionComputePushConstants.Mode = mode;
				Renderer::DispatchCompute(m_MainCommandBuffer, m_PreConvolutionComputePass, m_PreConvolutionMaterials[mip], workGroups, Buffer(&preConvolutionComputePushConstants, sizeof(preConvolutionComputePushConstants)));
				Renderer::EndGPUPerfMarker(m_MainCommandBuffer);
			};

			blur(mip, 1); // Horizontal blur
			blur(mip, 2); // Vertical Blur

			Renderer::EndGPUPerfMarker(m_MainCommandBuffer);
		}

		Renderer::EndComputePass(m_MainCommandBuffer, m_PreConvolutionComputePass);
	}

	void SceneRenderer::GTAOCompute()
	{
		const Buffer pushConstantBuffer(&GTAODataCB, sizeof GTAODataCB);

		m_GPUTimeQueries.GTAOPassQuery = m_MainCommandBuffer->BeginTimestampQuery();
		Renderer::BeginComputePass(m_MainCommandBuffer, m_GTAOComputePass);
		Renderer::DispatchCompute(m_MainCommandBuffer, m_GTAOComputePass, nullptr, m_GTAOWorkGroups, pushConstantBuffer);
		Renderer::EndComputePass(m_MainCommandBuffer, m_GTAOComputePass);
		m_MainCommandBuffer->EndTimestampQuery(m_GPUTimeQueries.GTAOPassQuery);
	}

	void SceneRenderer::GTAODenoiseCompute()
	{
		m_GTAODenoiseConstants.DenoiseBlurBeta = GTAODataCB.DenoiseBlurBeta;
		m_GTAODenoiseConstants.HalfRes = GTAODataCB.HalfRes;
		const Buffer pushConstantBuffer(&m_GTAODenoiseConstants, sizeof(GTAODenoiseConstants));

		m_GPUTimeQueries.GTAODenoisePassQuery = m_MainCommandBuffer->BeginTimestampQuery();
		for (uint32_t pass = 0; pass < (uint32_t)m_Options.GTAODenoisePasses; pass++)
		{
			auto renderPass = m_GTAODenoisePass[uint32_t(pass % 2 != 0)];
			Renderer::BeginComputePass(m_MainCommandBuffer, renderPass);
			Renderer::DispatchCompute(m_MainCommandBuffer, renderPass, nullptr, m_GTAODenoiseWorkGroups, pushConstantBuffer);
			Renderer::EndComputePass(m_MainCommandBuffer, renderPass);
		}
		m_MainCommandBuffer->EndTimestampQuery(m_GPUTimeQueries.GTAODenoisePassQuery);
	}

	void SceneRenderer::AOComposite()
	{
		m_GPUTimeQueries.AOCompositePassQuery = m_MainCommandBuffer->BeginTimestampQuery();
		Renderer::BeginRenderPass(m_MainCommandBuffer, m_AOCompositePass);
		Renderer::SubmitFullscreenQuad(m_MainCommandBuffer, m_AOCompositePass->GetSpecification().Pipeline, m_AOCompositeMaterial);
		Renderer::EndRenderPass(m_MainCommandBuffer);
		m_MainCommandBuffer->EndTimestampQuery(m_GPUTimeQueries.AOCompositePassQuery);
	}

	void SceneRenderer::JumpFloodPass()
	{
		BEY_PROFILE_FUNC();

		m_GPUTimeQueries.JumpFloodPassQuery = m_MainCommandBuffer->BeginTimestampQuery();
		Renderer::BeginRenderPass(m_MainCommandBuffer, m_JumpFloodInitPass);

		Renderer::SubmitFullscreenQuad(m_MainCommandBuffer, m_JumpFloodInitPass->GetSpecification().Pipeline, m_JumpFloodInitMaterial);
		Renderer::EndRenderPass(m_MainCommandBuffer);

		int steps = 2;
		int step = (int)glm::round(glm::pow<int>(steps - 1, 2));
		int index = 0;
		Buffer vertexOverrides;
		Ref<Framebuffer> passFB = m_JumpFloodPass[0]->GetTargetFramebuffer();
		glm::vec2 texelSize = { 1.0f / (float)passFB->GetWidth(), 1.0f / (float)passFB->GetHeight() };
		vertexOverrides.Allocate(sizeof(glm::vec2) + sizeof(int));
		vertexOverrides.Write(glm::value_ptr(texelSize), sizeof(glm::vec2));
		while (step != 0)
		{
			vertexOverrides.Write(&step, sizeof(int), sizeof(glm::vec2));

			Renderer::BeginRenderPass(m_MainCommandBuffer, m_JumpFloodPass[index]);
			Renderer::SubmitFullscreenQuadWithOverrides(m_MainCommandBuffer, m_JumpFloodPass[index]->GetSpecification().Pipeline, m_JumpFloodPassMaterial[index], vertexOverrides, Buffer());
			Renderer::EndRenderPass(m_MainCommandBuffer);

			index = (index + 1) % 2;
			step /= 2;
		}

		vertexOverrides.Release();

		//m_JumpFloodCompositeMaterial->Set("u_Texture", m_TempFramebuffers[1]->GetImage());
		m_MainCommandBuffer->EndTimestampQuery(m_GPUTimeQueries.JumpFloodPassQuery);
	}

	void SceneRenderer::SSRCompute()
	{
		BEY_PROFILE_FUNC();

		const Buffer pushConstantsBuffer(&m_SSROptions, sizeof(SSROptionsUB));

		m_GPUTimeQueries.SSRQuery = m_MainCommandBuffer->BeginTimestampQuery();
		Renderer::BeginComputePass(m_MainCommandBuffer, m_SSRPass);
		Renderer::DispatchCompute(m_MainCommandBuffer, m_SSRPass, nullptr, m_SSRWorkGroups, pushConstantsBuffer);
		Renderer::EndComputePass(m_MainCommandBuffer, m_SSRPass);
		m_MainCommandBuffer->EndTimestampQuery(m_GPUTimeQueries.SSRQuery);
	}

	void SceneRenderer::SSRCompositePass()
	{
		// Currently scales the SSR, renders with transparency.
		// The alpha channel is the confidence.
		m_GPUTimeQueries.SSRCompositeQuery = m_MainCommandBuffer->BeginTimestampQuery();
		Renderer::BeginRenderPass(m_MainCommandBuffer, m_SSRCompositePass);
		Renderer::SubmitFullscreenQuad(m_MainCommandBuffer, m_SSRCompositePass->GetPipeline(), nullptr);
		Renderer::EndRenderPass(m_MainCommandBuffer);
		m_MainCommandBuffer->EndTimestampQuery(m_GPUTimeQueries.SSRCompositeQuery);
	}

	void SceneRenderer::BloomCompute()
	{
		glm::uvec3 workGroups(0);

		struct BloomComputePushConstants
		{
			glm::vec4 Params;
			float LOD = 0.0f;
			int Mode = 0; // 0 = prefilter, 1 = downsample, 2 = firstUpsample, 3 = upsample
		} bloomComputePushConstants;
		bloomComputePushConstants.Params = { m_BloomSettings.Threshold, m_BloomSettings.Threshold - m_BloomSettings.Knee, m_BloomSettings.Knee * 2.0f, 0.25f / m_BloomSettings.Knee };
		bloomComputePushConstants.Mode = 0;

		m_GPUTimeQueries.BloomComputePassQuery = m_MainCommandBuffer->BeginTimestampQuery();

		Renderer::BeginComputePass(m_MainCommandBuffer, m_BloomComputePass);

		// ===================
		//      Prefilter
		// ===================
		Renderer::BeginGPUPerfMarker(m_MainCommandBuffer, "Bloom-Prefilter");
		{
			workGroups = { m_BloomComputeTextures[0].Texture->GetWidth() / m_BloomComputeWorkgroupSize, m_BloomComputeTextures[0].Texture->GetHeight() / m_BloomComputeWorkgroupSize, 1 };
			Renderer::DispatchCompute(m_MainCommandBuffer, m_BloomComputePass, m_BloomComputeMaterials.PrefilterMaterial, workGroups, Buffer(&bloomComputePushConstants, sizeof(bloomComputePushConstants)));
			m_BloomComputePipeline->ImageMemoryBarrier(m_MainCommandBuffer, m_BloomComputeTextures[0].Texture->GetImage(), ResourceAccessFlags::ShaderWrite, ResourceAccessFlags::ShaderRead);
		}
		Renderer::EndGPUPerfMarker(m_MainCommandBuffer);

		// ===================
		//      Downsample
		// ===================
		bloomComputePushConstants.Mode = 1;
		uint32_t mips = m_BloomComputeTextures[0].Texture->GetMipLevelCount() - 2;
		Renderer::BeginGPUPerfMarker(m_MainCommandBuffer, "Bloom-DownSample");
		{
			for (uint32_t i = 1; i < mips; i++)
			{
				auto [mipWidth, mipHeight] = m_BloomComputeTextures[0].Texture->GetMipSize(i);
				workGroups = { (uint32_t)glm::ceil((float)mipWidth / (float)m_BloomComputeWorkgroupSize) ,(uint32_t)glm::ceil((float)mipHeight / (float)m_BloomComputeWorkgroupSize), 1 };

				bloomComputePushConstants.LOD = i - 1.0f;
				Renderer::DispatchCompute(m_MainCommandBuffer, m_BloomComputePass, m_BloomComputeMaterials.DownsampleAMaterials[i], workGroups, Buffer(&bloomComputePushConstants, sizeof(bloomComputePushConstants)));

				m_BloomComputePipeline->ImageMemoryBarrier(m_MainCommandBuffer, m_BloomComputeTextures[1].Texture->GetImage(), ResourceAccessFlags::ShaderWrite, ResourceAccessFlags::ShaderRead);

				bloomComputePushConstants.LOD = (float)i;
				Renderer::DispatchCompute(m_MainCommandBuffer, m_BloomComputePass, m_BloomComputeMaterials.DownsampleBMaterials[i], workGroups, Buffer(&bloomComputePushConstants, sizeof(bloomComputePushConstants)));

				m_BloomComputePipeline->ImageMemoryBarrier(m_MainCommandBuffer, m_BloomComputeTextures[0].Texture->GetImage(), ResourceAccessFlags::ShaderWrite, ResourceAccessFlags::ShaderRead);
			}
		}
		Renderer::EndGPUPerfMarker(m_MainCommandBuffer);


		// ===================
		//   First Upsample
		// ===================
		bloomComputePushConstants.Mode = 2;
		bloomComputePushConstants.LOD--;
		Renderer::BeginGPUPerfMarker(m_MainCommandBuffer, "Bloom-FirstUpsamle");
		{
			auto [mipWidth, mipHeight] = m_BloomComputeTextures[2].Texture->GetMipSize(mips - 2);
			workGroups.x = (uint32_t)glm::ceil((float)mipWidth / (float)m_BloomComputeWorkgroupSize);
			workGroups.y = (uint32_t)glm::ceil((float)mipHeight / (float)m_BloomComputeWorkgroupSize);

			Renderer::DispatchCompute(m_MainCommandBuffer, m_BloomComputePass, m_BloomComputeMaterials.FirstUpsampleMaterial, workGroups, Buffer(&bloomComputePushConstants, sizeof(bloomComputePushConstants)));
			m_BloomComputePipeline->ImageMemoryBarrier(m_MainCommandBuffer, m_BloomComputeTextures[2].Texture->GetImage(), ResourceAccessFlags::ShaderWrite, ResourceAccessFlags::ShaderRead);
		}
		Renderer::EndGPUPerfMarker(m_MainCommandBuffer);

		// ===================
		//      Upsample
		// ===================
		Renderer::BeginGPUPerfMarker(m_MainCommandBuffer, "Bloom-Upsample");
		{
			bloomComputePushConstants.Mode = 3;
			for (int32_t mip = mips - 3; mip >= 0; mip--)
			{
				auto [mipWidth, mipHeight] = m_BloomComputeTextures[2].Texture->GetMipSize(mip);
				workGroups.x = (uint32_t)glm::ceil((float)mipWidth / (float)m_BloomComputeWorkgroupSize);
				workGroups.y = (uint32_t)glm::ceil((float)mipHeight / (float)m_BloomComputeWorkgroupSize);

				bloomComputePushConstants.LOD = (float)mip;
				Renderer::DispatchCompute(m_MainCommandBuffer, m_BloomComputePass, m_BloomComputeMaterials.UpsampleMaterials[mip], workGroups, Buffer(&bloomComputePushConstants, sizeof(bloomComputePushConstants)));

				m_BloomComputePipeline->ImageMemoryBarrier(m_MainCommandBuffer, m_BloomComputeTextures[2].Texture->GetImage(), ResourceAccessFlags::ShaderWrite, ResourceAccessFlags::ShaderRead);
			}
		}
		Renderer::EndGPUPerfMarker(m_MainCommandBuffer);

		Renderer::EndComputePass(m_MainCommandBuffer, m_BloomComputePass);
		m_MainCommandBuffer->EndTimestampQuery(m_GPUTimeQueries.BloomComputePassQuery);
	}

	void SceneRenderer::EdgeDetectionPass()
	{
		Renderer::BeginRenderPass(m_MainCommandBuffer, m_EdgeDetectionPass);
		Renderer::SubmitFullscreenQuad(m_MainCommandBuffer, m_EdgeDetectionPass->GetPipeline(), nullptr);
		Renderer::EndRenderPass(m_MainCommandBuffer);
	}

	void SceneRenderer::CompositePass()
	{
		BEY_PROFILE_FUNC();

		uint32_t frameIndex = Renderer::GetCurrentFrameIndex();

		m_GPUTimeQueries.CompositePassQuery = m_MainCommandBuffer->BeginTimestampQuery();
		m_CompositePass->SetInput("u_Texture", (!m_DLSSSettings.FakeDLSS && m_DLSSSettings.Enable) && m_DLSSImage ? m_DLSSImage : m_GeometryPass->GetOutput(0));

		//RendererUtils::ImageBarrier barrier
		//{
		//	.srcAccessMask = RendererUtils::COLOR_ATTACHMENT_WRITE_BIT,
		//	.dstAccessMask = RendererUtils::COLOR_ATTACHMENT_READ_BIT | RendererUtils::COLOR_ATTACHMENT_WRITE_BIT,
		//	.oldImageLayout = RendererUtils::IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		//	.newImageLayout = RendererUtils::IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		//	.srcStageMask = RendererUtils::VK_PIPELINE_STAGE_,
		//	.dstStageMask = ,
		//	.subresourceRange = 
		//}

		Renderer::BeginRenderPass(m_MainCommandBuffer, m_CompositePass);

		float exposure = m_SceneData.SceneCamera.Camera->GetExposure();
		m_CompositeMaterial->Set("u_Uniforms.Exposure", exposure);
		if (m_BloomSettings.Enabled)
		{
			m_CompositeMaterial->Set("u_Uniforms.BloomIntensity", m_BloomSettings.Intensity);
			m_CompositeMaterial->Set("u_Uniforms.BloomDirtIntensity", m_BloomSettings.DirtIntensity);
		}
		else
		{
			m_CompositeMaterial->Set("u_Uniforms.BloomIntensity", 0.0f);
			m_CompositeMaterial->Set("u_Uniforms.BloomDirtIntensity", 0.0f);
		}

		m_CompositeMaterial->Set("u_Uniforms.Opacity", m_CompositeSettings.Opacity);
		m_CompositeMaterial->Set("u_Uniforms.Tonemapper", m_CompositeSettings.Tonemapper);
		m_CompositeMaterial->Set("u_Uniforms.GrainStrength", m_CompositeSettings.GrainStrength);
		m_CompositeMaterial->Set("u_Uniforms.Time", Application::Get().GetTime());

		SceneRenderer::BeginGPUPerfMarker(m_MainCommandBuffer, "Composite");
		Renderer::SubmitFullscreenQuad(m_MainCommandBuffer, m_CompositePass->GetPipeline(), m_CompositeMaterial);
		SceneRenderer::EndGPUPerfMarker(m_MainCommandBuffer);

		Renderer::EndRenderPass(m_MainCommandBuffer);

		if (m_DOFSettings.Enabled)
		{
			//Renderer::EndRenderPass(m_MainCommandBuffer);

			Renderer::BeginRenderPass(m_MainCommandBuffer, m_DOFPass);
			//m_DOFMaterial->Set("u_Texture", m_CompositePipeline->GetSpecification().RenderPass->GetSpecification().TargetFramebuffer->GetImage());
			//m_DOFMaterial->Set("u_DepthTexture", m_PreDepthPipeline->GetSpecification().RenderPass->GetSpecification().TargetFramebuffer->GetDepthImage());
			m_DOFMaterial->Set("u_Uniforms.DOFParams", glm::vec2(m_DOFSettings.FocusDistance, m_DOFSettings.BlurSize));

			Renderer::SubmitFullscreenQuad(m_MainCommandBuffer, m_DOFPass->GetPipeline(), m_DOFMaterial);

			//SceneRenderer::BeginGPUPerfMarker(m_MainCommandBuffer, "JumpFlood-Composite");
			//Renderer::SubmitFullscreenQuad(m_MainCommandBuffer, m_JumpFloodCompositePipeline, nullptr, m_JumpFloodCompositeMaterial);
			//SceneRenderer::EndGPUPerfMarker(m_MainCommandBuffer);

			Renderer::EndRenderPass(m_MainCommandBuffer);

			// Copy DOF image to composite pipeline
			Renderer::CopyImage(m_MainCommandBuffer, m_DOFPass->GetTargetFramebuffer()->GetImage(), m_CompositePass->GetTargetFramebuffer()->GetImage());

			// WIP - will later be used for debugging/editor mouse picking
#if 0
			if (m_ReadBackImage)
			{
				Renderer::CopyImage(m_MainCommandBuffer,
					m_DOFPipeline->GetSpecification().RenderPass->GetSpecification().TargetFramebuffer->GetImage(),
					m_ReadBackImage);

				{
					auto alloc = m_ReadBackImage.As<VulkanImage2D>()->GetImageInfo().MemoryAlloc;
					VulkanAllocator allocator("SceneRenderer");
					glm::vec4* mappedMem = allocator.MapMemory<glm::vec4>(alloc);
					delete[] m_ReadBackBuffer;
					m_ReadBackBuffer = new glm::vec4[m_ReadBackImage->GetWidth() * m_ReadBackImage->GetHeight()];
					memcpy(m_ReadBackBuffer, mappedMem, sizeof(glm::vec4) * m_ReadBackImage->GetWidth() * m_ReadBackImage->GetHeight());
					allocator.UnmapMemory(alloc);
				}
			}
#endif
		}

		// Grid
		if (GetOptions().ShowGrid)
		{
			const static glm::mat4 transform = glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f)) * glm::scale(glm::mat4(1.0f), glm::vec3(8.0f));
			Renderer::BeginRenderPass(m_MainCommandBuffer, m_GridRenderPass);
			Renderer::RenderQuad(m_MainCommandBuffer, m_GridRenderPass->GetPipeline(), m_GridMaterial, transform);
			Renderer::EndRenderPass(m_MainCommandBuffer);
		}

		// TODO: don't do this in runtime!
		if (m_Specification.JumpFloodPass)
		{
			Renderer::BeginRenderPass(m_MainCommandBuffer, m_JumpFloodCompositePass);
			Renderer::SubmitFullscreenQuad(m_MainCommandBuffer, m_JumpFloodCompositePass->GetSpecification().Pipeline, m_JumpFloodCompositeMaterial);
			Renderer::EndRenderPass(m_MainCommandBuffer);
		}

		m_MainCommandBuffer->EndTimestampQuery(m_GPUTimeQueries.CompositePassQuery);

		if (m_Options.ShowSelectedInWireframe)
		{
			Renderer::BeginRenderPass(m_MainCommandBuffer, m_GeometryWireframePass);

			SceneRenderer::BeginGPUPerfMarker(m_MainCommandBuffer, "Static Meshes Wireframe");
			for (auto& [mk, dc] : m_SelectedStaticMeshDrawList)
			{
				const auto& transformData = m_MeshTransformMap.at(mk);
				Renderer::RenderStaticMeshWithMaterial(m_MainCommandBuffer, m_GeometryWireframePass->GetPipeline(), dc.StaticMesh, dc.SubmeshIndex, m_WireframeMaterial, transformData.TransformIndex, dc.InstanceCount);
			}

			for (auto& [mk, dc] : m_SelectedMeshDrawList)
			{
				if (!dc.IsRigged)
				{
					const auto& transformData = m_MeshTransformMap.at(mk);
					Renderer::RenderMeshWithMaterial(m_MainCommandBuffer, m_GeometryWireframePass->GetPipeline(), dc.Mesh, dc.SubmeshIndex, 0, transformData.TransformIndex, dc.InstanceCount, m_WireframeMaterial);
				}
			}

			SceneRenderer::EndGPUPerfMarker(m_MainCommandBuffer);
			Renderer::EndRenderPass(m_MainCommandBuffer);

			Renderer::BeginRenderPass(m_MainCommandBuffer, m_GeometryWireframeAnimPass);
			SceneRenderer::BeginGPUPerfMarker(m_MainCommandBuffer, "Dynamic Meshes Wireframe");
			for (auto& [mk, dc] : m_SelectedMeshDrawList)
			{
				const auto& transformData = m_MeshTransformMap.at(mk);
				if (dc.IsRigged)
				{
					const auto& boneTransformsData = m_MeshBoneTransformsMap.at(mk);
					Renderer::RenderMeshWithMaterial(m_MainCommandBuffer, m_GeometryWireframeAnimPass->GetPipeline(), dc.Mesh, dc.SubmeshIndex, boneTransformsData.BoneTransformsBaseIndex + dc.InstanceOffset, transformData.TransformIndex, dc.InstanceCount, m_WireframeMaterial);
				}
			}
			SceneRenderer::EndGPUPerfMarker(m_MainCommandBuffer);

			Renderer::EndRenderPass(m_MainCommandBuffer);
		}

		if (m_Options.ShowPhysicsColliders)
		{
			auto staticPass = m_Options.ShowPhysicsCollidersOnTop ? m_GeometryWireframeOnTopPass : m_GeometryWireframePass;
			auto animPass = m_Options.ShowPhysicsCollidersOnTop ? m_GeometryWireframeOnTopAnimPass : m_GeometryWireframeAnimPass;

			SceneRenderer::BeginGPUPerfMarker(m_MainCommandBuffer, "Static Meshes Collider");
			Renderer::BeginRenderPass(m_MainCommandBuffer, staticPass);
			for (auto& [mk, dc] : m_StaticColliderDrawList)
			{
				BEY_CORE_VERIFY(m_MeshTransformMap.find(mk) != m_MeshTransformMap.end());
				const auto& transformData = m_MeshTransformMap.at(mk);
				Renderer::RenderStaticMeshWithMaterial(m_MainCommandBuffer, staticPass->GetPipeline(), dc.StaticMesh, dc.SubmeshIndex, dc.OverrideMaterial, transformData.TransformIndex, dc.InstanceCount);
			}

			for (auto& [mk, dc] : m_ColliderDrawList)
			{
				BEY_CORE_VERIFY(m_MeshTransformMap.find(mk) != m_MeshTransformMap.end());
				const auto& transformData = m_MeshTransformMap.at(mk);
				if (!dc.IsRigged)
					Renderer::RenderMeshWithMaterial(m_MainCommandBuffer, staticPass->GetPipeline(), dc.Mesh, dc.SubmeshIndex, 0, transformData.TransformIndex, dc.InstanceCount, m_SimpleColliderMaterial);
			}

			Renderer::EndRenderPass(m_MainCommandBuffer);
			SceneRenderer::EndGPUPerfMarker(m_MainCommandBuffer);

			SceneRenderer::BeginGPUPerfMarker(m_MainCommandBuffer, "Animated Meshes Collider");
			Renderer::BeginRenderPass(m_MainCommandBuffer, animPass);
			for (auto& [mk, dc] : m_ColliderDrawList)
			{
				BEY_CORE_VERIFY(m_MeshTransformMap.find(mk) != m_MeshTransformMap.end());
				const auto& transformData = m_MeshTransformMap.at(mk);
				if (dc.IsRigged)
				{
					const auto& boneTransformsData = m_MeshBoneTransformsMap.at(mk);
					Renderer::RenderMeshWithMaterial(m_MainCommandBuffer, animPass->GetPipeline(), dc.Mesh, dc.SubmeshIndex, boneTransformsData.BoneTransformsBaseIndex, transformData.TransformIndex, dc.InstanceCount, m_SimpleColliderMaterial);
				}
				else
				{
					Renderer::RenderMeshWithMaterial(m_MainCommandBuffer, animPass->GetPipeline(), dc.Mesh, dc.SubmeshIndex, {}, transformData.TransformIndex, dc.InstanceCount, m_SimpleColliderMaterial);
				}
			}

			Renderer::EndRenderPass(m_MainCommandBuffer);
			SceneRenderer::EndGPUPerfMarker(m_MainCommandBuffer);
		}
	}

	void SceneRenderer::FlushDrawList()
	{
		if (m_ResourcesCreated && m_RenderWidth > 0 && m_RenderHeight > 0)
		{
			// Reset GPU time queries
			m_GPUTimeQueries = SceneRenderer::GPUTimeQueries();

			PreRender();
			if (m_RaytracingSettings.Mode != RaytracingMode::None)
				BuildAccelerationStructures();

			m_MainCommandBuffer->Begin();
			//m_CommandBuffers[CmdBuffers::eDrawRaytracing]->Begin();

			//PrepareDDGIVolumes();


			// Main render passes
			ShadowMapPass();
			SpotShadowMapPass();
			PreDepthPass();
			HZBCompute();
			MotionVectorsCompute();
			PreIntegration();
			LightCullingPass();
			SkyboxPass();

			GeometryPass();
			if (VulkanContext::GetCurrentDevice()->IsRaytracingSupported())
				PathTracingPass();
			//m_CommandBuffers[CmdBuffers::eDrawRaytracing]->End();

			//DDGIRaytracing();
			//DDGIIrradiance();
			//DDGIVis();

			// GTAO
			if (m_Options.EnableGTAO)
			{
				GTAOCompute();
				GTAODenoiseCompute();
			}

			if (m_Options.EnableGTAO)
				AOComposite();

			if (VulkanContext::GetCurrentDevice()->IsDLSSSupported())
				DLSSPass();

			PreConvolutionCompute();

			// Post-processing
			if (m_Specification.JumpFloodPass)
				JumpFloodPass();

			//SSR
			if (m_Options.EnableSSR)
			{
				SSRCompute();
				SSRCompositePass();
			}

			if (m_Specification.EnableEdgeOutlineEffect)
				EdgeDetectionPass();

			BloomCompute();
			CompositePass();
			m_MainCommandBuffer->End();

			m_AccumulatedFrames++;
			m_AccumulatedPathtracingFrames++;

			if (m_RaytracingSettings.Mode != RaytracingMode::None) // if ray tracing
			{
				m_CommandBuffers[CmdBuffers::eTLASBuild]->Submit(m_GPUSemaphores.at(GPUSemaphoreUsage::TLASBuild));
				m_MainCommandBuffer->Submit(nullptr, m_GPUSemaphores.at(GPUSemaphoreUsage::TLASBuild));
			}
			else
			{
				m_MainCommandBuffer->Submit();
			}
		}
		else
		{
			// Empty pass
			m_MainCommandBuffer->Begin();

			ClearPass();

			m_MainCommandBuffer->End();
			m_MainCommandBuffer->Submit();
		}

		UpdateStatistics();

		m_DrawList.clear();
		m_TransparentDrawList.clear();
		m_SelectedMeshDrawList.clear();
		m_ShadowPassDrawList.clear();

		m_StaticMeshDrawList.clear();
		m_TransparentStaticMeshDrawList.clear();
		m_SelectedStaticMeshDrawList.clear();
		m_StaticMeshShadowPassDrawList.clear();

		m_ColliderDrawList.clear();
		m_StaticColliderDrawList.clear();
		m_SceneData = {};

		m_MeshTransformMap.clear();
		m_MeshBoneTransformsMap.clear();
	}

	void SceneRenderer::PreRender()
	{
		BEY_PROFILE_FUNC();

		uint32_t framesInFlight = Renderer::GetConfig().FramesInFlight;
		uint32_t frameIndex = Renderer::GetCurrentFrameIndex();
		uint32_t offset = 0;
		{
			for (auto& transformData : m_MeshTransformMap | std::views::values)
			{
				transformData.TransformIndex = offset;
				for (const auto& transform : transformData.Transforms)
				{
					m_SubmeshTransformBuffers[frameIndex].Data[offset] = transform;
					offset++;
				}
			}
		}

		{
			for (uint32_t i = 0; i < offset; i++)
			{
				//m_TransformBuffers[frameIndex].Data[offset].CurrentMRow = m_SubmeshTransformBuffers[frameIndex].Data[offset];
				std::memcpy(m_TransformBuffers[frameIndex].Data[i].CurrentMRow, m_SubmeshTransformBuffers[frameIndex].Data[i].MRow, 48);
				std::memcpy(m_TransformBuffers[frameIndex].Data[i].PreviousMRow, m_SubmeshTransformBuffers[(frameIndex + framesInFlight - 1) % framesInFlight].Data[i].MRow, 48);
				if (m_RaytracingSettings.Mode != RaytracingMode::None)
					m_MainRaytracer->GetObjDescs()[i].TransformIndex = i;
			}

			// Update prev transform after it's consumed. 
			for (auto& [meshKey, transformData] : m_MeshTransformMap)
			{
				auto& historyTransform = m_TransformHistoryMap[meshKey];
				historyTransform = transformData;
			}

			// Upload all transforms
			m_SBSTransforms->Get()->SetData(m_TransformBuffers[frameIndex].Data, static_cast<uint32_t>(offset * sizeof(TransformData)));
		}

		uint32_t index = 0;
		for (auto& [meshKey, boneTransformsData] : m_MeshBoneTransformsMap)
		{
			boneTransformsData.BoneTransformsBaseIndex = index;
			for (const auto& boneTransforms : boneTransformsData.BoneTransformsData)
			{
				m_BoneTransformsData[index++] = boneTransforms;
			}
		}

		if (index > 0)
		{
			Ref<SceneRenderer> instance = this;
			Renderer::Submit([instance, index]() mutable
			{
				instance->m_SBSBoneTransforms->RT_Get()->RT_SetData(instance->m_BoneTransformsData, static_cast<uint32_t>(index * sizeof(BoneTransforms)));
			});
		}


	}

	void SceneRenderer::BuildAccelerationStructures()
	{
		if (m_RaytracingSettings.Mode != RaytracingMode::None)
		{
			//m_GPUTimeQueries.BuildAccelerationStructuresQuery = m_MainCommandBuffer->BeginTimestampQuery();
			m_CommandBuffers[eTLASBuild]->Begin();
			m_MainRaytracer->BuildTlas(m_CommandBuffers[eTLASBuild]);
			//m_MainRaytracer->BuildTlas(m_MainCommandBuffer);
			m_CommandBuffers[eTLASBuild]->End();

			//m_MainRaytracer->BuildTlas(nullptr);

			m_MainRaytracer->ClearInternalInstances();
			//m_RebuildTlas = false;
			//m_MainCommandBuffer->EndTimestampQuery(m_GPUTimeQueries.BuildAccelerationStructuresQuery);

			Renderer::Submit([instance = Ref(this)]() mutable
			{
				const uint32_t frame = Renderer::RT_GetCurrentFrameIndex();

				{
					const auto& data = instance->m_MainRaytracer->GetObjDescs();
					if (data.empty())
						return;
					auto storageBuffer = instance->m_SBSObjectSpecs->Get(frame);
					storageBuffer->RT_Resize(uint32_t(instance->m_MainRaytracer->GetObjDescs().size() * sizeof(data[0])));
					storageBuffer->RT_SetData(data.data(), uint32_t(instance->m_MainRaytracer->GetObjDescs().size() * sizeof(data[0])));
				}
				{
					const auto& data = instance->m_MainRaytracer->GetMaterials();
					if (data.empty())
						return;

					auto storageBuffer = instance->m_SBSMaterialBuffer->Get(frame);
					storageBuffer->RT_Resize(uint32_t(instance->m_MainRaytracer->GetMaterials().size() * sizeof(data[0])));
					storageBuffer->RT_SetData(data.data(), uint32_t(instance->m_MainRaytracer->GetMaterials().size() * sizeof(data[0])));
				}
			});
		}
	}

	void SceneRenderer::CopyToBoneTransformStorage(const MeshKey& meshKey, const Ref<MeshSource>& meshSource, const std::vector<glm::mat4>& boneTransforms)
	{
		auto& boneTransformStorage = m_MeshBoneTransformsMap[meshKey].BoneTransformsData.emplace_back();
		if (boneTransforms.empty())
		{
			boneTransformStorage.fill(glm::identity<glm::mat4>());
		}
		else
		{
			for (size_t i = 0; i < meshSource->m_BoneInfo.size(); ++i)
			{
				const auto submeshInvTransform = meshSource->m_BoneInfo[i].SubMeshInverseTransform;
				const auto boneTransform = boneTransforms[meshSource->m_BoneInfo[i].BoneIndex];
				const auto invBindPose = meshSource->m_BoneInfo[i].InverseBindPose;
				boneTransformStorage[i] = submeshInvTransform * boneTransform * invBindPose;
			}
		}
	}

	void SceneRenderer::CreateBloomPassMaterials()
	{
		auto inputImage = m_GeometryPass->GetOutput(0);

		// Prefilter
		m_BloomComputeMaterials.PrefilterMaterial = Material::Create(m_BloomComputePass->GetShader());
		m_BloomComputeMaterials.PrefilterMaterial->Set("o_Image", m_BloomComputeTextures[0].ImageViews[0]);
		m_BloomComputeMaterials.PrefilterMaterial->Set("u_Texture", inputImage);
		m_BloomComputeMaterials.PrefilterMaterial->Set("u_BloomTexture", inputImage);

		// Downsample
		uint32_t mips = m_BloomComputeTextures[0].Texture->GetMipLevelCount() - 2;
		m_BloomComputeMaterials.DownsampleAMaterials.resize(mips);
		m_BloomComputeMaterials.DownsampleBMaterials.resize(mips);
		for (uint32_t i = 1; i < mips; i++)
		{
			m_BloomComputeMaterials.DownsampleAMaterials[i] = Material::Create(m_BloomComputePass->GetShader());
			m_BloomComputeMaterials.DownsampleAMaterials[i]->Set("o_Image", m_BloomComputeTextures[1].ImageViews[i]);
			m_BloomComputeMaterials.DownsampleAMaterials[i]->Set("u_Texture", m_BloomComputeTextures[0].Texture);
			m_BloomComputeMaterials.DownsampleAMaterials[i]->Set("u_BloomTexture", inputImage);

			m_BloomComputeMaterials.DownsampleBMaterials[i] = Material::Create(m_BloomComputePass->GetShader());
			m_BloomComputeMaterials.DownsampleBMaterials[i]->Set("o_Image", m_BloomComputeTextures[0].ImageViews[i]);
			m_BloomComputeMaterials.DownsampleBMaterials[i]->Set("u_Texture", m_BloomComputeTextures[1].Texture);
			m_BloomComputeMaterials.DownsampleBMaterials[i]->Set("u_BloomTexture", inputImage);
		}

		// Upsample
		m_BloomComputeMaterials.FirstUpsampleMaterial = Material::Create(m_BloomComputePass->GetShader());
		m_BloomComputeMaterials.FirstUpsampleMaterial->Set("o_Image", m_BloomComputeTextures[2].ImageViews[mips - 2]);
		m_BloomComputeMaterials.FirstUpsampleMaterial->Set("u_Texture", m_BloomComputeTextures[0].Texture);
		m_BloomComputeMaterials.FirstUpsampleMaterial->Set("u_BloomTexture", inputImage);

		m_BloomComputeMaterials.UpsampleMaterials.resize(mips - 3 + 1);
		for (int32_t mip = mips - 3; mip >= 0; mip--)
		{
			m_BloomComputeMaterials.UpsampleMaterials[mip] = Material::Create(m_BloomComputePass->GetShader());
			m_BloomComputeMaterials.UpsampleMaterials[mip]->Set("o_Image", m_BloomComputeTextures[2].ImageViews[mip]);
			m_BloomComputeMaterials.UpsampleMaterials[mip]->Set("u_Texture", m_BloomComputeTextures[0].Texture);
			m_BloomComputeMaterials.UpsampleMaterials[mip]->Set("u_BloomTexture", m_BloomComputeTextures[2].Texture);
		}
	}

	void SceneRenderer::CreatePreConvolutionPassMaterials()
	{
		auto inputImage = m_SkyboxPass->GetOutput(0);

		uint32_t mips = m_PreConvolutedTexture.Texture->GetMipLevelCount();
		m_PreConvolutionMaterials.resize(mips);

		for (uint32_t i = 0; i < mips; i++)
		{
			m_PreConvolutionMaterials[i] = Material::Create(m_PreConvolutionComputePass->GetShader());
			m_PreConvolutionMaterials[i]->Set("o_Image", m_PreConvolutedTexture.ImageViews[i]);
			m_PreConvolutionMaterials[i]->Set("u_Input", i == 0 ? inputImage : m_PreConvolutedTexture.Texture->GetImage());
		}

	}

	void SceneRenderer::CreateHZBPassMaterials()
	{
		constexpr uint32_t maxMipBatchSize = 4;
		const uint32_t hzbMipCount = m_HierarchicalDepthTexture.Texture->GetMipLevelCount();

		Ref<Shader> hzbShader = Renderer::GetShaderLibrary()->Get("HZB");

		uint32_t materialIndex = 0;
		m_HZBMaterials.resize(Math::DivideAndRoundUp(hzbMipCount, 4u));
		for (uint32_t startDestMip = 0; startDestMip < hzbMipCount; startDestMip += maxMipBatchSize)
		{
			Ref<Material> material = Material::Create(hzbShader);
			m_HZBMaterials[materialIndex++] = material;

			if (startDestMip == 0)
				material->Set("u_InputDepth", m_PreDepthPass->GetDepthOutput());
			else
				material->Set("u_InputDepth", m_HierarchicalDepthTexture.Texture);

			const uint32_t endDestMip = glm::min(startDestMip + maxMipBatchSize, hzbMipCount);
			uint32_t destMip;
			for (destMip = startDestMip; destMip < endDestMip; ++destMip)
			{
				uint32_t index = destMip - startDestMip;
				material->Set("o_HZB", m_HierarchicalDepthTexture.ImageViews[destMip], index);
			}

			// Fill the rest .. or we could enable the null descriptor feature
			destMip -= startDestMip;
			for (; destMip < maxMipBatchSize; ++destMip)
			{
				material->Set("o_HZB", m_HierarchicalDepthTexture.ImageViews[hzbMipCount - 1], destMip); // could be white texture?
			}
		}
	}

	void SceneRenderer::CreatePreIntegrationPassMaterials()
	{
		Ref<Shader> preIntegrationShader = Renderer::GetShaderLibrary()->Get("Pre-Integration");

		Ref<Texture2D> visibilityTexture = m_PreIntegrationVisibilityTexture.Texture;
		m_PreIntegrationMaterials.resize(visibilityTexture->GetMipLevelCount() - 1);
		for (uint32_t i = 0; i < visibilityTexture->GetMipLevelCount() - 1; i++)
		{
			Ref<Material> material = Material::Create(preIntegrationShader);
			m_PreIntegrationMaterials[i] = material;

			material->Set("o_VisibilityImage", m_PreIntegrationVisibilityTexture.ImageViews[i]);
			material->Set("u_VisibilityTex", visibilityTexture);
			material->Set("u_HZB", m_HierarchicalDepthTexture.Texture);
		}
	}

	void SceneRenderer::ClearPass()
	{
		BEY_PROFILE_FUNC();

		Renderer::BeginRenderPass(m_MainCommandBuffer, m_PreDepthPass, true);
		Renderer::EndRenderPass(m_MainCommandBuffer);

		Renderer::BeginRenderPass(m_MainCommandBuffer, m_CompositePass, true);
		Renderer::EndRenderPass(m_MainCommandBuffer);

		//Renderer::BeginRenderPass(m_MainCommandBuffer, m_DOFPipeline->GetSpecification().RenderPass, true);
		//Renderer::EndRenderPass(m_MainCommandBuffer);
	}

	Ref<RasterPipeline> SceneRenderer::GetFinalPipeline()
	{
		return m_CompositePass->GetSpecification().Pipeline;
	}

	Ref<RenderPass> SceneRenderer::GetFinalRenderPass()
	{
		return m_CompositePass;
	}

	Ref<Image2D> SceneRenderer::GetFinalPassImage()
	{
		if (!m_ResourcesCreated)
			return nullptr;

		//		return GetFinalPipeline()->GetSpecification().RenderPass->GetSpecification().TargetFramebuffer->GetImage();
		return m_CompositePass->GetOutput(0);
	}

	SceneRendererOptions& SceneRenderer::GetOptions()
	{
		return m_Options;
	}

	void SceneRenderer::CalculateCascades(CascadeData* cascades, const SceneRendererCamera& sceneCamera, const glm::vec3& lightDirection) const
	{
		//TODO: Reversed Z projection?

		float scaleToOrigin = m_ScaleShadowCascadesToOrigin;

		glm::mat4 viewMatrix = sceneCamera.ViewMatrix;
		constexpr glm::vec4 origin = glm::vec4(glm::vec3(0.0f), 1.0f);
		viewMatrix[3] = glm::lerp(viewMatrix[3], origin, scaleToOrigin);

		auto viewProjection = sceneCamera.Camera->GetUnReversedProjectionMatrix() * viewMatrix;

		const int SHADOW_MAP_CASCADE_COUNT = 4;
		float cascadeSplits[SHADOW_MAP_CASCADE_COUNT];

		float nearClip = sceneCamera.Near;
		float farClip = sceneCamera.Far;
		float clipRange = farClip - nearClip;

		float minZ = nearClip;
		float maxZ = nearClip + clipRange;

		float range = maxZ - minZ;
		float ratio = maxZ / minZ;

		// Calculate split depths based on view camera frustum
		// Based on method presented in https://developer.nvidia.com/gpugems/GPUGems3/gpugems3_ch10.html
		for (uint32_t i = 0; i < SHADOW_MAP_CASCADE_COUNT; i++)
		{
			float p = (i + 1) / static_cast<float>(SHADOW_MAP_CASCADE_COUNT);
			float log = minZ * std::pow(ratio, p);
			float uniform = minZ + range * p;
			float d = CascadeSplitLambda * (log - uniform) + uniform;
			cascadeSplits[i] = (d - nearClip) / clipRange;
		}

		cascadeSplits[3] = 0.3f;

		// Manually set cascades here
		// cascadeSplits[0] = 0.05f;
		// cascadeSplits[1] = 0.15f;
		// cascadeSplits[2] = 0.3f;
		// cascadeSplits[3] = 1.0f;

		// Calculate orthographic projection matrix for each cascade
		float lastSplitDist = 0.0;
		for (uint32_t i = 0; i < SHADOW_MAP_CASCADE_COUNT; i++)
		{
			float splitDist = cascadeSplits[i];

			glm::vec3 frustumCorners[8] =
			{
				glm::vec3(-1.0f,  1.0f, -1.0f),
				glm::vec3(1.0f,  1.0f, -1.0f),
				glm::vec3(1.0f, -1.0f, -1.0f),
				glm::vec3(-1.0f, -1.0f, -1.0f),
				glm::vec3(-1.0f,  1.0f,  1.0f),
				glm::vec3(1.0f,  1.0f,  1.0f),
				glm::vec3(1.0f, -1.0f,  1.0f),
				glm::vec3(-1.0f, -1.0f,  1.0f),
			};

			// Project frustum corners into world space
			glm::mat4 invCam = glm::inverse(viewProjection);
			for (uint32_t i = 0; i < 8; i++)
			{
				glm::vec4 invCorner = invCam * glm::vec4(frustumCorners[i], 1.0f);
				frustumCorners[i] = invCorner / invCorner.w;
			}

			for (uint32_t i = 0; i < 4; i++)
			{
				glm::vec3 dist = frustumCorners[i + 4] - frustumCorners[i];
				frustumCorners[i + 4] = frustumCorners[i] + (dist * splitDist);
				frustumCorners[i] = frustumCorners[i] + (dist * lastSplitDist);
			}

			// Get frustum center
			glm::vec3 frustumCenter = glm::vec3(0.0f);
			for (uint32_t i = 0; i < 8; i++)
				frustumCenter += frustumCorners[i];

			frustumCenter /= 8.0f;

			//frustumCenter *= 0.01f;

			float radius = 0.0f;
			for (uint32_t i = 0; i < 8; i++)
			{
				float distance = glm::length(frustumCorners[i] - frustumCenter);
				radius = glm::max(radius, distance);
			}
			radius = std::ceil(radius * 16.0f) / 16.0f;

			glm::vec3 maxExtents = glm::vec3(radius);
			glm::vec3 minExtents = -maxExtents;

			glm::vec3 lightDir = -lightDirection;
			glm::mat4 lightViewMatrix = glm::lookAt(frustumCenter - lightDir * -minExtents.z, frustumCenter, glm::vec3(0.0f, 0.0f, 1.0f));
			glm::mat4 lightOrthoMatrix = glm::ortho(minExtents.x, maxExtents.x, minExtents.y, maxExtents.y, 0.0f + CascadeNearPlaneOffset, maxExtents.z - minExtents.z + CascadeFarPlaneOffset);

			// Offset to texel space to avoid shimmering (from https://stackoverflow.com/questions/33499053/cascaded-shadow-map-shimmering)
			glm::mat4 shadowMatrix = lightOrthoMatrix * lightViewMatrix;
			float ShadowMapResolution = (float)m_DirectionalShadowMapPass[0]->GetTargetFramebuffer()->GetWidth();

			glm::vec4 shadowOrigin = (shadowMatrix * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f)) * ShadowMapResolution / 2.0f;
			glm::vec4 roundedOrigin = glm::round(shadowOrigin);
			glm::vec4 roundOffset = roundedOrigin - shadowOrigin;
			roundOffset = roundOffset * 2.0f / ShadowMapResolution;
			roundOffset.z = 0.0f;
			roundOffset.w = 0.0f;

			lightOrthoMatrix[3] += roundOffset;

			// Store split distance and matrix in cascade
			cascades[i].SplitDepth = (nearClip + splitDist * clipRange) * -1.0f;
			cascades[i].ViewProj = lightOrthoMatrix * lightViewMatrix;
			cascades[i].View = lightViewMatrix;

			lastSplitDist = cascadeSplits[i];
		}
	}

	void SceneRenderer::CalculateCascadesManualSplit(CascadeData* cascades, const SceneRendererCamera& sceneCamera, const glm::vec3& lightDirection) const
	{
		//TODO: Reversed Z projection?

		float scaleToOrigin = m_ScaleShadowCascadesToOrigin;

		glm::mat4 viewMatrix = sceneCamera.ViewMatrix;
		constexpr glm::vec4 origin = glm::vec4(glm::vec3(0.0f), 1.0f);
		viewMatrix[3] = glm::lerp(viewMatrix[3], origin, scaleToOrigin);

		auto viewProjection = sceneCamera.Camera->GetUnReversedProjectionMatrix() * viewMatrix;

		const int SHADOW_MAP_CASCADE_COUNT = 4;

		float nearClip = sceneCamera.Near;
		float farClip = sceneCamera.Far;
		float clipRange = farClip - nearClip;

		float minZ = nearClip;
		float maxZ = nearClip + clipRange;

		float range = maxZ - minZ;
		float ratio = maxZ / minZ;

		// Calculate orthographic projection matrix for each cascade
		float lastSplitDist = 0.0;
		for (uint32_t i = 0; i < SHADOW_MAP_CASCADE_COUNT; i++)
		{
			float splitDist = m_ShadowCascadeSplits[0];
			lastSplitDist = 0.0;

			glm::vec3 frustumCorners[8] =
			{
				glm::vec3(-1.0f,  1.0f, -1.0f),
				glm::vec3(1.0f,  1.0f, -1.0f),
				glm::vec3(1.0f, -1.0f, -1.0f),
				glm::vec3(-1.0f, -1.0f, -1.0f),
				glm::vec3(-1.0f,  1.0f,  1.0f),
				glm::vec3(1.0f,  1.0f,  1.0f),
				glm::vec3(1.0f, -1.0f,  1.0f),
				glm::vec3(-1.0f, -1.0f,  1.0f),
			};

			// Project frustum corners into world space
			glm::mat4 invCam = glm::inverse(viewProjection);
			for (uint32_t i = 0; i < 8; i++)
			{
				glm::vec4 invCorner = invCam * glm::vec4(frustumCorners[i], 1.0f);
				frustumCorners[i] = invCorner / invCorner.w;
			}

			for (uint32_t i = 0; i < 4; i++)
			{
				glm::vec3 dist = frustumCorners[i + 4] - frustumCorners[i];
				frustumCorners[i + 4] = frustumCorners[i] + (dist * splitDist);
				frustumCorners[i] = frustumCorners[i] + (dist * lastSplitDist);
			}

			// Get frustum center
			glm::vec3 frustumCenter = glm::vec3(0.0f);
			for (uint32_t i = 0; i < 8; i++)
				frustumCenter += frustumCorners[i];

			frustumCenter /= 8.0f;

			//frustumCenter *= 0.01f;

			float radius = 0.0f;
			for (uint32_t i = 0; i < 8; i++)
			{
				float distance = glm::length(frustumCorners[i] - frustumCenter);
				radius = glm::max(radius, distance);
			}
			radius = std::ceil(radius * 16.0f) / 16.0f;
			radius *= m_ShadowCascadeSplits[1];

			glm::vec3 maxExtents = glm::vec3(radius);
			glm::vec3 minExtents = -maxExtents;

			glm::vec3 lightDir = -lightDirection;
			glm::mat4 lightViewMatrix = glm::lookAt(frustumCenter - lightDir * -minExtents.z, frustumCenter, glm::vec3(0.0f, 0.0f, 1.0f));
			glm::mat4 lightOrthoMatrix = glm::ortho(minExtents.x, maxExtents.x, minExtents.y, maxExtents.y, 0.0f + CascadeNearPlaneOffset, maxExtents.z - minExtents.z + CascadeFarPlaneOffset);

			// Offset to texel space to avoid shimmering (from https://stackoverflow.com/questions/33499053/cascaded-shadow-map-shimmering)
			glm::mat4 shadowMatrix = lightOrthoMatrix * lightViewMatrix;
			float ShadowMapResolution = (float)m_DirectionalShadowMapPass[0]->GetTargetFramebuffer()->GetWidth();
			glm::vec4 shadowOrigin = (shadowMatrix * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f)) * ShadowMapResolution / 2.0f;
			glm::vec4 roundedOrigin = glm::round(shadowOrigin);
			glm::vec4 roundOffset = roundedOrigin - shadowOrigin;
			roundOffset = roundOffset * 2.0f / ShadowMapResolution;
			roundOffset.z = 0.0f;
			roundOffset.w = 0.0f;

			lightOrthoMatrix[3] += roundOffset;

			// Store split distance and matrix in cascade
			cascades[i].SplitDepth = (nearClip + splitDist * clipRange) * -1.0f;
			cascades[i].ViewProj = lightOrthoMatrix * lightViewMatrix;
			cascades[i].View = lightViewMatrix;

			lastSplitDist = m_ShadowCascadeSplits[0];
		}
	}

	void SceneRenderer::UpdateStatistics()
	{
		m_Statistics.DrawCalls = 0;
		m_Statistics.Instances = 0;
		m_Statistics.Meshes = 0;

		for (auto& [mk, dc] : m_SelectedStaticMeshDrawList)
		{
			m_Statistics.Instances += dc.InstanceCount;
			m_Statistics.DrawCalls++;
			m_Statistics.Meshes++;
		}

		for (auto& [mk, dc] : m_StaticMeshDrawList)
		{
			m_Statistics.Instances += dc.InstanceCount;
			m_Statistics.DrawCalls++;
			m_Statistics.Meshes++;
		}

		for (auto& [mk, dc] : m_SelectedMeshDrawList)
		{
			m_Statistics.Instances += dc.InstanceCount;
			m_Statistics.DrawCalls++;
			m_Statistics.Meshes++;
		}

		for (auto& [mk, dc] : m_DrawList)
		{
			m_Statistics.Instances += dc.InstanceCount;
			m_Statistics.DrawCalls++;
			m_Statistics.Meshes++;
		}

		m_Statistics.SavedDraws = m_Statistics.Instances - m_Statistics.DrawCalls;

		uint32_t frameIndex = Renderer::GetCurrentFrameIndex();
		m_Statistics.TotalGPUTime = m_MainCommandBuffer->GetExecutionGPUTime(frameIndex);
	}

	void SceneRenderer::SetLineWidth(const float width)
	{
		m_LineWidth = width;

		if (m_GeometryWireframePass)
			m_GeometryWireframePass->GetPipeline()->GetSpecification().LineWidth = width;
		if (m_GeometryWireframeOnTopPass)
			m_GeometryWireframeOnTopPass->GetPipeline()->GetSpecification().LineWidth = width;
		if (m_GeometryWireframeAnimPass)
			m_GeometryWireframeAnimPass->GetPipeline()->GetSpecification().LineWidth = width;
		if (m_GeometryWireframeOnTopAnimPass)
			m_GeometryWireframeOnTopAnimPass->GetPipeline()->GetSpecification().LineWidth = width;
	}

	SceneRenderer::RaytracingSettings::RaytracingSettings()
		: Mode(RaytracingMode::RestirComp), MaxFrames(500000), EnableRussianRoulette(true)
	{}

}
