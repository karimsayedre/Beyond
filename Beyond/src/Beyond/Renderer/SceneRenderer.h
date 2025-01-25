#pragma once

#include <EASTL/fixed_vector.h>

#include "AccelerationStructure.h"
#include "AccelerationStructureSet.h"
#include "Renderer2D.h"
#include "Beyond/Scene/Components.h"
#include "RenderPass.h"
#include "ComputePass.h"
#include "RaytracingPass.h"
#include "ShaderDefs.h"

#include "Beyond/Renderer/UniformBufferSet.h"
#include "Beyond/Renderer/RenderCommandBuffer.h"
#include "Beyond/Renderer/PipelineCompute.h"

#include "StorageBufferSet.h"

#include "Beyond/Project/TieringSettings.h"
#include "Beyond/Scene/Scene.h"
#include "Beyond/Core/Enums.h"

#include "DebugRenderer.h"
#include "DLSS.h"
#include "DrawCommands.h"
#include "GPUSemaphore.h"
#include "Raytracer.h"
#include "EASTL/array.h"
#include "rtxgi/ddgi/gfx/DDGIVolume_VK.h"

namespace Beyond {


#define BEY_FOREACH_SETTING(X)      \
	X(eMain, 0)					\
	X(eTLASBuild, 1)					\
	//X(eDrawRaytracing, 1)					\
	//X(ePostprocess, 2)			\

#define BEY_GENERATE_ENUM(x, y)   x,
#define BEY_GENERATE_STRING(x, y) #x,
#define BEY_GENERATE_VALUE(x, y)  y,
#define BEY_INIT_STRUCT(s)        s = { FOREACH_SETTING(GENERATE_VALUE) }

	enum CmdBuffers : int
	{
		BEY_FOREACH_SETTING(BEY_GENERATE_ENUM) Count
	};
	//BEY_ENABLE_ENUM_OPERATORS(CmdBuffers);
	const static char* s_CommandBufferNames[] = { BEY_FOREACH_SETTING(BEY_GENERATE_STRING) };
	constexpr bool s_CommandBufferIsComputeQueue[]{ false, true, false };


	struct SceneRendererOptions
	{
		bool ShowGrid = true;
		bool ShowSelectedInWireframe = false;

		enum class PhysicsColliderView
		{
			SelectedEntity = 0, All = 1
		};

		bool ShowPhysicsColliders = false;
		PhysicsColliderView PhysicsColliderMode = PhysicsColliderView::SelectedEntity;
		bool ShowPhysicsCollidersOnTop = false;
		glm::vec4 SimplePhysicsCollidersColor = glm::vec4{ 0.2f, 1.0f, 0.2f, 1.0f };
		glm::vec4 ComplexPhysicsCollidersColor = glm::vec4{ 0.5f, 0.5f, 1.0f, 1.0f };

		// General AO
		float AOShadowTolerance = 1.0f;

		// GTAO
		bool EnableGTAO = true;
		bool GTAOBentNormals = false;
		int GTAODenoisePasses = 4;

		// SSR
		bool EnableSSR = false;
		ShaderDef::AOMethod ReflectionOcclusionMethod = ShaderDef::AOMethod::None;
	};

	struct SSROptionsUB
	{
		//SSR
		glm::vec2 HZBUvFactor;
		glm::vec2 FadeIn = { 0.1f, 0.15f };
		float Brightness = 0.7f;
		float DepthTolerance = 0.8f;
		float FacingReflectionsFading = 0.1f;
		int MaxSteps = 70;
		uint32_t NumDepthMips;
		float RoughnessDepthTolerance = 1.0f;
		bool HalfRes = true;
		bool Padding0[3]{ 0, 0, 0 };
		bool EnableConeTracing = true;
		bool Padding1[3]{ 0, 0, 0 };
		float LuminanceFactor = 1.0f;
	};

	struct SceneRendererCamera
	{
		std::shared_ptr<Camera> Camera;
		glm::mat4 ViewMatrix;
		float Near, Far; //Non-reversed
		float FOV;
	};

	struct BloomSettings
	{
		bool Enabled = false;
		float Threshold = 1.0f;
		float Knee = 0.1f;
		float UpsampleScale = 1.0f;
		float Intensity = 1.0f;
		float DirtIntensity = 1.0f;
	};

	struct DOFSettings
	{
		bool Enabled = false;
		float FocusDistance = 0.0f;
		float BlurSize = 1.0f;
	};

	struct SceneRendererSpecification
	{
		Tiering::Renderer::RendererTieringSettings Tiering;
		uint32_t NumShadowCascades = 4;

		bool EnableEdgeOutlineEffect = false;
		bool JumpFloodPass = true;
	};

	struct TransformData
	{
		glm::vec4 CurrentMRow[3];
		glm::vec4 PreviousMRow[3];
	};

	struct TransformsStorageBuffer
	{
		TransformData* Data = nullptr;
	};

	class SceneRenderer final : public RefCounted
	{
	public:
		struct Statistics
		{
			uint32_t DrawCalls = 0;
			uint32_t Meshes = 0;
			uint32_t Instances = 0;
			uint32_t SavedDraws = 0;

			float TotalGPUTime = 0.0f;
		};
	public:
		SceneRenderer(Ref<Scene> scene, SceneRendererSpecification specification = SceneRendererSpecification());
		virtual ~SceneRenderer() override;

		void Init();

		void Shutdown();

		// Should only be called at initialization.
		void InitOptions();

		void SetScene(Ref<Scene> scene);

		void SetViewportSize(uint32_t width, uint32_t height);

		void UpdateGTAOData();
		void PrepareDDGIVolumes();

		void BeginScene(const SceneRendererCamera& camera, Timestep ts);
		void EndScene();

		static void InsertGPUPerfMarker(Ref<RenderCommandBuffer> renderCommandBuffer, const eastl::string& label, const glm::vec4& markerColor = {});
		static void BeginGPUPerfMarker(Ref<RenderCommandBuffer> renderCommandBuffer, const eastl::string& label, const glm::vec4& markerColor = {});
		static void EndGPUPerfMarker(Ref<RenderCommandBuffer> renderCommandBuffer);

		void SubmitMesh(Ref<Mesh> mesh, uint32_t submeshIndex, Ref<MaterialTable> materialTabl, const glm::mat4& transform = glm::mat4(1.0f), const std::vector<glm::mat4>& boneTransforms = {}, Ref<Material> overrideMaterial = nullptr);
		void SubmitStaticMesh(Ref<StaticMesh> staticMesh, Ref<MaterialTable> materialTable, const glm::mat4& transform = glm::mat4(1.0f), Ref<Material> overrideMaterial = nullptr);

		void SubmitSelectedMesh(Ref<Mesh> mesh, uint32_t submeshIndex, Ref<MaterialTable> materialTable, const glm::mat4& transform = glm::mat4(1.0f), const std::vector<glm::mat4>& boneTransforms = {}, Ref<Material> overrideMaterial = nullptr);
		void SubmitSelectedStaticMesh(Ref<StaticMesh> staticMesh, Ref<MaterialTable> materialTable, const glm::mat4& transform = glm::mat4(1.0f), Ref<Material> overrideMaterial = nullptr);

		void SubmitPhysicsDebugMesh(Ref<Mesh> mesh, uint32_t submeshIndex, const glm::mat4& transform = glm::mat4(1.0f));
		void SubmitPhysicsStaticDebugMesh(Ref<StaticMesh> mesh, const glm::mat4& transform = glm::mat4(1.0f), const bool isPrimitiveCollider = true);

		Ref<RasterPipeline> GetFinalPipeline();
		Ref<RenderPass> GetFinalRenderPass();
		// TODO Ref<RenderPass> GetCompositeRenderPass() { return m_CompositePipeline->GetSpecification().RenderPass; }
		Ref<RenderPass> GetCompositeRenderPass() { return nullptr; }
		Ref<Framebuffer> GetExternalCompositeFramebuffer() { return m_CompositePass->GetTargetFramebuffer(); }
		Ref<Image2D> GetFinalPassImage();

		Ref<Renderer2D> GetRenderer2D() { return m_Renderer2D; }
		Ref<Renderer2D> GetScreenSpaceRenderer2D() { return m_Renderer2DScreenSpace; }
		Ref<DebugRenderer> GetDebugRenderer() { return m_DebugRenderer; }

		SceneRendererOptions& GetOptions();
		const SceneRendererSpecification& GetSpecification() const { return m_Specification; }

		void SetShadowSettings(float nearPlane, float farPlane, float lambda, float scaleShadowToOrigin = 0.0f)
		{
			CascadeNearPlaneOffset = nearPlane;
			CascadeFarPlaneOffset = farPlane;
			CascadeSplitLambda = lambda;
			m_ScaleShadowCascadesToOrigin = scaleShadowToOrigin;
		}

		void SetShadowCascades(float a, float b, float c, float d)
		{
			m_UseManualCascadeSplits = true;
			m_ShadowCascadeSplits[0] = a;
			m_ShadowCascadeSplits[1] = b;
			m_ShadowCascadeSplits[2] = c;
			m_ShadowCascadeSplits[3] = d;
		}

		BloomSettings& GetBloomSettings() { return m_BloomSettings; }
		DOFSettings& GetDOFSettings() { return m_DOFSettings; }

		void SetLineWidth(float width);

		static void WaitForThreads();

		uint32_t GetViewportWidth() const { return m_RenderWidth; }
		uint32_t GetViewportHeight() const { return m_RenderHeight; }

		float GetOpacity() const { return m_CompositeSettings.Opacity; }
		void SetOpacity(float opacity) { m_CompositeSettings.Opacity = opacity; }

		const glm::mat4& GetScreenSpaceProjectionMatrix() const { return m_ScreenSpaceProjectionMatrix; }

		const Statistics& GetStatistics() const { return m_Statistics; }
	private:
		void DDGIIrradiance();
		void FlushDrawList();

		void PreRender();
		void BuildAccelerationStructures();



		void CopyToBoneTransformStorage(const MeshKey& meshKey, const Ref<MeshSource>& meshSource, const std::vector<glm::mat4>& boneTransforms);

		void CreateBloomPassMaterials();
		void CreatePreConvolutionPassMaterials();
		void CreateHZBPassMaterials();
		void CreatePreIntegrationPassMaterials();

		// Passes
		void ClearPass();
		void ClearPass(Ref<RenderPass> renderPass, bool explicitClear = false);
		void GTAOCompute();

		void AOComposite();

		void GTAODenoiseCompute();

		void ShadowMapPass();
		void SpotShadowMapPass();
		void PreDepthPass();
		void HZBCompute();
		void MotionVectorsCompute();
		void PreIntegration();
		void LightCullingPass();
		void SkyboxPass();
		void GeometryPass();
		void PathTracingPass();
		void DLSSPass();
		void DDGIRaytracing();
		void DDGIVis();
		void PreConvolutionCompute();
		void JumpFloodPass();

		// Post-processing
		void BloomCompute();
		void SSRCompute();
		void SSRCompositePass();
		void EdgeDetectionPass();
		void CompositePass();

		struct CascadeData
		{
			glm::mat4 ViewProj;
			glm::mat4 View;
			float SplitDepth;
		};
		void CalculateCascades(CascadeData* cascades, const SceneRendererCamera& sceneCamera, const glm::vec3& lightDirection) const;
		void CalculateCascadesManualSplit(CascadeData* cascades, const SceneRendererCamera& sceneCamera, const glm::vec3& lightDirection) const;

		void UpdateStatistics();
	private:
		Ref<Scene> m_Scene;
		SceneRendererSpecification m_Specification;
		std::vector<Ref<RenderCommandBuffer>> m_CommandBuffers;
		Ref<RenderCommandBuffer> m_MainCommandBuffer;

		Ref<Renderer2D> m_Renderer2D, m_Renderer2DScreenSpace;
		Ref<DebugRenderer> m_DebugRenderer;

		glm::mat4 m_ScreenSpaceProjectionMatrix{ 1.0f };

		struct SceneInfo
		{
			SceneRendererCamera SceneCamera;

			// Resources
			Ref<Environment> SceneEnvironment;
			float SkyboxLod = 0.0f;
			float SceneEnvironmentIntensity;
			LightEnvironment SceneLightEnvironment;
		} m_SceneData;

		glm::mat4 m_PreviousInvViewProjection;

		struct UBCamera
		{
			// projection with near and far inverted
			glm::mat4 ReprojectionMatrix;
			glm::mat4 PrevViewProjection;
			glm::mat4 ViewProjection;
			glm::mat4 InverseViewProjection;
			glm::mat4 Projection;
			glm::mat4 InverseProjection;
			glm::mat4 View;
			glm::mat4 InverseView;
			glm::mat4 PrevView;
			glm::vec2 NDCToViewMul;
			glm::vec2 NDCToViewAdd;
			glm::vec2 DepthUnpackConsts;
			glm::vec2 CameraTanHalfFOV;
			glm::vec2 CurrentJitter;
			glm::vec2 PreviousJitter;
			glm::vec2 ClipToRenderTargetScale;
		} CameraDataUB;

		struct CBGTAOData
		{
			glm::vec2 NDCToViewMul_x_PixelSize;
			float EffectRadius = 0.5f;
			float EffectFalloffRange = 0.62f;

			float RadiusMultiplier = 1.46f;
			float FinalValuePower = 2.2f;
			float DenoiseBlurBeta = 1.2f;
			bool HalfRes = false;
			char Padding0[3]{ 0, 0, 0 };

			float SampleDistributionPower = 2.0f;
			float ThinOccluderCompensation = 0.0f;
			float DepthMIPSamplingOffset = 3.3f;
			int NoiseIndex = 0;

			glm::vec2 HZBUVFactor;
			float ShadowTolerance;
			float Padding;
		} GTAODataCB;

		struct UBScreenData
		{
			glm::vec2 InvFullResolution;
			glm::vec2 FullResolution;
			glm::vec2 InvHalfResolution;
			glm::vec2 HalfResolution;
			glm::vec2 HZBUVFactor;
		} m_ScreenDataUB;

		struct UBShadow
		{
			glm::mat4 ViewProjection[4];
		} ShadowData;

		struct UBPointLights
		{
			uint32_t Count{ 0 };
			glm::vec3 Padding{};
			PointLight PointLights[1024]{};
		} PointLightsUB;

		struct UBSpotLights
		{
			uint32_t Count{ 0 };
			glm::vec3 Padding{};
			SpotLight SpotLights[800]{};
		} SpotLightUB;

		struct UBSpotShadowData
		{
			glm::mat4 ShadowMatrices[800]{};
		} SpotShadowDataUB;

		struct UBScene
		{
			DirectionalLight Lights;
			glm::vec3 CameraPosition;
			uint32_t FrameIndex;
			glm::vec3 PrevCameraPosition;
			float MipBias;
			float EnvironmentMapLod;
			float EnvironmentMapIntensity = 1.0f;
			glm::vec2 Padding2{}; // Added padding to align to 16 
		} SceneDataUB;

		struct UBRendererData
		{
			glm::vec4 CascadeSplits;
			uint32_t TilesCountX{ 0 };
			bool ShowCascades = false;
			char Padding0[3] = { 0,0,0 }; // Bools are 4-bytes in GLSL
			bool SoftShadows = true;
			char Padding1[3] = { 0,0,0 };
			float Range = 0.5f;
			float MaxShadowDistance = 200.0f;
			float ShadowFade = 1.0f;
			bool CascadeFading = true;
			char Padding2[3] = { 0,0,0 };
			float CascadeTransitionFade = 1.0f;
			bool ShowLightComplexity = false;
			char Padding3[3] = { 0,0,0 };
		} RendererDataUB;


		// Ray tracing
		Ref<Image2D> m_RaytracingImage;
		Ref<Image2D> m_AccumulationImage;
		Ref<Image2D> m_AlbedoImage;
		Ref<Image2D> m_RaytracingNormalsImage;
		Ref<Image2D> m_RaytracingMetalnessRoughnessImage;
		Ref<Image2D> m_RaytracingPrimaryHitT;
		Ref<Material> m_RaytracingMaterial;
		Ref<Material> m_PathtracingMaterial;
		Ref<RaytracingPass> m_RayTracingRenderPass;
		Ref<RaytracingPass> m_PathTracingRenderPass;
		Ref<RaytracingPass> m_RestirRenderPass;
		bool m_RaytracerReset = false;
		Ref<Image2D> m_PreviousPositionImage;
		Ref<Image2D> m_DLSSImage;
		Ref<DLSS> m_DLSS;
		Timestep m_TimeStep;
		bool m_DLSSSupported;
		Ref<Material> m_RestirMaterial;
		Ref<Material> m_RestirCompMaterial;
		Ref<ComputePass> m_RestirCompRenderPass;

		enum class RaytracingMode : uint8_t
		{
			None, Raytracing, Pathtracing, Restir, RestirComp
		};

		struct RaytracingSettings
		{
			RaytracingMode Mode;
			uint32_t MaxFrames;
			bool EnableRussianRoulette = true;
			int WorkGroupSize = 8;

			RaytracingSettings();
		} m_RaytracingSettings;
		uint32_t m_AccumulatedPathtracingFrames = 0;
		uint32_t m_AccumulatedFrames = 0;
		glm::vec2 m_CurrentJitter;

		DLSSSettings m_DLSSSettings;

		struct DDGISettings
		{
			bool Enable = true;
			bool TextureVis = false;
			bool ProbeVis = false;
		} m_DDGISettings;

		struct
		{
			uint32_t InstanceOffset = 0;
			uint32_t ProbeType = 0;
			float ProbeRadius = 1.0f;
			float DistanceDivisor = 3.0f;
			float RayDataTextureScale = 1.0f;
			float IrradianceTextureScale = 2.0f;
			float DistanceTextureScale = 1.0f;
			float ProbeDataTextureScale = 1.0f;
			float ProbeVariabilityTextureScale = 0.0f;
		} m_DDGITextureVisSettings;

		std::vector<rtxgi::vulkan::DDGIVolume> m_DDGIVolumes;
		Ref<RaytracingPass> m_DDGIRayTracingRenderPass;
		Ref<RaytracingPass> m_DDGIVisRenderPass;
		Ref<StorageBuffer> m_SBDDGIConstants; // Not a set, one buffer fits all frames
		Ref<StorageBuffer> m_SBDDGIReourceIndices;
		Ref<Image2D> m_DebugImage;
		Ref<StaticMesh> m_SphereMesh;
		Ref<PipelineCompute> m_VisProbeUpdatePipeline;
		Ref<ComputePass> m_DDGIProbeUpdatePass;
		Ref<ComputePass> m_DDGIIrradiancePass;
		Ref<ComputePass> m_DDGITexVisPass;
		Ref<StorageBufferSet> m_SBSDDGIProbeInstances;
		Ref<PipelineCompute> m_DDGIIrradiancePipeline;
		Ref<PipelineCompute> m_DDGITexVisPipeline;
		Ref<Image2D> m_DDGIOutputImage;

		Ref<Image2D> m_ExposureImage;
		Ref<ComputePass> m_ExposurePass;


		Ref<Raytracer> m_MainRaytracer;
		Ref<Raytracer> m_DDGIVisRaytracer;
		void SubmitToRaytracer(const DrawCommand& dc, const MaterialAsset* material, const glm::mat3x4& transform);
		void SubmitToRaytracer(const StaticDrawCommand& dc, const MaterialAsset* material, const glm::mat3x4& transform);


		// GTAO
		Ref<ComputePass> m_GTAOComputePass;
		Ref<ComputePass> m_GTAODenoisePass[2];
		struct GTAODenoiseConstants
		{
			float DenoiseBlurBeta;
			bool HalfRes;
			char Padding1[3]{ 0, 0, 0 };
		} m_GTAODenoiseConstants;
		Ref<Image2D> m_GTAOOutputImage;
		Ref<Image2D> m_GTAODenoiseImage;
		// Points to m_GTAOOutputImage or m_GTAODenoiseImage!
		Ref<Image2D> m_GTAOFinalImage; //TODO: WeakRef!
		Ref<Image2D> m_GTAOEdgesOutputImage;
		glm::uvec3 m_GTAOWorkGroups{ 1 };
		Ref<Material> m_GTAODenoiseMaterial[2]; //Ping, Pong
		Ref<Material> m_AOCompositeMaterial;
		glm::uvec3 m_GTAODenoiseWorkGroups{ 1 };

		Ref<Shader> m_CompositeShader;

		// Shadows
		Ref<RasterPipeline> m_SpotShadowPassPipeline;
		Ref<RasterPipeline> m_SpotShadowPassAnimPipeline;
		Ref<Material> m_SpotShadowPassMaterial;

		glm::uvec3 m_LightCullingWorkGroups;

		Ref<UniformBufferSet> m_UBSCamera;
		Ref<UniformBufferSet> m_UBSShadow;
		Ref<UniformBufferSet> m_UBSScene;
		Ref<UniformBufferSet> m_UBSRendererData;
		Ref<UniformBufferSet> m_UBSPointLights;
		Ref<UniformBufferSet> m_UBSScreenData;
		Ref<UniformBufferSet> m_UBSSpotLights;
		Ref<UniformBufferSet> m_UBSSpotShadowData;

		Ref<StorageBufferSet> m_SBSVisiblePointLightIndicesBuffer;
		Ref<StorageBufferSet> m_SBSVisibleSpotLightIndicesBuffer;
		Ref<StorageBufferSet> m_SBSObjectSpecs;
		Ref<StorageBufferSet> m_SBSMaterialBuffer;

		Ref<AccelerationStructureSet> m_SceneTLAS;
		Ref<AccelerationStructureSet> m_DDGIVisTLAS;

		std::vector<Ref<RenderPass>> m_DirectionalShadowMapPass; // Per-cascade
		std::vector<Ref<RenderPass>> m_DirectionalShadowMapAnimPass; // Per-cascade
		Ref<RenderPass> m_GeometryPass;
		Ref<RenderPass> m_GeometryAnimPass;
		Ref<RenderPass> m_PreDepthPass, m_PreDepthAnimPass, m_PreDepthTransparentPass;
		Ref<RenderPass> m_SpotShadowPass;
		Ref<RenderPass> m_DeinterleavingPass[2];
		Ref<RenderPass> m_AOCompositePass;

		Ref<ComputePass> m_LightCullingPass;

		float LightDistance = 0.1f;
		float CascadeSplitLambda = 0.92f;
		glm::vec4 CascadeSplits;
		float CascadeFarPlaneOffset = 50.0f, CascadeNearPlaneOffset = -50.0f;
		float m_ScaleShadowCascadesToOrigin = 0.0f;
		float m_ShadowCascadeSplits[4];
		bool m_UseManualCascadeSplits = false;

		Ref<ComputePass> m_HierarchicalDepthPass;

		// SSR
		Ref<RenderPass> m_SSRCompositePass;
		Ref<ComputePass> m_SSRPass;
		Ref<ComputePass> m_PreConvolutionComputePass;
		Ref<ComputePass> m_SSRUpscalePass;
		Ref<Image2D> m_SSRImage;

		// Pre-Integration
		Ref<ComputePass> m_PreIntegrationPass;
		struct PreIntegrationVisibilityTexture
		{
			Ref<Texture2D> Texture;
			std::vector<Ref<ImageView>> ImageViews; // per-mip
		} m_PreIntegrationVisibilityTexture;
		std::vector<Ref<Material>> m_PreIntegrationMaterials; // per-mip

		// Hierarchical Depth
		struct HierarchicalDepthTexture
		{
			Ref<Texture2D> Texture;
			std::vector<Ref<ImageView>> ImageViews; // per-mip
		} m_HierarchicalDepthTexture;
		std::vector<Ref<Material>> m_HZBMaterials; // per-mip

		struct PreConvolutionComputeTexture
		{
			Ref<Texture2D> Texture;
			std::vector<Ref<ImageView>> ImageViews; // per-mip
		} m_PreConvolutedTexture;
		std::vector<Ref<Material>> m_PreConvolutionMaterials; // per-mip
		Ref<Material> m_SSRCompositeMaterial;
		glm::uvec3 m_SSRWorkGroups{ 1 };

		glm::vec2 FocusPoint = { 0.5f, 0.5f };

		Ref<Material> m_CompositeMaterial;

		Ref<RasterPipeline> m_GeometryPipeline;
		Ref<RasterPipeline> m_TransparentGeometryPipeline;
		Ref<RasterPipeline> m_GeometryPipelineAnim;

		Ref<RenderPass> m_SelectedGeometryPass;
		Ref<RenderPass> m_SelectedGeometryAnimPass;
		Ref<Material> m_SelectedGeometryMaterial;
		Ref<Material> m_SelectedGeometryMaterialAnim;

		Ref<RenderPass> m_GeometryWireframePass;
		Ref<RenderPass> m_GeometryWireframeAnimPass;
		Ref<RenderPass> m_GeometryWireframeOnTopPass;
		Ref<RenderPass> m_GeometryWireframeOnTopAnimPass;
		Ref<Material> m_WireframeMaterial;

		Ref<RasterPipeline> m_PreDepthPipeline;
		Ref<RasterPipeline> m_PreDepthTransparentPipeline;
		Ref<RasterPipeline> m_PreDepthPipelineAnim;
		Ref<Material> m_PreDepthMaterial;

		Ref<RenderPass> m_CompositePass;

		Ref<RasterPipeline> m_ShadowPassPipelines[4];
		Ref<RasterPipeline> m_ShadowPassPipelinesAnim[4];

		Ref<RenderPass> m_EdgeDetectionPass;

		Ref<Material> m_ShadowPassMaterial;

		Ref<RasterPipeline> m_SkyboxPipeline;
		Ref<Material> m_SkyboxMaterial;
		Ref<RenderPass> m_SkyboxPass;

		Ref<RenderPass> m_DOFPass;
		Ref<Material> m_DOFMaterial;

		Ref<PipelineCompute> m_LightCullingPipeline;

		// Jump Flood Pass
		Ref<RenderPass> m_JumpFloodInitPass;
		Ref<RenderPass> m_JumpFloodPass[2];
		Ref<RenderPass> m_JumpFloodCompositePass;
		Ref<Material> m_JumpFloodInitMaterial, m_JumpFloodPassMaterial[2];
		Ref<Material> m_JumpFloodCompositeMaterial;

		// Bloom compute
		Ref<ComputePass> m_BloomComputePass;
		uint32_t m_BloomComputeWorkgroupSize = 8;
		Ref<PipelineCompute> m_BloomComputePipeline;

		struct BloomComputeTextures
		{
			Ref<Texture2D> Texture;
			std::vector<Ref<ImageView>> ImageViews; // per-mip
		};
		std::vector<BloomComputeTextures> m_BloomComputeTextures{ 3 };

		struct BloomComputeMaterials
		{
			Ref<Material> PrefilterMaterial;
			std::vector<Ref<Material>> DownsampleAMaterials;
			std::vector<Ref<Material>> DownsampleBMaterials;
			Ref<Material> FirstUpsampleMaterial;
			std::vector<Ref<Material>> UpsampleMaterials;
		} m_BloomComputeMaterials;

	public:


	private:
		// per-frame
		std::vector<TransformBuffer> m_SubmeshTransformBuffers;
		std::vector<TransformsStorageBuffer> m_TransformBuffers;
		Ref<StorageBufferSet> m_SBSTransforms;
		//TransformVertexData* m_TransformsData = nullptr;

		using BoneTransforms = std::array<glm::mat4, 100>; // Note: 100 == MAX_BONES from the shaders
		Ref<StorageBufferSet> m_SBSBoneTransforms;
		BoneTransforms* m_BoneTransformsData = nullptr;

		std::vector<Ref<Framebuffer>> m_TempFramebuffers;

		struct TransformMapData
		{
			std::vector<TransformVertexData> Transforms;
			uint32_t TransformIndex = 0;
		};

		struct BoneTransformsMapData
		{
			std::vector<BoneTransforms> BoneTransformsData;
			uint32_t BoneTransformsBaseIndex = 0;
		};

		std::unordered_map<MeshKey, TransformMapData> m_MeshTransformMap;
		std::unordered_map<MeshKey, TransformMapData> m_TransformHistoryMap;

		eastl::hash_map<MeshKey, BoneTransformsMapData> m_MeshBoneTransformsMap;


		eastl::hash_map<MeshKey, DrawCommand> m_DrawList;
		eastl::hash_map<MeshKey, DrawCommand> m_TransparentDrawList;
		eastl::hash_map<MeshKey, DrawCommand> m_SelectedMeshDrawList;
		eastl::hash_map<MeshKey, DrawCommand> m_ShadowPassDrawList;

		eastl::hash_map<MeshKey, StaticDrawCommand> m_StaticMeshDrawList;
		eastl::hash_map<MeshKey, StaticDrawCommand> m_TransparentStaticMeshDrawList;
		eastl::hash_map<MeshKey, StaticDrawCommand> m_SelectedStaticMeshDrawList;
		eastl::hash_map<MeshKey, StaticDrawCommand> m_StaticMeshShadowPassDrawList;

		// Debug
		eastl::hash_map<MeshKey, StaticDrawCommand> m_StaticColliderDrawList;
		eastl::hash_map<MeshKey, DrawCommand> m_ColliderDrawList;

		// Grid
		Ref<RenderPass> m_GridRenderPass;
		Ref<Material> m_GridMaterial;

		Ref<Material> m_OutlineMaterial;
		Ref<Material> m_SimpleColliderMaterial;
		Ref<Material> m_ComplexColliderMaterial;

		//Ref<Framebuffer> m_CompositingFramebuffer;

		SceneRendererOptions m_Options;
		SSROptionsUB m_SSROptions;

		uint32_t m_TargetWidth = 0, m_TargetHeight = 0;
		uint32_t m_RenderWidth = 0, m_RenderHeight = 0;
		float m_InvTargetWidth = 0.f, m_InvTargetHeight = 0.f;
		float m_InvRenderWidth = 0.f, m_InvRenderHeight = 0.f;
		bool m_NeedsResize = false;
		bool m_Active = false;
		bool m_ResourcesCreatedGPU = false;
		bool m_ResourcesCreated = false;

		float m_LineWidth = 2.0f;

		BloomSettings m_BloomSettings;
		DOFSettings m_DOFSettings;
		Ref<Texture2D> m_BloomDirtTexture;

		Ref<Image2D> m_ReadBackImage;
		glm::vec4* m_ReadBackBuffer = nullptr;

		struct CompositeSettings
		{
			float Opacity = 1.0f;
			float GrainStrength = 0.0f;
			uint32_t Tonemapper = 1;
		} m_CompositeSettings;

		enum class GPUSemaphoreUsage
		{
			TLASBuild
		};

		std::unordered_map<GPUSemaphoreUsage, Ref<GPUSemaphore>> m_GPUSemaphores;

		struct GPUTimeQueries
		{
			//uint32_t BuildAccelerationStructuresQuery = 0;
			uint32_t DirShadowMapPassQuery = 0;
			uint32_t SpotShadowMapPassQuery = 0;
			uint32_t DepthPrePassQuery = 0;
			uint32_t HierarchicalDepthQuery = 0;
			uint32_t MotionVectorsQuery = 0;
			uint32_t PreIntegrationQuery = 0;
			uint32_t LightCullingPassQuery = 0;
			uint32_t RaytracingQuery = 0;
			uint32_t GeometryPassQuery = 0;
			uint32_t DDGIRaytraceQuery = 0;
			uint32_t DDGIIrradianceQuery = 0;
			uint32_t PreConvolutionQuery = 0;
			uint32_t GTAOPassQuery = 0;
			uint32_t GTAODenoisePassQuery = 0;
			uint32_t AOCompositePassQuery = 0;
			uint32_t DLSSPassQuery = 0;
			uint32_t SSRQuery = 0;
			uint32_t SSRCompositeQuery = 0;
			uint32_t BloomComputePassQuery = 0;
			uint32_t JumpFloodPassQuery = 0;
			uint32_t CompositePassQuery = 0;
		} m_GPUTimeQueries;

		Statistics m_Statistics;

		friend class SceneRendererPanel;
		friend class VulkanDLSS;
	};

}
