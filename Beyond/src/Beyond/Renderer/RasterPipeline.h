#pragma once

#include "Beyond/Core/Ref.h"

#include "Beyond/Renderer/VertexBuffer.h"
#include "Beyond/Renderer/Shader.h"
#include "Beyond/Renderer/UniformBuffer.h"
#include "Beyond/Renderer/Framebuffer.h"

namespace Beyond {

	enum class PrimitiveTopology
	{
		None = 0,
		Points,
		Lines,
		Triangles,
		LineStrip,
		TriangleStrip,
		TriangleFan
	};

	struct PipelineSpecification
	{
		Ref<Shader> Shader;
		Ref<Framebuffer> TargetFramebuffer;
		VertexBufferLayout Layout;
		VertexBufferLayout InstanceLayout;
		VertexBufferLayout BoneInfluenceLayout;
		PrimitiveTopology Topology = PrimitiveTopology::Triangles;
		DepthCompareOperator DepthOperator = DepthCompareOperator::GreaterOrEqual;
		bool BackfaceCulling = true;
		bool DepthTest = true;
		bool DepthWrite = true;
		bool Wireframe = false;
		float LineWidth = 1.0f;

		eastl::string DebugName;
	};

	struct PipelineStatistics
	{
		uint64_t InputAssemblyVertices = 0;
		uint64_t InputAssemblyPrimitives = 0;
		uint64_t VertexShaderInvocations = 0;
		uint64_t ClippingInvocations = 0;
		uint64_t ClippingPrimitives = 0;
		uint64_t FragmentShaderInvocations = 0;
		uint64_t ComputeShaderInvocations = 0;
		
		// TODO: tesselation shader stats when we have them
	};

	// Identical to Vulkan's VkPipelineStageFlagBits
	// Note: this is a bitfield
	enum class PipelineStage
	{
		None = 0,
		TopOfPipe = 0x00000001,
		DrawIndirect = 0x00000002,
		VertexInput = 0x00000004,
		VertexShader = 0x00000008,
		TesselationControlShader = 0x00000010,
		TesselationEvaluationShader = 0x00000020,
		GeometryShader = 0x00000040,
		FragmentShader = 0x00000080,
		RaytracingShader = 0x00200000,
		EarlyFragmentTests = 0x00000100,
		LateFragmentTests = 0x00000200,
		ColorAttachmentOutput = 0x00000400,
		ComputeShader = 0x00000800,
		Transfer = 0x00001000,
		BottomOfPipe = 0x00002000,
		Host = 0x00004000,
		AllGraphics = 0x00008000,
		AllCommands = 0x00010000,
	};

	// Identical to Vulkan's VkAccessFlagBits
	// Note: this is a bitfield
	enum class ResourceAccessFlags
	{
		None = 0,
		IndirectCommandRead = 0x00000001,
		IndexRead = 0x00000002,
		VertexAttributeRead = 0x00000004,
		UniformRead = 0x00000008,
		InputAttachmentRead = 0x00000010,
		ShaderRead = 0x00000020,
		ShaderWrite = 0x00000040,
		ColorAttachmentRead = 0x00000080,
		ColorAttachmentWrite = 0x00000100,
		DepthStencilAttachmentRead = 0x00000200,
		DepthStencilAttachmentWrite = 0x00000400,
		TransferRead = 0x00000800,
		TransferWrite = 0x00001000,
		HostRead = 0x00002000,
		HostWrite = 0x00004000,
		MemoryRead = 0x00008000,
		MemoryWrite = 0x00010000,
	};

	// Identical to Vulkan's VkAccessFlagBits
	// Note: this is a bitfield
	enum class ImageLayout
	{
		Undefined = 0,
		General = 1,
		ColorAttachmentOptimal = 2,
		DepthStencilAttachmentOptimal = 3,
		DepthStencilReadOnlyOptimal = 4,
		ShaderReadOnlyOptimal = 5,
		TransferSrcOptimal = 6,
		TransferDstOptimal = 7,
		Preinitialized = 8,
		DepthReadOnlyStencilAttachmentOptimal = 1000117000,
		DepthAttachmentStencilReadOnlyOptimal = 1000117001,
		DepthAttachmentOptimal = 1000241000,
		DepthReadOnlyOptimal = 1000241001,
		StencilAttachmentOptimal = 1000241002,
		StencilReadOnlyOptimal = 1000241003,
		ReadOnlyOptimal = 1000314000,
		AttachmentOptimal = 1000314001,
		PresentSrcKHR = 1000001002,
		VideoDecodeDstKHR = 1000024000,
		VideoDecodeSrcKHR = 1000024001,
		VideoDecodeDPBKHR = 1000024002,
		SharedPresentKHR = 1000111000,
		FragmentDensityMapOptimalEXT = 1000218000,
		FragmentShadingRateAttachmentOptimalKHR = 1000164003,
#ifdef VK_ENABLE_BETA_EXTENSIONS
		VideoEncodeDstKHR = 1000299000,
#endif
#ifdef VK_ENABLE_BETA_EXTENSIONS
		VideoEncodeSrcKHR = 1000299001,
#endif
#ifdef VK_ENABLE_BETA_EXTENSIONS
		VideoEncodeDPBKHR = 1000299002,
#endif
		AttachmentFeedbackLoopOptimalEXT = 1000339000,
		DepthReadOnlyStencilAttachmentOptimalKHR = DepthReadOnlyStencilAttachmentOptimal,
		DepthAttachmentStencilReadOnlyOptimalKHR = DepthAttachmentStencilReadOnlyOptimal,
		ShadingRateOptimalNV = FragmentShadingRateAttachmentOptimalKHR,
		DepthAttachmentOptimalKHR = DepthAttachmentOptimal,
		DepthReadOnlyOptimalKHR = DepthReadOnlyOptimal,
		StencilAttachmentOptimalKHR = StencilAttachmentOptimal,
		StencilReadOnlyOptimalKHR = StencilReadOnlyOptimal,
		ReadOnlyOptimalKHR = ReadOnlyOptimal,
		AttachmentOptimalKHR = AttachmentOptimal,
		MaxEnum = 0x7FFFFFFF

	};

	class RasterPipeline : public RefCounted
	{
	public:
		virtual ~RasterPipeline() = default;

		virtual PipelineSpecification& GetSpecification() = 0;
		virtual const PipelineSpecification& GetSpecification() const = 0;

		virtual void Invalidate() = 0;

		virtual Ref<Shader> GetShader() const = 0;

		static Ref<RasterPipeline> Create(const PipelineSpecification& spec);
	};

}
