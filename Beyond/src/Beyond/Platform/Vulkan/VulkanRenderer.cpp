#include "pch.h"
#include "VulkanRenderer.h"

#include "imgui.h"

#include "Vulkan.h"
#include "VulkanContext.h"
#include "Beyond/Core/Application.h"

#include "Beyond/Renderer/Renderer.h"

#include "Beyond/Asset/AssetManager.h"

#include "VulkanRasterPipeline.h"
#include "VulkanVertexBuffer.h"
#include "VulkanIndexBuffer.h"
#include "VulkanFramebuffer.h"
#include "VulkanRenderCommandBuffer.h"
#include "VulkanRenderPass.h"
#include "VulkanComputePass.h"

#include "VulkanShader.h"
#include "VulkanTexture.h"
#include "VulkanAPI.h"

#include "backends/imgui_impl_glfw.h"
#include "examples/imgui_impl_vulkan_with_textures.h"

#include "Beyond/Core/Timer.h"
#include "Beyond/Debug/Profiler.h"

#if BEY_HAS_SHADER_COMPILER
#include "ShaderCompiler/VulkanShaderCompiler.h"
#endif

#include <memory_resource>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "BindlessDescriptorSetManager.h"
#include "VulkanRasterPipeline.h"
#include "VulkanRaytracingPipeline.h"

#include "VulkanComputePipeline.h"
#include "VulkanDiagnostics.h"
#include "VulkanMaterial.h"
#include "VulkanRaytracingPass.h"
#include "VulkanSampler.h"
#include "VulkanStorageBuffer.h"
#include "Beyond/Core/EASTLFormat.h"
#include "Beyond/Renderer/SceneRenderer.h"
#include "rtxgi/ddgi/gfx/DDGIVolume_VK.h"
#include "rtxgi/ddgi/DDGIVolume.h"

namespace Beyond {
	struct VulkanRendererData
	{
		BindlessDescriptorSetManager BindlessDescriptorSetManager;
		std::vector<std::byte> m_BufferResource{ 1024 * 1024 * 2 };
		std::pmr::monotonic_buffer_resource m_Res{ m_BufferResource.data(), m_BufferResource.size() };

		std::pmr::vector<RenderPassInput> BindlessDescriptorUpdateQueue{ &m_Res };

		RendererCapabilities RenderCaps;

		Ref<Texture2D> BRDFLut;

		Ref<VertexBuffer> QuadVertexBuffer;
		Ref<IndexBuffer> QuadIndexBuffer;

		std::vector<VkDescriptorPool> DescriptorPools;
		VkDescriptorPool MaterialDescriptorPool;
		VkDescriptorPool DDGIDescriptorPool;

		std::vector<uint32_t> DescriptorPoolAllocationCount;

		// Default samplers
		Ref<Sampler> BilinearSampler = nullptr;
		Ref<Sampler> PointSampler = nullptr;
		Ref<Sampler> AnisoSampler = nullptr;

		int32_t SelectedDrawCall = -1;
		int32_t DrawCallCount = 0;
		int32_t RaytraceCount = 0;

		rtxgi::vulkan::DDGIVolumeResources DDGIResources;
		std::vector<std::unique_ptr<rtxgi::vulkan::DDGIVolume>> Volumes;
	};

	static VulkanRendererData* s_VulkanRendererData = nullptr;

	namespace Utils {

		static const char* VulkanVendorIDToString(uint32_t vendorID)
		{
			switch (vendorID)
			{
				case 0x10DE: return "NVIDIA";
				case 0x1002: return "AMD";
				case 0x8086: return "INTEL";
				case 0x13B5: return "ARM";
			}
			return "Unknown";
		}

	}

	void VulkanRenderer::Init()
	{
		auto& caps = s_VulkanRendererData->RenderCaps;
		auto& properties = VulkanContext::GetCurrentDevice()->GetPhysicalDevice()->GetProperties();
		caps.Vendor = Utils::VulkanVendorIDToString(properties.vendorID);
		caps.Device = properties.deviceName;
		caps.Version = fmt::to_eastl_string(properties.driverVersion);

		auto device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();
		VKUtils::SetDebugUtilsObjectName(device, VK_OBJECT_TYPE_DEVICE, Renderer::GetCapabilities().Device, device);

		Utils::DumpGPUInfo();

		// Create descriptor pools
		Renderer::Submit([]() mutable
		{
			// Create Descriptor Pool
			std::vector<VkDescriptorPoolSize> poolSizes;

			// Add descriptor types based on the support
			poolSizes.push_back({ VK_DESCRIPTOR_TYPE_SAMPLER, 1000 });
			poolSizes.push_back({ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 });
			poolSizes.push_back({ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 5000 });
			poolSizes.push_back({ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 });
			poolSizes.push_back({ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 });
			poolSizes.push_back({ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 });
			poolSizes.push_back({ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 });
			poolSizes.push_back({ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 });
			poolSizes.push_back({ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 });
			poolSizes.push_back({ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 });

			if (VulkanContext::GetCurrentDevice()->GetPhysicalDevice()->IsExtensionSupported(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME))
			{
				poolSizes.push_back({ VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1000 });
			}
			VkDescriptorPoolCreateInfo pool_info = {};
			pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
			pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
			pool_info.maxSets = 100000;
			pool_info.poolSizeCount = (uint32_t)poolSizes.size();
			pool_info.pPoolSizes = poolSizes.data();
			VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();
			uint32_t framesInFlight = Renderer::GetConfig().FramesInFlight;
			for (uint32_t i = 0; i < framesInFlight; i++)
			{
				VK_CHECK_RESULT(vkCreateDescriptorPool(device, &pool_info, nullptr, &s_VulkanRendererData->DescriptorPools[i]));
				//s_VulkanRendererData->DescriptorPoolAllocationCount[i] = 0;
				VKUtils::SetDebugUtilsObjectName(device, VK_OBJECT_TYPE_DESCRIPTOR_POOL, fmt::eastl_format("Vulkan Renderer Descriptor Pool, frame: {}", i), s_VulkanRendererData->DescriptorPools[i]);
			}
			VK_CHECK_RESULT(vkCreateDescriptorPool(device, &pool_info, nullptr, &s_VulkanRendererData->MaterialDescriptorPool));
			VKUtils::SetDebugUtilsObjectName(device, VK_OBJECT_TYPE_DESCRIPTOR_POOL, "Vulkan Renderer Material Descriptor Pool", s_VulkanRendererData->MaterialDescriptorPool);

			VK_CHECK_RESULT(vkCreateDescriptorPool(device, &pool_info, nullptr, &s_VulkanRendererData->DDGIDescriptorPool));
			VKUtils::SetDebugUtilsObjectName(device, VK_OBJECT_TYPE_DESCRIPTOR_POOL, "Vulkan Renderer DDGI Descriptor Pool", s_VulkanRendererData->DDGIDescriptorPool);

			//const auto& probeBlendingDistance = Renderer::GetShaderLibrary()->Get("ProbeBlendingDistanceCS").As<VulkanShader>()->GetSpirvData().at(VK_SHADER_STAGE_COMPUTE_BIT);
			//const auto& probeBlendingIrradiance = Renderer::GetShaderLibrary()->Get("ProbeBlendingIrradianceCS").As<VulkanShader>()->GetSpirvData().at(VK_SHADER_STAGE_COMPUTE_BIT);

			//const auto& probeClassificationUpdate = Renderer::GetShaderLibrary()->Get("ProbeClassificationCS", 0).As<VulkanShader>()->GetSpirvData().at(VK_SHADER_STAGE_COMPUTE_BIT);
			//const auto& probeClassificationReset = Renderer::GetShaderLibrary()->Get("ProbeClassificationCS", 1).As<VulkanShader>()->GetSpirvData().at(VK_SHADER_STAGE_COMPUTE_BIT);

			//const auto& probeRelocationUpdate = Renderer::GetShaderLibrary()->Get("ProbeRelocationCS", 0).As<VulkanShader>()->GetSpirvData().at(VK_SHADER_STAGE_COMPUTE_BIT);
			//const auto& probeRelocationReset = Renderer::GetShaderLibrary()->Get("ProbeRelocationCS", 1).As<VulkanShader>()->GetSpirvData().at(VK_SHADER_STAGE_COMPUTE_BIT);

			//const auto& probeReductionUpdate = Renderer::GetShaderLibrary()->Get("ReductionCS", 0).As<VulkanShader>()->GetSpirvData().at(VK_SHADER_STAGE_COMPUTE_BIT);
			//const auto& probeReductionReset = Renderer::GetShaderLibrary()->Get("ReductionCS", 1).As<VulkanShader>()->GetSpirvData().at(VK_SHADER_STAGE_COMPUTE_BIT);

			//auto& ddgi = s_VulkanRendererData->DDGIResources;
			//ddgi.managed.enabled = true;
			//ddgi.managed.device = device;
			//ddgi.managed.physicalDevice = VulkanContext::GetCurrentDevice()->GetPhysicalDevice()->GetVulkanPhysicalDevice();
			//ddgi.managed.descriptorPool = s_VulkanRendererData->DDGIDescriptorPool;
			//ddgi.managed.probeBlendingIrradianceCS = { probeBlendingIrradiance.data(), probeBlendingIrradiance.size() * sizeof(uint32_t) };
			//ddgi.managed.probeBlendingDistanceCS = { probeBlendingDistance.data(), probeBlendingDistance.size() * sizeof(uint32_t) };
			//ddgi.managed.probeRelocation = { {probeRelocationUpdate.data(), probeRelocationUpdate.size() * sizeof(uint32_t)}, {probeRelocationReset.data(), probeRelocationReset.size() * sizeof(uint32_t)} };
			//ddgi.managed.probeClassification = { {probeClassificationUpdate.data(), probeClassificationUpdate.size() * sizeof(uint32_t)}, {probeClassificationReset.data(), probeClassificationReset.size() * sizeof(uint32_t)} };
			//ddgi.managed.probeVariability = { {probeReductionUpdate.data(), probeReductionUpdate.size() * sizeof(uint32_t)}, {probeReductionReset.data(), probeReductionReset.size() * sizeof(uint32_t)} };

		});

		// Create fullscreen quad
		float x = -1;
		float y = -1;
		float width = 2, height = 2;
		struct QuadVertex
		{
			glm::vec3 Position;
			glm::vec2 TexCoord;
		};

		QuadVertex* data = hnew QuadVertex[4];

		data[0].Position = glm::vec3(x, y, 0.0f);
		data[0].TexCoord = glm::vec2(0, 0);

		data[1].Position = glm::vec3(x + width, y, 0.0f);
		data[1].TexCoord = glm::vec2(1, 0);

		data[2].Position = glm::vec3(x + width, y + height, 0.0f);
		data[2].TexCoord = glm::vec2(1, 1);

		data[3].Position = glm::vec3(x, y + height, 0.0f);
		data[3].TexCoord = glm::vec2(0, 1);

		s_VulkanRendererData->QuadVertexBuffer = VertexBuffer::Create(data, 4 * sizeof(QuadVertex), "VulkanRenderer fullscreen quad");
		uint32_t indices[6] = { 0, 1, 2, 2, 3, 0, };
		s_VulkanRendererData->QuadIndexBuffer = IndexBuffer::Create(indices, "VulkanRenderer fullscreen quad", 6 * sizeof(uint32_t));

		s_VulkanRendererData->BRDFLut = Renderer::GetBRDFLutTexture();

		UpdateBindlessDescriptorSet(true);
	}

	void VulkanRenderer::InitBindlessDescriptorSetManager()
	{
		BindlessDescriptorSetManagerSpecification bindlessSpec;
		bindlessSpec.DebugName = "Bindless";
		bindlessSpec.Set = (uint32_t)DescriptorSetAlias::Bindless;
		bindlessSpec.DynamicSet = (uint32_t)DescriptorSetAlias::DynamicBindless;
		s_VulkanRendererData = hnew VulkanRendererData{ .BindlessDescriptorSetManager = bindlessSpec };
		const auto& config = Renderer::GetConfig();
		s_VulkanRendererData->DescriptorPools.resize(config.FramesInFlight);
		s_VulkanRendererData->DescriptorPoolAllocationCount.resize(config.FramesInFlight);
	}

	void VulkanRenderer::Shutdown()
	{
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();
		vkDeviceWaitIdle(device);

		/*if (s_VulkanRendererData->PointSampler)
		{
			Vulkan::DestroySampler(s_VulkanRendererData->PointSampler);
		}

		if (s_VulkanRendererData->BilinearSampler)
		{
			Vulkan::DestroySampler(s_VulkanRendererData->BilinearSampler);
		}*/

#if BEY_HAS_SHADER_COMPILER
		VulkanShaderCompiler::ClearUniformBuffers();
#endif
		/*s_VulkanRendererData->BindlessDescriptorUpdateQueue.clear();
		s_VulkanRendererData->DescriptorPools.clear();
		hdelete s_VulkanRendererData;
		s_VulkanRendererData = nullptr;*/
	}



	RendererCapabilities& VulkanRenderer::GetCapabilities()
	{
		return s_VulkanRendererData->RenderCaps;
	}

	Ref<Sampler> VulkanRenderer::GetBilinearSampler()
	{
		if (s_VulkanRendererData->BilinearSampler)
			return s_VulkanRendererData->BilinearSampler;

		SamplerSpecification spec;
		spec.DebugName = "BilinearSampler";
		spec.CreateBindlessDescriptor = true;
		spec.AddressModeU = TextureWrap::Repeat;
		spec.AddressModeV = TextureWrap::Repeat;
		spec.AddressModeW = TextureWrap::Repeat;
		spec.MagFilter = TextureFilter::Linear;
		spec.MinFilter = TextureFilter::Linear;
		spec.MipmapMode = MipmapMode::Nearest;
		spec.BorderColor = {};
		spec.CompareEnable = false;
		spec.CompareOp = DepthCompareOperator::Always;
		spec.MinLod = 0.f;
		spec.MaxLod = FLT_MAX;

		s_VulkanRendererData->BilinearSampler = Sampler::Create(spec);

		return s_VulkanRendererData->BilinearSampler;
	}

	Ref<Sampler> VulkanRenderer::GetPointSampler()
	{
		if (s_VulkanRendererData->PointSampler)
			return s_VulkanRendererData->PointSampler;


		SamplerSpecification spec;
		spec.DebugName = "PointSampler";
		spec.CreateBindlessDescriptor = true;
		spec.AddressModeU = TextureWrap::ClampToEdge;
		spec.AddressModeV = TextureWrap::ClampToEdge;
		spec.AddressModeW = TextureWrap::ClampToEdge;
		spec.MagFilter = TextureFilter::Nearest;
		spec.MinFilter = TextureFilter::Nearest;
		spec.MipmapMode = MipmapMode::Nearest;
		spec.BorderColor = {};
		spec.CompareEnable = false;
		spec.CompareOp = DepthCompareOperator::Always;
		spec.MinLod = 0.f;
		spec.MaxLod = FLT_MAX;

		s_VulkanRendererData->PointSampler = Sampler::Create(spec);

		return s_VulkanRendererData->PointSampler;
	}

	Ref<Sampler> VulkanRenderer::GetAnisoSampler()
	{
		if (s_VulkanRendererData->AnisoSampler)
			return s_VulkanRendererData->AnisoSampler;


		SamplerSpecification spec;
		spec.DebugName = "AnisoSampler";
		spec.CreateBindlessDescriptor = true;
		spec.AddressModeU = TextureWrap::Repeat;
		spec.AddressModeV = TextureWrap::Repeat;
		spec.AddressModeW = TextureWrap::Repeat;
		spec.MagFilter = TextureFilter::Linear;
		spec.MinFilter = TextureFilter::Linear;
		spec.MipmapMode = MipmapMode::Linear;
		spec.BorderColor = {};
		spec.CompareEnable = false;
		spec.AnisotropyEnable = true;
		spec.MaxAnisotropy = 16.0f;
		spec.CompareOp = DepthCompareOperator::Always;
		spec.MinLod = 0.f;
		spec.MaxLod = FLT_MAX;

		s_VulkanRendererData->AnisoSampler = Sampler::Create(spec);

		return s_VulkanRendererData->AnisoSampler;
	}

	std::vector<uint32_t> VulkanRenderer::GetBindlessSets()
	{
		return s_VulkanRendererData->BindlessDescriptorSetManager.GetSets();
	}

	int32_t& VulkanRenderer::GetSelectedDrawCall()
	{
		return s_VulkanRendererData->SelectedDrawCall;
	}

	void VulkanRenderer::RenderStaticMesh(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<RasterPipeline> pipeline, Ref<StaticMesh> mesh, uint32_t submeshIndex, Ref<MaterialTable> materialTable, uint32_t drawID, uint32_t instanceCount)
	{
		BEY_CORE_VERIFY(mesh);
		BEY_CORE_VERIFY(materialTable);

		Renderer::Submit([renderCommandBuffer, pipeline, drawID, mesh, submeshIndex, materialTable = Ref<MaterialTable>::Create(materialTable), instanceCount]() mutable
		{
			BEY_PROFILE_SCOPE_DYNAMIC("VulkanRenderer::RenderMesh");
			BEY_SCOPE_PERF("VulkanRenderer::RenderMesh");

			if (s_VulkanRendererData->SelectedDrawCall != -1 && s_VulkanRendererData->DrawCallCount > s_VulkanRendererData->SelectedDrawCall)
				return;

			uint32_t frameIndex = Renderer::RT_GetCurrentFrameIndex();
			VkCommandBuffer commandBuffer = renderCommandBuffer.As<VulkanRenderCommandBuffer>()->GetActiveCommandBuffer();

			Ref<MeshSource> meshSource = mesh->GetMeshSource();
			Ref<VulkanVertexBuffer> vulkanMeshVB = meshSource->GetVertexBuffer().As<VulkanVertexBuffer>();
			VkBuffer vbMeshBuffer = vulkanMeshVB->GetVulkanBuffer();
			VkDeviceSize offsets[1] = { 0 };
			vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vbMeshBuffer, offsets);

			auto vulkanMeshIB = Ref<VulkanIndexBuffer>(meshSource->GetIndexBuffer());
			VkBuffer ibBuffer = vulkanMeshIB->GetVulkanBuffer();
			vkCmdBindIndexBuffer(commandBuffer, ibBuffer, 0, VK_INDEX_TYPE_UINT32);

			Ref<VulkanRasterPipeline> vulkanPipeline = pipeline.As<VulkanRasterPipeline>();

			std::vector<std::vector<VkWriteDescriptorSet>> writeDescriptors;

			const auto& submeshes = meshSource->GetSubmeshes();
			const Submesh& submesh = submeshes[submeshIndex];
			Ref<MaterialTable> meshMaterialTable = mesh->GetMaterials();
			uint32_t materialCount = meshMaterialTable->GetMaterialCount();
			// NOTE: probably should not involve Asset Manager at this stage
			AssetHandle materialHandle = materialTable->HasMaterial(submesh.MaterialIndex) ? materialTable->GetMaterial(submesh.MaterialIndex) : meshMaterialTable->GetMaterial(submesh.MaterialIndex);
			Ref<MaterialAsset> material = AssetManager::GetAsset<MaterialAsset>(materialHandle);
			Ref<VulkanMaterial> vulkanMaterial = material->GetMaterial().As<VulkanMaterial>();
			BEY_CORE_ASSERT(vulkanMaterial->GetShader()->GetHash() == vulkanPipeline->GetShader()->GetHash());

			if (s_VulkanRendererData->SelectedDrawCall != -1 && s_VulkanRendererData->DrawCallCount > s_VulkanRendererData->SelectedDrawCall)
				return;

			VkPipelineLayout layout = vulkanPipeline->GetVulkanPipelineLayout();
			//vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, (uint32_t)descriptorSets.size(), descriptorSets.data(), 0, nullptr);

			VkDescriptorSet descriptorSet = vulkanMaterial->GetDescriptorSet(frameIndex);
			if (descriptorSet)
				vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, 1, &descriptorSet, 0, nullptr);
			uint32_t pushConstOffset = 0;
			vkCmdPushConstants(commandBuffer, layout, VK_SHADER_STAGE_VERTEX_BIT, 0, 4, &drawID);
			pushConstOffset += 16;

			const Buffer uniformStorageBuffer = vulkanMaterial->GetUniformStorageBuffer();
			BEY_CORE_ASSERT(sizeof(MaterialBuffer) == uniformStorageBuffer.GetSize());
			MaterialBuffer materialBuffer;
			std::memcpy(&materialBuffer, (std::byte*)uniformStorageBuffer.Data, sizeof (MaterialBuffer));

			vkCmdPushConstants(commandBuffer, layout, VK_SHADER_STAGE_FRAGMENT_BIT, pushConstOffset, sizeof(MaterialBuffer), &materialBuffer);

			SET_VULKAN_CHECKPOINT(commandBuffer, fmt::eastl_format("VulkanRenderer::RenderStaticMesh, Shader: {}", pipeline->GetShader()->GetName()));

			vkCmdDrawIndexed(commandBuffer, submesh.IndexCount, instanceCount, submesh.BaseIndex, submesh.BaseVertex, 0);
			s_VulkanRendererData->DrawCallCount++;
		});
	}

	void VulkanRenderer::RenderSubmeshInstanced(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<RasterPipeline> pipeline, Ref<Mesh> mesh, uint32_t submeshIndex, Ref<MaterialTable> materialTable, uint32_t boneTransformsOffset, uint32_t drawID, uint32_t instanceCount)
	{
		BEY_CORE_VERIFY(mesh);
		BEY_CORE_VERIFY(materialTable);

		Renderer::Submit([renderCommandBuffer, pipeline, mesh, drawID, submeshIndex, materialTable, boneTransformsOffset, instanceCount]() mutable
		{
			BEY_PROFILE_SCOPE_DYNAMIC("VulkanRenderer::RenderSubmeshInstanced");
			BEY_SCOPE_PERF("VulkanRenderer::RenderSubmeshInstanced");

			if (s_VulkanRendererData->SelectedDrawCall != -1 && s_VulkanRendererData->DrawCallCount > s_VulkanRendererData->SelectedDrawCall)
				return;

			uint32_t frameIndex = Renderer::RT_GetCurrentFrameIndex();
			VkCommandBuffer commandBuffer = renderCommandBuffer.As<VulkanRenderCommandBuffer>()->GetActiveCommandBuffer();

			Ref<MeshSource> meshSource = mesh->GetMeshSource();
			Ref<VulkanVertexBuffer> vulkanMeshVB = meshSource->GetVertexBuffer().As<VulkanVertexBuffer>();
			VkBuffer vbMeshBuffer = vulkanMeshVB->GetVulkanBuffer();
			VkDeviceSize vertexOffsets[1] = { 0 };
			vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vbMeshBuffer, vertexOffsets);

			auto vulkanMeshIB = Ref<VulkanIndexBuffer>(meshSource->GetIndexBuffer());
			VkBuffer ibBuffer = vulkanMeshIB->GetVulkanBuffer();
			vkCmdBindIndexBuffer(commandBuffer, ibBuffer, 0, VK_INDEX_TYPE_UINT32);

			Ref<VulkanRasterPipeline> vulkanPipeline = pipeline.As<VulkanRasterPipeline>();

			const auto& submeshes = meshSource->GetSubmeshes();
			const auto& submesh = submeshes[submeshIndex];

			if (submesh.IsRigged)
			{
				VkBuffer boneInfluenceVB = meshSource->GetBoneInfluenceBuffer().As<VulkanVertexBuffer>()->GetVulkanBuffer();
				vkCmdBindVertexBuffers(commandBuffer, 1, 1, &boneInfluenceVB, vertexOffsets);
			}

			Ref<MaterialTable> meshMaterialTable = mesh->GetMaterials();
			uint32_t materialCount = meshMaterialTable->GetMaterialCount();
			// NOTE: probably should not involve Asset Manager at this stage
			AssetHandle materialHandle = materialTable->HasMaterial(submesh.MaterialIndex) ? materialTable->GetMaterial(submesh.MaterialIndex) : meshMaterialTable->GetMaterial(submesh.MaterialIndex);
			Ref<MaterialAsset> material = AssetManager::GetAsset<MaterialAsset>(materialHandle);

			if (s_VulkanRendererData->SelectedDrawCall != -1 && s_VulkanRendererData->DrawCallCount > s_VulkanRendererData->SelectedDrawCall)
				return;

			VkPipelineLayout layout = vulkanPipeline->GetVulkanPipelineLayout();
			Ref<VulkanMaterial> vulkanMaterial = material->GetMaterial().As<VulkanMaterial>();
			uint32_t pushConstantOffset = 0;
			if (submesh.IsRigged)
			{
				struct
				{
					uint32_t BoneOffset;
					uint32_t DrawID;
				} vertexPush;
				vertexPush.BoneOffset = boneTransformsOffset;
				vertexPush.DrawID = drawID;
				vkCmdPushConstants(commandBuffer, layout, VK_SHADER_STAGE_VERTEX_BIT, pushConstantOffset, sizeof(vertexPush), &vertexPush);
				pushConstantOffset += 16; // TODO: it's 16 because that happens to be what's declared in the layouts in the shaders.  Need a better way of doing this.  Cannot just use the size of the pushConstantBuffer, because you dont know what alignment the next push constant range might have
			}
			else
			{
				vkCmdPushConstants(commandBuffer, layout, VK_SHADER_STAGE_VERTEX_BIT, pushConstantOffset, sizeof(uint32_t), &drawID);
				pushConstantOffset += 16; // TODO: it's 16 because that happens to be what's declared in the layouts in the shaders.  Need a better way of doing this.  Cannot just use the size of the pushConstantBuffer, because you dont know what alignment the next push constant range might have
			}

			if (vulkanMaterial)
			{
				VkDescriptorSet descriptorSet = vulkanMaterial->GetDescriptorSet(frameIndex);
				if (descriptorSet)
					vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, 1, &descriptorSet, 0, nullptr);

				const Buffer uniformStorageBuffer = vulkanMaterial->GetUniformStorageBuffer();
				if (uniformStorageBuffer)
				{
					MaterialBuffer materialBuffer;
					std::memcpy(&materialBuffer, (std::byte*)uniformStorageBuffer.Data, sizeof(MaterialBuffer));
					vkCmdPushConstants(commandBuffer, layout, VK_SHADER_STAGE_FRAGMENT_BIT, pushConstantOffset, sizeof(MaterialBuffer), &materialBuffer);
				}
			}
			SET_VULKAN_CHECKPOINT(commandBuffer, fmt::eastl_format("VulkanRenderer::RenderSubmeshInstanced, Shader: {}", pipeline->GetShader()->GetName()));

			vkCmdDrawIndexed(commandBuffer, submesh.IndexCount, instanceCount, submesh.BaseIndex, submesh.BaseVertex, 0);
			s_VulkanRendererData->DrawCallCount++;
		});
	}

	void VulkanRenderer::RenderMeshWithMaterial(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<RasterPipeline> pipeline, Ref<Mesh> mesh, uint32_t submeshIndex, Ref<Material> material, uint32_t boneTransformsOffset, uint32_t drawID, uint32_t instanceCount, Buffer additionalUniforms)
	{
		BEY_CORE_ASSERT(mesh);
		BEY_CORE_ASSERT(mesh->GetMeshSource());

		Buffer pushConstantBuffer;
		bool isRigged = mesh->GetMeshSource()->IsSubmeshRigged(submeshIndex);

		pushConstantBuffer.Allocate(additionalUniforms.Size + (isRigged ? sizeof(uint32_t) : 0) + sizeof(drawID));
		if (additionalUniforms.Size || isRigged)
		{
			if (additionalUniforms.Size)
				pushConstantBuffer.Write(additionalUniforms.Data, additionalUniforms.Size);

			if (isRigged)
				pushConstantBuffer.Write(&boneTransformsOffset, sizeof(uint32_t), additionalUniforms.Size);

		}
		pushConstantBuffer.Write(&drawID, sizeof(drawID), additionalUniforms.Size + (isRigged ? sizeof(uint32_t) : 0));

		Ref<VulkanMaterial> vulkanMaterial = material.As<VulkanMaterial>();
		Renderer::Submit([renderCommandBuffer, pipeline, mesh, submeshIndex, vulkanMaterial, instanceCount, pushConstantBuffer]() mutable
		{
			BEY_PROFILE_FUNC("VulkanRenderer::RenderMeshWithMaterial");
			BEY_SCOPE_PERF("VulkanRenderer::RenderMeshWithMaterial");

			uint32_t frameIndex = Renderer::RT_GetCurrentFrameIndex();
			VkCommandBuffer commandBuffer = renderCommandBuffer.As<VulkanRenderCommandBuffer>()->GetActiveCommandBuffer();

			Ref<MeshSource> meshSource = mesh->GetMeshSource();
			VkBuffer meshVB = meshSource->GetVertexBuffer().As<VulkanVertexBuffer>()->GetVulkanBuffer();
			VkDeviceSize vertexOffsets[1] = { 0 };
			vkCmdBindVertexBuffers(commandBuffer, 0, 1, &meshVB, vertexOffsets);

			VkBuffer meshIB = meshSource->GetIndexBuffer().As<VulkanIndexBuffer>()->GetVulkanBuffer();
			vkCmdBindIndexBuffer(commandBuffer, meshIB, 0, VK_INDEX_TYPE_UINT32);

			//RT_UpdateMaterialForRendering(vulkanMaterial, uniformBufferSet, storageBufferSet);

			Ref<VulkanRasterPipeline> vulkanPipeline = pipeline.As<VulkanRasterPipeline>();

			const auto& submeshes = meshSource->GetSubmeshes();
			const auto& submesh = submeshes[submeshIndex];

			if (submesh.IsRigged)
			{
				Ref<VulkanVertexBuffer> vulkanBoneInfluencesVB = meshSource->GetBoneInfluenceBuffer().As<VulkanVertexBuffer>();
				VkBuffer vbBoneInfluencesBuffer = vulkanBoneInfluencesVB->GetVulkanBuffer();
				vkCmdBindVertexBuffers(commandBuffer, 1, 1, &vbBoneInfluencesBuffer, vertexOffsets);
			}

			VkPipelineLayout layout = vulkanPipeline->GetVulkanPipelineLayout();

			uint32_t pushConstantOffset = 0;
			if (pushConstantBuffer.Size)
			{
				vkCmdPushConstants(commandBuffer, layout, VK_SHADER_STAGE_VERTEX_BIT, pushConstantOffset, (uint32_t)pushConstantBuffer.Size, pushConstantBuffer.Data);
				pushConstantOffset += 16;  // TODO: it's 16 because that happens to be what's declared in the layouts in the shaders.  Need a better way of doing this.  Cannot just use the size of the pushConstantBuffer, because you dont know what alignment the next push constant range might have
			}

			// Bind descriptor sets describing shader binding points
			// NOTE: Descriptor Set 0 is the material, Descriptor Set 1 (if present) is the animation data
			// std::vector<VkDescriptorSet> descriptorSets;
			if (vulkanMaterial)
			{
				VkDescriptorSet descriptorSet = vulkanMaterial->GetDescriptorSet(frameIndex);
				if (descriptorSet)
					vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, 1, &descriptorSet, 0, nullptr);

				Buffer uniformStorageBuffer = vulkanMaterial->GetUniformStorageBuffer();
				if (uniformStorageBuffer.Size)
					vkCmdPushConstants(commandBuffer, layout, VK_SHADER_STAGE_FRAGMENT_BIT, pushConstantOffset, (uint32_t)uniformStorageBuffer.Size, uniformStorageBuffer.Data);
			}

			SET_VULKAN_CHECKPOINT(commandBuffer, fmt::eastl_format("VulkanRenderer::RenderMeshWithMaterial, Shader: {}", pipeline->GetShader()->GetName()));
			vkCmdDrawIndexed(commandBuffer, submesh.IndexCount, instanceCount, submesh.BaseIndex, submesh.BaseVertex, 0);

			pushConstantBuffer.Release();
		});
	}

	void VulkanRenderer::RenderStaticMeshWithMaterial(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<RasterPipeline> pipeline, Ref<StaticMesh> staticMesh, uint32_t submeshIndex, Ref<Material> material, uint32_t drawID, uint32_t instanceCount, Buffer additionalUniforms /*= Buffer()*/)
	{
		BEY_CORE_ASSERT(staticMesh);
		BEY_CORE_ASSERT(staticMesh->GetMeshSource());

		Buffer pushConstantBuffer;
		pushConstantBuffer.Allocate(additionalUniforms.Size + sizeof(drawID));
		if (additionalUniforms.Size)
			pushConstantBuffer.Write(additionalUniforms.Data, additionalUniforms.Size);
		pushConstantBuffer.Write(&drawID, sizeof(drawID), additionalUniforms.Size);

		Ref<VulkanMaterial> vulkanMaterial = material.As<VulkanMaterial>();
		Renderer::Submit([renderCommandBuffer, pipeline, staticMesh, drawID, submeshIndex, vulkanMaterial, instanceCount, pushConstantBuffer]() mutable
		{
			BEY_PROFILE_FUNC("VulkanRenderer::RenderMeshWithMaterial");
			BEY_SCOPE_PERF("VulkanRenderer::RenderMeshWithMaterial");

			uint32_t frameIndex = Renderer::RT_GetCurrentFrameIndex();
			VkCommandBuffer commandBuffer = renderCommandBuffer.As<VulkanRenderCommandBuffer>()->GetActiveCommandBuffer();

			Ref<MeshSource> meshSource = staticMesh->GetMeshSource();
			auto vulkanMeshVB = meshSource->GetVertexBuffer().As<VulkanVertexBuffer>();
			VkBuffer vbMeshBuffer = vulkanMeshVB->GetVulkanBuffer();
			VkDeviceSize vertexOffsets[1] = { 0 };
			vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vbMeshBuffer, vertexOffsets);

			auto vulkanMeshIB = Ref<VulkanIndexBuffer>(meshSource->GetIndexBuffer());
			VkBuffer ibBuffer = vulkanMeshIB->GetVulkanBuffer();
			vkCmdBindIndexBuffer(commandBuffer, ibBuffer, 0, VK_INDEX_TYPE_UINT32);

			Ref<VulkanRasterPipeline> vulkanPipeline = pipeline.As<VulkanRasterPipeline>();
			VkPipelineLayout layout = vulkanPipeline->GetVulkanPipelineLayout();

			// Bind descriptor sets describing shader binding points
			// TODO std::vector<VkDescriptorSet> descriptorSets = resourceSets.As<VulkanResourceSets>()->GetDescriptorSets();
			// TODO VkDescriptorSet descriptorSet = vulkanMaterial->GetDescriptorSet(frameIndex);
			// TODO descriptorSets[0] = descriptorSet;
			// TODO vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, (uint32_t)descriptorSets.size(), descriptorSets.data(), 0, nullptr);

			Buffer uniformStorageBuffer = vulkanMaterial->GetUniformStorageBuffer();
			uint32_t pushConstantOffset = 0;
			if (pushConstantBuffer.Size)
			{
				vkCmdPushConstants(commandBuffer, layout, VK_SHADER_STAGE_VERTEX_BIT, pushConstantOffset, (uint32_t)pushConstantBuffer.Size, pushConstantBuffer.Data);
				pushConstantOffset += 16; // TODO: it's 16 because that happens to be what's declared in the layouts in the shaders.  Need a better way of doing this.  Cannot just use the size of the pushConstantBuffer, because you dont know what alignment the next push constant range might have
			}

			if (uniformStorageBuffer)
			{
				vkCmdPushConstants(commandBuffer, layout, VK_SHADER_STAGE_FRAGMENT_BIT, pushConstantOffset, (uint32_t)uniformStorageBuffer.Size, uniformStorageBuffer.Data);
				pushConstantOffset += (uint32_t)uniformStorageBuffer.Size;
			}

			const auto& submeshes = meshSource->GetSubmeshes();
			const auto& submesh = submeshes[submeshIndex];

			SET_VULKAN_CHECKPOINT(commandBuffer, fmt::eastl_format("VulkanRenderer::RenderStaticMeshWithMaterial, Shader: {}", pipeline->GetShader()->GetName()));
			vkCmdDrawIndexed(commandBuffer, submesh.IndexCount, instanceCount, submesh.BaseIndex, submesh.BaseVertex, 0);

			pushConstantBuffer.Release();
		});
	}

	void VulkanRenderer::RenderQuad(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<RasterPipeline> pipeline, Ref<Material> material, const glm::mat4& transform)
	{
		Ref<VulkanMaterial> vulkanMaterial = material.As<VulkanMaterial>();
		Renderer::Submit([renderCommandBuffer, pipeline, vulkanMaterial, transform]() mutable
		{
			BEY_PROFILE_FUNC("VulkanRenderer::RenderQuad");

			uint32_t frameIndex = Renderer::RT_GetCurrentFrameIndex();
			VkCommandBuffer commandBuffer = renderCommandBuffer.As<VulkanRenderCommandBuffer>()->GetActiveCommandBuffer();

			Ref<VulkanRasterPipeline> vulkanPipeline = pipeline.As<VulkanRasterPipeline>();

			VkPipelineLayout layout = vulkanPipeline->GetVulkanPipelineLayout();

			auto vulkanMeshVB = s_VulkanRendererData->QuadVertexBuffer.As<VulkanVertexBuffer>();
			VkBuffer vbMeshBuffer = vulkanMeshVB->GetVulkanBuffer();
			VkDeviceSize offsets[1] = { 0 };
			vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vbMeshBuffer, offsets);

			auto vulkanMeshIB = s_VulkanRendererData->QuadIndexBuffer.As<VulkanIndexBuffer>();
			VkBuffer ibBuffer = vulkanMeshIB->GetVulkanBuffer();
			vkCmdBindIndexBuffer(commandBuffer, ibBuffer, 0, VK_INDEX_TYPE_UINT32);

			Buffer uniformStorageBuffer = vulkanMaterial->GetUniformStorageBuffer();

			vkCmdPushConstants(commandBuffer, layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &transform);
			vkCmdPushConstants(commandBuffer, layout, VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(glm::mat4), (uint32_t)uniformStorageBuffer.Size, uniformStorageBuffer.Data);
			SET_VULKAN_CHECKPOINT(commandBuffer, fmt::eastl_format("VulkanRenderer::RenderQuad, Shader: {}", pipeline->GetShader()->GetName()));
			vkCmdDrawIndexed(commandBuffer, (uint32_t)s_VulkanRendererData->QuadIndexBuffer->GetCount(), 1, 0, 0, 0);
		});
	}

	void VulkanRenderer::RenderGeometry(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<RasterPipeline> pipeline, Ref<Material> material, Ref<VertexBuffer> vertexBuffer, Ref<IndexBuffer> indexBuffer, const glm::mat4& transform, uint32_t indexCount /*= 0*/)
	{
		Ref<VulkanMaterial> vulkanMaterial = material.As<VulkanMaterial>();
		if (indexCount == 0)
			indexCount = (uint32_t)indexBuffer->GetCount();

		Renderer::Submit([renderCommandBuffer, pipeline, vulkanMaterial, vertexBuffer, indexBuffer, transform, indexCount]() mutable
		{
			BEY_PROFILE_FUNC("VulkanRenderer::RenderGeometry");

			uint32_t frameIndex = Renderer::RT_GetCurrentFrameIndex();
			VkCommandBuffer commandBuffer = renderCommandBuffer.As<VulkanRenderCommandBuffer>()->GetActiveCommandBuffer();

			Ref<VulkanRasterPipeline> vulkanPipeline = pipeline.As<VulkanRasterPipeline>();

			VkPipelineLayout layout = vulkanPipeline->GetVulkanPipelineLayout();

			auto vulkanMeshVB = vertexBuffer.As<VulkanVertexBuffer>();
			VkBuffer vbMeshBuffer = vulkanMeshVB->GetVulkanBuffer();
			VkDeviceSize offsets[1] = { 0 };
			vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vbMeshBuffer, offsets);

			auto vulkanMeshIB = indexBuffer.As<VulkanIndexBuffer>();
			VkBuffer ibBuffer = vulkanMeshIB->GetVulkanBuffer();
			vkCmdBindIndexBuffer(commandBuffer, ibBuffer, 0, VK_INDEX_TYPE_UINT32);

			VkDescriptorSet descriptorSet = vulkanMaterial->GetDescriptorSet(frameIndex);
			if (descriptorSet)
				vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, 1, &descriptorSet, 0, nullptr);

			vkCmdPushConstants(commandBuffer, layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &transform);
			Buffer uniformStorageBuffer = vulkanMaterial->GetUniformStorageBuffer();
			if (uniformStorageBuffer)
				vkCmdPushConstants(commandBuffer, layout, VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(glm::mat4), (uint32_t)uniformStorageBuffer.Size, uniformStorageBuffer.Data);

			SET_VULKAN_CHECKPOINT(commandBuffer, fmt::eastl_format("VulkanRenderer::RenderGeometry, Shader: {}", pipeline->GetShader()->GetName()));
			vkCmdDrawIndexed(commandBuffer, indexCount, 1, 0, 0, 0);
		});
	}

	VkDescriptorSet VulkanRenderer::RT_AllocateDescriptorSet(VkDescriptorSetAllocateInfo& allocInfo)
	{
		BEY_PROFILE_FUNC();

		uint32_t bufferIndex = Renderer::RT_GetCurrentFrameIndex();
		allocInfo.descriptorPool = s_VulkanRendererData->DescriptorPools[bufferIndex];
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();
		VkDescriptorSet result;
		VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &result));
		VKUtils::SetDebugUtilsObjectName(device, VK_OBJECT_TYPE_DESCRIPTOR_SET, fmt::eastl_format("Renderer Descriptor Set, frame: {}", bufferIndex), result);

		s_VulkanRendererData->DescriptorPoolAllocationCount[bufferIndex] += allocInfo.descriptorSetCount;
		return result;
	}

	VkDescriptorSet VulkanRenderer::AllocateMaterialDescriptorSet(VkDescriptorSetAllocateInfo& allocInfo, uint32_t set, uint32_t frame, std::string_view shaderName)
	{
		BEY_PROFILE_FUNC();

		uint32_t bufferIndex = Renderer::RT_GetCurrentFrameIndex();
		allocInfo.descriptorPool = s_VulkanRendererData->MaterialDescriptorPool;
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();
		VkDescriptorSet result;
		VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &result));
		VKUtils::SetDebugUtilsObjectName(device, VK_OBJECT_TYPE_DESCRIPTOR_SET, fmt::eastl_format("Shader: {}, Frame: {}, Set: {}", shaderName, frame, set), result);

		//s_VulkanRendererData->DescriptorPoolAllocationCount[bufferIndex] += allocInfo.descriptorSetCount;
		return result;
	}

	void VulkanRenderer::ClearImage(Ref<RenderCommandBuffer> commandBuffer, Ref<Image2D> image, const ImageClearValue& clearValue, ImageSubresourceRange subresourceRange)
	{
		Renderer::Submit([commandBuffer, image = image.As<VulkanImage2D>(), clearValue, subresourceRange]
		{
			const auto vulkanCommandBuffer = commandBuffer.As<VulkanRenderCommandBuffer>()->GetCommandBuffer(Renderer::RT_GetCurrentFrameIndex());
			VkImageSubresourceRange vulkanSubresourceRange{};
			vulkanSubresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			vulkanSubresourceRange.baseMipLevel = subresourceRange.BaseMip;
			vulkanSubresourceRange.levelCount = subresourceRange.MipCount;
			vulkanSubresourceRange.baseArrayLayer = subresourceRange.BaseLayer;
			vulkanSubresourceRange.layerCount = subresourceRange.LayerCount;

			vkCmdClearColorImage(vulkanCommandBuffer, image->GetImageInfo().Image,
				image->GetVulkanDescriptorInfo().imageLayout,
				(VkClearColorValue*)&clearValue, 1, &vulkanSubresourceRange);
		});
	}

	void VulkanRenderer::CopyImage(Ref<RenderCommandBuffer> commandBuffer, Ref<Image2D> sourceImage, Ref<Image2D> destinationImage)
	{
		Renderer::Submit([commandBuffer, src = sourceImage.As<VulkanImage2D>(), dst = destinationImage.As<VulkanImage2D>()]
		{
			const auto vulkanCommandBuffer = commandBuffer.As<VulkanRenderCommandBuffer>()->GetCommandBuffer(Renderer::RT_GetCurrentFrameIndex());

			VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();

			VkImage srcImage = src->GetImageInfo().Image;
			VkImage dstImage = dst->GetImageInfo().Image;
			glm::uvec2 srcSize = src->GetSize();
			glm::uvec2 dstSize = dst->GetSize();

			VkImageCopy region;
			region.srcOffset = { 0, 0, 0 };
			region.dstOffset = { 0, 0, 0 };
			region.extent = { srcSize.x, srcSize.y, 1 };
			region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			region.srcSubresource.baseArrayLayer = 0;
			region.srcSubresource.mipLevel = 0;
			region.srcSubresource.layerCount = 1;
			region.dstSubresource = region.srcSubresource;

			VkImageLayout srcImageLayout = src->GetVulkanDescriptorInfo().imageLayout;
			VkImageLayout dstImageLayout = dst->GetVulkanDescriptorInfo().imageLayout;

			{
				VkImageMemoryBarrier srcImageMemoryBarrier{};
				srcImageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
				srcImageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
				srcImageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
				srcImageMemoryBarrier.oldLayout = srcImageLayout;
				srcImageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
				srcImageMemoryBarrier.image = srcImage;

				srcImageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				srcImageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
				srcImageMemoryBarrier.subresourceRange.baseMipLevel = 0;
				srcImageMemoryBarrier.subresourceRange.layerCount = 1;
				srcImageMemoryBarrier.subresourceRange.levelCount = 1;

				vkCmdPipelineBarrier(vulkanCommandBuffer, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
					0, nullptr,
					0, nullptr,
					1, &srcImageMemoryBarrier);
			}

			{
				VkImageMemoryBarrier dstImageMemoryBarrier{};
				dstImageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
				dstImageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
				dstImageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
				dstImageMemoryBarrier.oldLayout = dstImageLayout;
				dstImageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
				dstImageMemoryBarrier.image = dstImage;

				dstImageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				dstImageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
				dstImageMemoryBarrier.subresourceRange.baseMipLevel = 0;
				dstImageMemoryBarrier.subresourceRange.layerCount = 1;
				dstImageMemoryBarrier.subresourceRange.levelCount = 1;

				vkCmdPipelineBarrier(vulkanCommandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
					0, nullptr,
					0, nullptr,
					1, &dstImageMemoryBarrier);
			}

			vkCmdCopyImage(vulkanCommandBuffer, srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);


			{
				VkImageMemoryBarrier srcImageMemoryBarrier{};
				srcImageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
				srcImageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
				srcImageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
				srcImageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
				srcImageMemoryBarrier.newLayout = srcImageLayout;
				srcImageMemoryBarrier.image = srcImage;

				srcImageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				srcImageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
				srcImageMemoryBarrier.subresourceRange.baseMipLevel = 0;
				srcImageMemoryBarrier.subresourceRange.layerCount = 1;
				srcImageMemoryBarrier.subresourceRange.levelCount = 1;

				vkCmdPipelineBarrier(vulkanCommandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
					0, nullptr,
					0, nullptr,
					1, &srcImageMemoryBarrier);
			}

			{
				VkImageMemoryBarrier dstImageMemoryBarrier{};
				dstImageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
				dstImageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
				dstImageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;
				dstImageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
				dstImageMemoryBarrier.newLayout = dstImageLayout;
				dstImageMemoryBarrier.image = dstImage;

				dstImageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				dstImageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
				dstImageMemoryBarrier.subresourceRange.baseMipLevel = 0;
				dstImageMemoryBarrier.subresourceRange.layerCount = 1;
				dstImageMemoryBarrier.subresourceRange.levelCount = 1;

				vkCmdPipelineBarrier(vulkanCommandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, 0,
					0, nullptr,
					0, nullptr,
					1, &dstImageMemoryBarrier);
			}
		});
	}

	void VulkanRenderer::BlitDepthImage(Ref<RenderCommandBuffer> commandBuffer, Ref<Image2D> sourceImage, Ref<Image2D> destinationImage)
	{
		Renderer::Submit([commandBuffer, src = sourceImage.As<VulkanImage2D>(), dst = destinationImage.As<VulkanImage2D>()]
		{
			const auto vulkanCommandBuffer = commandBuffer.As<VulkanRenderCommandBuffer>()->GetCommandBuffer(Renderer::RT_GetCurrentFrameIndex());

			VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();

			VkImage srcImage = src->GetImageInfo().Image;
			VkImage dstImage = dst->GetImageInfo().Image;
			glm::uvec2 srcSize = src->GetSize();
			glm::uvec2 dstSize = dst->GetSize();

			VkImageBlit region{ .srcSubresource = {}, .srcOffsets = {}, .dstSubresource = {}, .dstOffsets = {}};
			region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
			region.srcSubresource.baseArrayLayer = 0;
			region.srcSubresource.mipLevel = 0;
			region.srcSubresource.layerCount = 1;
			region.srcOffsets[1].z = 1;
			region.dstOffsets[1].z = 1;

			region.dstSubresource = region.srcSubresource;

			// Set the source offsets
			region.srcOffsets[0] = { 0, 0, 0 };
			region.srcOffsets[1] = { static_cast<int32_t>(srcSize.x), static_cast<int32_t>(srcSize.y), 1 };

			// Set the destination offsets
			region.dstOffsets[0] = { 0, 0, 0 };
			region.dstOffsets[1] = { static_cast<int32_t>(dstSize.x), static_cast<int32_t>(dstSize.y), 1 };

			//// Ensure the offsets are correct even if src is smaller than dst
			//if (srcSize.x < dstSize.x || srcSize.y < dstSize.y)
			//{
			//	region.dstOffsets[1].x = std::min(static_cast<int32_t>(srcSize.x), static_cast<int32_t>(dstSize.x));
			//	region.dstOffsets[1].y = std::min(static_cast<int32_t>(srcSize.y), static_cast<int32_t>(dstSize.y));
			//}

			VkImageLayout srcImageLayout = src->GetVulkanDescriptorInfo().imageLayout;
			VkImageLayout dstImageLayout = dst->GetVulkanDescriptorInfo().imageLayout;

			{
				VkImageMemoryBarrier imageMemoryBarrier{};
				imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
				imageMemoryBarrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
				imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
				imageMemoryBarrier.oldLayout = srcImageLayout;
				imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
				imageMemoryBarrier.image = srcImage;

				imageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
				imageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
				imageMemoryBarrier.subresourceRange.baseMipLevel = 0;
				imageMemoryBarrier.subresourceRange.layerCount = 1;
				imageMemoryBarrier.subresourceRange.levelCount = 1;


				vkCmdPipelineBarrier(vulkanCommandBuffer, VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
					0, nullptr,
					0, nullptr,
					1, &imageMemoryBarrier);
			}

			{
				VkImageMemoryBarrier imageMemoryBarrier{};
				imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
				imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
				imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
				imageMemoryBarrier.oldLayout = dstImageLayout;
				imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
				imageMemoryBarrier.image = dstImage;

				imageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
				imageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
				imageMemoryBarrier.subresourceRange.baseMipLevel = 0;
				imageMemoryBarrier.subresourceRange.layerCount = 1;
				imageMemoryBarrier.subresourceRange.levelCount = 1;

				vkCmdPipelineBarrier(vulkanCommandBuffer, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
					0, nullptr,
					0, nullptr,
					1, &imageMemoryBarrier);
			}

			vkCmdBlitImage(vulkanCommandBuffer, srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region, VK_FILTER_NEAREST);


			 {
			 	VkImageMemoryBarrier imageMemoryBarrier{};
			 	imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			 	imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
			 	imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			 	imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			 	imageMemoryBarrier.newLayout = srcImageLayout;
			 	imageMemoryBarrier.image = srcImage;
			 
			 	imageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
			 	imageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
			 	imageMemoryBarrier.subresourceRange.baseMipLevel = 0;
			 	imageMemoryBarrier.subresourceRange.layerCount = 1;
			 	imageMemoryBarrier.subresourceRange.levelCount = 1;
			 
			 	vkCmdPipelineBarrier(vulkanCommandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
			 		0, nullptr,
			 		0, nullptr,
			 		1, &imageMemoryBarrier);
			 }
			 
			 {
			 	VkImageMemoryBarrier imageMemoryBarrier{};
			 	imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			 	imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
			 	imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			 	imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			 	imageMemoryBarrier.newLayout = dstImageLayout;
			 	imageMemoryBarrier.image = dstImage;
			 
			 	imageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
			 	imageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
			 	imageMemoryBarrier.subresourceRange.baseMipLevel = 0;
			 	imageMemoryBarrier.subresourceRange.layerCount = 1;
			 	imageMemoryBarrier.subresourceRange.levelCount = 1;
			 
			 	vkCmdPipelineBarrier(vulkanCommandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0,
			 		0, nullptr,
			 		0, nullptr,
			 		1, &imageMemoryBarrier);
			 }
		});
	}

	void VulkanRenderer::CopyBuffer(Ref<RenderCommandBuffer> renderCommandBuffer, void* dest,
		Ref<StorageBuffer> storageBuffer)
	{
		BEY_CORE_ASSERT(false);
		///*VulkanAllocator allocator("Copy");

		//auto srcBuffer = storageBuffer.As<VulkanStorageBuffer>();
		//auto* src = allocator.MapMemory<uint8_t>(srcBuffer->GetAllocation());
		//std::memcpy(dest, src, srcBuffer->GetSize());
		//allocator.UnmapMemory(srcBuffer->GetAllocation());*/

		//uint32_t sizeInBytes = 0;
		//auto uploadMemory = storageBuffer.As<VulkanStorageBuffer>()->GetUploadMemory();
		//auto uploadBuffer = storageBuffer.As<VulkanStorageBuffer>()->GetUploadBuffer();
		//auto buffer = storageBuffer.As<VulkanStorageBuffer>()->GetVulkanBuffer();

		//VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();
		//// Copy the instance data to the upload buffer
		//uint8_t* pData = nullptr;
		//VK_CHECK_RESULT(vkMapMemory(device, uploadMemory, 0, sizeInBytes, 0, reinterpret_cast<void**>(&pData)));
		//memcpy(pData, instances.get().data(), sizeInBytes);
		//vkUnmapMemory(device, uploadMemory);

		//// Schedule a copy of the upload buffer to the device buffer
		//VkBufferCopy bufferCopy = {};
		//bufferCopy.size = sizeInBytes;
		//vkCmdCopyBuffer(renderCommandBuffer.As<VulkanRenderCommandBuffer>()->GetActiveCommandBuffer(), uploadBuffer, buffer, 1, &bufferCopy);
	}

	void VulkanRenderer::AddBindlessDescriptor(RenderPassInput&& input)
	{
		s_VulkanRendererData->BindlessDescriptorUpdateQueue.emplace_back(std::forward<RenderPassInput>(input));
	}

	void VulkanRenderer::UpdateBindlessDescriptorSet(bool forceRebakeAll)
	{
		bool needRebake = !s_VulkanRendererData->BindlessDescriptorUpdateQueue.empty();
		for (const auto& input : s_VulkanRendererData->BindlessDescriptorUpdateQueue)
		{
			needRebake |= s_VulkanRendererData->BindlessDescriptorSetManager.SetBindlessInput(input);
		}

		s_VulkanRendererData->BindlessDescriptorUpdateQueue.clear();

		if (forceRebakeAll)
			Renderer::Submit([]
		{
			s_VulkanRendererData->BindlessDescriptorSetManager.BakeAll();
		});
		else/* if (needRebake)*/
			//Renderer::Submit([]
		{
			//s_VulkanRendererData->BindlessDescriptorSetManager.Bake();
			s_VulkanRendererData->BindlessDescriptorSetManager.AllocateDescriptorSets();
			s_VulkanRendererData->BindlessDescriptorSetManager.InvalidateAndUpdate();

		}
		//);
	}

	void VulkanRenderer::AddBindlessShader(Ref<Shader> shader)
	{
		s_VulkanRendererData->BindlessDescriptorSetManager.SetShader(shader);
	}

	void VulkanRenderer::SubmitFullscreenQuad(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<RasterPipeline> pipeline, Ref<Material> material)
	{
		Ref<VulkanMaterial> vulkanMaterial = material.As<VulkanMaterial>();
		Renderer::Submit([renderCommandBuffer, pipeline, vulkanMaterial]() mutable
		{
			BEY_PROFILE_FUNC("VulkanRenderer::SubmitFullscreenQuad");

			uint32_t frameIndex = Renderer::RT_GetCurrentFrameIndex();
			VkCommandBuffer commandBuffer = renderCommandBuffer.As<VulkanRenderCommandBuffer>()->GetActiveCommandBuffer();

			Ref<VulkanRasterPipeline> vulkanPipeline = pipeline.As<VulkanRasterPipeline>();

			VkPipelineLayout layout = vulkanPipeline->GetVulkanPipelineLayout();

			auto vulkanMeshVB = s_VulkanRendererData->QuadVertexBuffer.As<VulkanVertexBuffer>();
			VkBuffer vbMeshBuffer = vulkanMeshVB->GetVulkanBuffer();
			VkDeviceSize offsets[1] = { 0 };
			vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vbMeshBuffer, offsets);

			auto vulkanMeshIB = s_VulkanRendererData->QuadIndexBuffer.As<VulkanIndexBuffer>();
			VkBuffer ibBuffer = vulkanMeshIB->GetVulkanBuffer();
			vkCmdBindIndexBuffer(commandBuffer, ibBuffer, 0, VK_INDEX_TYPE_UINT32);

			if (vulkanMaterial)
			{
				VkDescriptorSet descriptorSet = vulkanMaterial->GetDescriptorSet(frameIndex);
				if (descriptorSet)
					vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, 1, &descriptorSet, 0, nullptr);

				Buffer uniformStorageBuffer = vulkanMaterial->GetUniformStorageBuffer();
				if (uniformStorageBuffer.Size)
					vkCmdPushConstants(commandBuffer, layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, (uint32_t)uniformStorageBuffer.Size, uniformStorageBuffer.Data);
			}

			vkCmdDrawIndexed(commandBuffer, (uint32_t)s_VulkanRendererData->QuadIndexBuffer->GetCount(), 1, 0, 0, 0);
		});
	}

	void VulkanRenderer::SubmitFullscreenQuadWithOverrides(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<RasterPipeline> pipeline, Ref<Material> material, Buffer vertexShaderOverrides, Buffer fragmentShaderOverrides)
	{
		Buffer vertexPushConstantBuffer;
		if (vertexShaderOverrides)
		{
			vertexPushConstantBuffer.Allocate(vertexShaderOverrides.Size);
			vertexPushConstantBuffer.Write(vertexShaderOverrides.Data, vertexShaderOverrides.Size);
		}

		Buffer fragmentPushConstantBuffer;
		if (fragmentShaderOverrides)
		{
			fragmentPushConstantBuffer.Allocate(fragmentShaderOverrides.Size);
			fragmentPushConstantBuffer.Write(fragmentShaderOverrides.Data, fragmentShaderOverrides.Size);
		}

		Ref<VulkanMaterial> vulkanMaterial = material.As<VulkanMaterial>();
		Renderer::Submit([renderCommandBuffer, pipeline, vulkanMaterial, vertexPushConstantBuffer, fragmentPushConstantBuffer]() mutable
		{
			BEY_PROFILE_FUNC("VulkanRenderer::SubmitFullscreenQuad");

			uint32_t frameIndex = Renderer::RT_GetCurrentFrameIndex();
			VkCommandBuffer commandBuffer = renderCommandBuffer.As<VulkanRenderCommandBuffer>()->GetActiveCommandBuffer();

			Ref<VulkanRasterPipeline> vulkanPipeline = pipeline.As<VulkanRasterPipeline>();

			VkPipelineLayout layout = vulkanPipeline->GetVulkanPipelineLayout();

			auto vulkanMeshVB = s_VulkanRendererData->QuadVertexBuffer.As<VulkanVertexBuffer>();
			VkBuffer vbMeshBuffer = vulkanMeshVB->GetVulkanBuffer();
			VkDeviceSize offsets[1] = { 0 };
			vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vbMeshBuffer, offsets);

			auto vulkanMeshIB = s_VulkanRendererData->QuadIndexBuffer.As<VulkanIndexBuffer>();
			VkBuffer ibBuffer = vulkanMeshIB->GetVulkanBuffer();
			vkCmdBindIndexBuffer(commandBuffer, ibBuffer, 0, VK_INDEX_TYPE_UINT32);

			VkDescriptorSet descriptorSet = vulkanMaterial->GetDescriptorSet(frameIndex);
			if (descriptorSet)
				vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, 1, &descriptorSet, 0, nullptr);

			if (vertexPushConstantBuffer)
				vkCmdPushConstants(commandBuffer, layout, VK_SHADER_STAGE_VERTEX_BIT, 0, (uint32_t)vertexPushConstantBuffer.Size, vertexPushConstantBuffer.Data);
			if (fragmentPushConstantBuffer)
				vkCmdPushConstants(commandBuffer, layout, VK_SHADER_STAGE_FRAGMENT_BIT, (uint32_t)vertexPushConstantBuffer.Size, (uint32_t)fragmentPushConstantBuffer.Size, fragmentPushConstantBuffer.Data);

			vkCmdDrawIndexed(commandBuffer, (uint32_t)s_VulkanRendererData->QuadIndexBuffer->GetCount(), 1, 0, 0, 0);

			vertexPushConstantBuffer.Release();
			fragmentPushConstantBuffer.Release();
		});
	}

	void VulkanRenderer::BeginFrame()
	{
		Renderer::Submit([]()
		{
			BEY_PROFILE_FUNC("VulkanRenderer::BeginFrame");

			VulkanSwapChain& swapChain = Application::Get().GetWindow().GetSwapChain();

			// Reset descriptor pools here
			VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();
			uint32_t bufferIndex = swapChain.GetCurrentBufferIndex();
			vkResetDescriptorPool(device, s_VulkanRendererData->DescriptorPools[bufferIndex], 0);
			memset(s_VulkanRendererData->DescriptorPoolAllocationCount.data(), 0, s_VulkanRendererData->DescriptorPoolAllocationCount.size() * sizeof(uint32_t));

			s_VulkanRendererData->DrawCallCount = 0;
			s_VulkanRendererData->RaytraceCount = 0;

#if 0
			VkCommandBufferBeginInfo cmdBufInfo = {};
			cmdBufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			cmdBufInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
			cmdBufInfo.pNext = nullptr;

			VkCommandBuffer drawCommandBuffer = swapChain.GetCurrentDrawCommandBuffer();
			commandBuffer = drawCommandBuffer;
			BEY_CORE_ASSERT(commandBuffer);
			VK_CHECK_RESULT(vkBeginCommandBuffer(drawCommandBuffer, &cmdBufInfo));
#endif
		});
	}

	void VulkanRenderer::EndFrame()
	{
#if 0
		Renderer::Submit([]()
		{
			VK_CHECK_RESULT(vkEndCommandBuffer(commandBuffer));
			commandBuffer = nullptr;
		});
#endif
	}

	void VulkanRenderer::InsertGPUPerfMarker(Ref<RenderCommandBuffer> renderCommandBuffer, const eastl::string& label, const glm::vec4& color)
	{
		Renderer::Submit([this, renderCommandBuffer, label, color]()
		{
			RT_InsertGPUPerfMarker(renderCommandBuffer, label, color);
		});
	}

	void VulkanRenderer::BeginGPUPerfMarker(Ref<RenderCommandBuffer> renderCommandBuffer, const eastl::string& label, const glm::vec4& markerColor)
	{
		Renderer::Submit([this, renderCommandBuffer, label, markerColor]()
		{
			RT_BeginGPUPerfMarker(renderCommandBuffer, label, markerColor);
		});
	}

	void VulkanRenderer::EndGPUPerfMarker(Ref<RenderCommandBuffer> renderCommandBuffer)
	{
		Renderer::Submit([this, renderCommandBuffer]()
		{
			RT_EndGPUPerfMarker(renderCommandBuffer);
		});
	}

	void VulkanRenderer::RT_InsertGPUPerfMarker(Ref<RenderCommandBuffer> renderCommandBuffer, const eastl::string& label, const glm::vec4& color)
	{
		const uint32_t bufferIndex = Renderer::RT_GetCurrentFrameIndex();
		VkCommandBuffer commandBuffer = renderCommandBuffer.As<VulkanRenderCommandBuffer>()->GetCommandBuffer(bufferIndex);
		VkDebugUtilsLabelEXT debugLabel{};
		debugLabel.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
		memcpy(&debugLabel.color, glm::value_ptr(color), sizeof(float) * 4);
		debugLabel.pLabelName = label.c_str();
		vkCmdInsertDebugUtilsLabelEXT(commandBuffer, &debugLabel);
	}

	void VulkanRenderer::RT_BeginGPUPerfMarker(Ref<RenderCommandBuffer> renderCommandBuffer, const eastl::string& label, const glm::vec4& markerColor)
	{
		const uint32_t bufferIndex = Renderer::RT_GetCurrentFrameIndex();
		VkCommandBuffer commandBuffer = renderCommandBuffer.As<VulkanRenderCommandBuffer>()->GetCommandBuffer(bufferIndex);
		VkDebugUtilsLabelEXT debugLabel{};
		debugLabel.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
		memcpy(&debugLabel.color, glm::value_ptr(markerColor), sizeof(float) * 4);
		debugLabel.pLabelName = label.c_str();
		vkCmdBeginDebugUtilsLabelEXT(commandBuffer, &debugLabel);
	}

	void VulkanRenderer::RT_EndGPUPerfMarker(Ref<RenderCommandBuffer> renderCommandBuffer)
	{
		const uint32_t bufferIndex = Renderer::RT_GetCurrentFrameIndex();
		VkCommandBuffer commandBuffer = renderCommandBuffer.As<VulkanRenderCommandBuffer>()->GetCommandBuffer(bufferIndex);
		vkCmdEndDebugUtilsLabelEXT(commandBuffer);
	}

	// OLD
	static void BeginRenderPassOld(Ref<RenderCommandBuffer> renderCommandBuffer, const Ref<RenderPass>& renderPass, bool explicitClear)
	{
		Renderer::Submit([renderCommandBuffer, renderPass, explicitClear]()
		{
			BEY_PROFILE_SCOPE_DYNAMIC(fmt::format("VulkanRenderer::BeginRenderPass ({})", renderPass->GetSpecification().DebugName).c_str());
			BEY_CORE_TRACE_TAG("Renderer", "BeginRenderPass - {}", renderPass->GetSpecification().DebugName);

			uint32_t frameIndex = Renderer::RT_GetCurrentFrameIndex();
			VkCommandBuffer commandBuffer = renderCommandBuffer.As<VulkanRenderCommandBuffer>()->GetActiveCommandBuffer();

			VkDebugUtilsLabelEXT debugLabel{};
			debugLabel.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
			memcpy(&debugLabel.color, glm::value_ptr(renderPass->GetSpecification().MarkerColor), sizeof(float) * 4);
			debugLabel.pLabelName = renderPass->GetSpecification().DebugName.c_str();
			vkCmdBeginDebugUtilsLabelEXT(commandBuffer, &debugLabel);

			// NOTE: now stored in pipeline (and RenderPass spec contains pipeline)
			// auto fb = renderPass->GetSpecification().TargetFramebuffer;
			Ref<VulkanFramebuffer> framebuffer;// = fb.As<VulkanFramebuffer>();
			const auto& fbSpec = framebuffer->GetSpecification();

			uint32_t width = framebuffer->GetWidth();
			uint32_t height = framebuffer->GetHeight();

			VkViewport viewport = {};
			viewport.minDepth = 0.0f;
			viewport.maxDepth = 1.0f;

			VkRenderPassBeginInfo renderPassBeginInfo = {};
			renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			renderPassBeginInfo.pNext = nullptr;
			renderPassBeginInfo.renderPass = framebuffer->GetRenderPass();
			renderPassBeginInfo.renderArea.offset.x = 0;
			renderPassBeginInfo.renderArea.offset.y = 0;
			renderPassBeginInfo.renderArea.extent.width = width;
			renderPassBeginInfo.renderArea.extent.height = height;
			if (framebuffer->GetSpecification().SwapChainTarget)
			{
				VulkanSwapChain& swapChain = Application::Get().GetWindow().GetSwapChain();
				width = swapChain.GetWidth();
				height = swapChain.GetHeight();
				renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
				renderPassBeginInfo.pNext = nullptr;
				renderPassBeginInfo.renderPass = framebuffer->GetRenderPass();
				renderPassBeginInfo.renderArea.offset.x = 0;
				renderPassBeginInfo.renderArea.offset.y = 0;
				renderPassBeginInfo.renderArea.extent.width = width;
				renderPassBeginInfo.renderArea.extent.height = height;
				renderPassBeginInfo.framebuffer = swapChain.GetCurrentFramebuffer();

				viewport.x = 0.0f;
				viewport.y = (float)height;
				viewport.width = (float)width;
				viewport.height = -(float)height;
			}
			else
			{
				width = framebuffer->GetWidth();
				height = framebuffer->GetHeight();
				renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
				renderPassBeginInfo.pNext = nullptr;
				renderPassBeginInfo.renderPass = framebuffer->GetRenderPass();
				renderPassBeginInfo.renderArea.offset.x = 0;
				renderPassBeginInfo.renderArea.offset.y = 0;
				renderPassBeginInfo.renderArea.extent.width = width;
				renderPassBeginInfo.renderArea.extent.height = height;
				renderPassBeginInfo.framebuffer = framebuffer->GetVulkanFramebuffer();

				viewport.x = 0.0f;
				viewport.y = 0.0f;
				viewport.width = (float)width;
				viewport.height = (float)height;
			}

			// TODO: Does our framebuffer have a depth attachment?
			const auto& clearValues = framebuffer->GetVulkanClearValues();
			renderPassBeginInfo.clearValueCount = (uint32_t)clearValues.size();
			renderPassBeginInfo.pClearValues = clearValues.data();

			vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

			if (explicitClear)
			{
				const uint32_t colorAttachmentCount = (uint32_t)framebuffer->GetColorAttachmentCount();
				const uint32_t totalAttachmentCount = colorAttachmentCount + (framebuffer->HasDepthAttachment() ? 1 : 0);
				BEY_CORE_ASSERT(clearValues.size() == totalAttachmentCount);

				std::vector<VkClearAttachment> attachments(totalAttachmentCount);
				std::vector<VkClearRect> clearRects(totalAttachmentCount);
				for (uint32_t i = 0; i < colorAttachmentCount; i++)
				{
					attachments[i].aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
					attachments[i].colorAttachment = i;
					attachments[i].clearValue = clearValues[i];

					clearRects[i].rect.offset = { (int32_t)0, (int32_t)0 };
					clearRects[i].rect.extent = { width, height };
					clearRects[i].baseArrayLayer = 0;
					clearRects[i].layerCount = 1;
				}

				if (framebuffer->HasDepthAttachment())
				{
					attachments[colorAttachmentCount].aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
					attachments[colorAttachmentCount].clearValue = clearValues[colorAttachmentCount];
					clearRects[colorAttachmentCount].rect.offset = { (int32_t)0, (int32_t)0 };
					clearRects[colorAttachmentCount].rect.extent = { width, height };
					clearRects[colorAttachmentCount].baseArrayLayer = 0;
					clearRects[colorAttachmentCount].layerCount = 1;
				}

				vkCmdClearAttachments(commandBuffer, totalAttachmentCount, attachments.data(), totalAttachmentCount, clearRects.data());

			}

			// Update dynamic viewport state
			vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

			// Update dynamic scissor state
			VkRect2D scissor = {};
			scissor.extent.width = width;
			scissor.extent.height = height;
			scissor.offset.x = 0;
			scissor.offset.y = 0;
			vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
		});
	}

	void VulkanRenderer::BeginRenderPass(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<RenderPass> renderPass, bool explicitClear)
	{
		Renderer::Submit([renderCommandBuffer, renderPass, explicitClear]()
		{
			BEY_PROFILE_SCOPE_DYNAMIC(fmt::format("VulkanRenderer::BeginRenderPass ({})", renderPass->GetSpecification().DebugName).c_str());
			BEY_CORE_TRACE_TAG("Renderer", "BeginRenderPass - {}", renderPass->GetSpecification().DebugName);


			uint32_t frameIndex = Renderer::RT_GetCurrentFrameIndex();
			VkCommandBuffer commandBuffer = renderCommandBuffer.As<VulkanRenderCommandBuffer>()->GetActiveCommandBuffer();

			VkDebugUtilsLabelEXT debugLabel{};
			debugLabel.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
			memcpy(&debugLabel.color, glm::value_ptr(renderPass->GetSpecification().MarkerColor), sizeof(float) * 4);
			debugLabel.pLabelName = renderPass->GetSpecification().DebugName.c_str();
			vkCmdBeginDebugUtilsLabelEXT(commandBuffer, &debugLabel);

			auto fb = renderPass->GetSpecification().Pipeline->GetSpecification().TargetFramebuffer;
			Ref<VulkanFramebuffer> framebuffer = fb.As<VulkanFramebuffer>();

			uint32_t width = framebuffer->GetWidth();
			uint32_t height = framebuffer->GetHeight();

			VkViewport viewport = {};
			viewport.minDepth = 0.0f;
			viewport.maxDepth = 1.0f;

			VkRenderPassBeginInfo renderPassBeginInfo = {};
			renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			renderPassBeginInfo.pNext = nullptr;
			renderPassBeginInfo.renderPass = framebuffer->GetRenderPass();
			renderPassBeginInfo.renderArea.offset.x = 0;
			renderPassBeginInfo.renderArea.offset.y = 0;
			renderPassBeginInfo.renderArea.extent.width = width;
			renderPassBeginInfo.renderArea.extent.height = height;
			if (framebuffer->GetSpecification().SwapChainTarget)
			{
				VulkanSwapChain& swapChain = Application::Get().GetWindow().GetSwapChain();
				width = swapChain.GetWidth();
				height = swapChain.GetHeight();
				renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
				renderPassBeginInfo.pNext = nullptr;
				renderPassBeginInfo.renderPass = framebuffer->GetRenderPass();
				renderPassBeginInfo.renderArea.offset.x = 0;
				renderPassBeginInfo.renderArea.offset.y = 0;
				renderPassBeginInfo.renderArea.extent.width = width;
				renderPassBeginInfo.renderArea.extent.height = height;
				renderPassBeginInfo.framebuffer = swapChain.GetCurrentFramebuffer();

				viewport.x = 0.0f;
				viewport.y = (float)height;
				viewport.width = (float)width;
				viewport.height = -(float)height;
			}
			else
			{
				width = framebuffer->GetWidth();
				height = framebuffer->GetHeight();
				renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
				renderPassBeginInfo.pNext = nullptr;
				renderPassBeginInfo.renderPass = framebuffer->GetRenderPass();
				renderPassBeginInfo.renderArea.offset.x = 0;
				renderPassBeginInfo.renderArea.offset.y = 0;
				renderPassBeginInfo.renderArea.extent.width = width;
				renderPassBeginInfo.renderArea.extent.height = height;
				renderPassBeginInfo.framebuffer = framebuffer->GetVulkanFramebuffer();

				viewport.x = 0.0f;
				viewport.y = 0.0f;
				viewport.width = (float)width;
				viewport.height = (float)height;
			}

			// TODO: Does our framebuffer have a depth attachment?
			const auto& clearValues = framebuffer->GetVulkanClearValues();
			renderPassBeginInfo.clearValueCount = (uint32_t)clearValues.size();
			renderPassBeginInfo.pClearValues = clearValues.data();

			vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

			if (explicitClear)
			{
				const uint32_t colorAttachmentCount = (uint32_t)framebuffer->GetColorAttachmentCount();
				const uint32_t totalAttachmentCount = colorAttachmentCount + (framebuffer->HasDepthAttachment() ? 1 : 0);
				BEY_CORE_ASSERT(clearValues.size() == totalAttachmentCount);

				std::vector<VkClearAttachment> attachments(totalAttachmentCount);
				std::vector<VkClearRect> clearRects(totalAttachmentCount);
				for (uint32_t i = 0; i < colorAttachmentCount; i++)
				{
					attachments[i].aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
					attachments[i].colorAttachment = i;
					attachments[i].clearValue = clearValues[i];

					clearRects[i].rect.offset = { (int32_t)0, (int32_t)0 };
					clearRects[i].rect.extent = { width, height };
					clearRects[i].baseArrayLayer = 0;
					clearRects[i].layerCount = 1;
				}

				if (framebuffer->HasDepthAttachment())
				{
					attachments[colorAttachmentCount].aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT /*| VK_IMAGE_ASPECT_STENCIL_BIT*/;
					attachments[colorAttachmentCount].clearValue = clearValues[colorAttachmentCount];
					clearRects[colorAttachmentCount].rect.offset = { (int32_t)0, (int32_t)0 };
					clearRects[colorAttachmentCount].rect.extent = { width, height };
					clearRects[colorAttachmentCount].baseArrayLayer = 0;
					clearRects[colorAttachmentCount].layerCount = 1;
				}

				vkCmdClearAttachments(commandBuffer, totalAttachmentCount, attachments.data(), totalAttachmentCount, clearRects.data());

			}

			// Update dynamic viewport state
			vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

			// Update dynamic scissor state
			VkRect2D scissor = {};
			scissor.extent.width = width;
			scissor.extent.height = height;
			scissor.offset.x = 0;
			scissor.offset.y = 0;
			vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

			// TODO: automatic layout transitions for input resources

			// Bind Vulkan Pipeline
			Ref<VulkanRasterPipeline> vulkanPipeline = renderPass->GetSpecification().Pipeline.As<VulkanRasterPipeline>();
			VkPipeline vPipeline = vulkanPipeline->GetVulkanPipeline();
			vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vPipeline);

			if (vulkanPipeline->IsDynamicLineWidth())
				vkCmdSetLineWidth(commandBuffer, vulkanPipeline->GetSpecification().LineWidth);

			// Bind input descriptors (starting from set 1, set 0 is for per-draw)
			Ref<VulkanRenderPass> vulkanRenderPass = renderPass.As<VulkanRenderPass>();
			vulkanRenderPass->Prepare();
			if (vulkanRenderPass->HasDescriptorSets())
			{
				const auto& descriptorSets = vulkanRenderPass->GetDescriptorSets(frameIndex);
				vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vulkanPipeline->GetVulkanPipelineLayout(), vulkanRenderPass->GetFirstSetIndex(), (uint32_t)descriptorSets.size(), descriptorSets.data(), 0, nullptr);
			}

			if (s_VulkanRendererData->BindlessDescriptorSetManager.HasDescriptorSets(renderPass->GetPipeline()->GetShader()))
			{
				const auto& descriptorSets = s_VulkanRendererData->BindlessDescriptorSetManager.GetDescriptorSets(vulkanRenderPass->GetPipeline()->GetShader(), frameIndex);
				vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vulkanPipeline->GetVulkanPipelineLayout(), s_VulkanRendererData->BindlessDescriptorSetManager.GetFirstSetIndex(vulkanPipeline->GetShader()->GetRootSignature()), (uint32_t)descriptorSets.size(), descriptorSets.data(), 0, nullptr);
			}
		});
	}

	void VulkanRenderer::EndRenderPass(Ref<RenderCommandBuffer> renderCommandBuffer)
	{
		Renderer::Submit([renderCommandBuffer]()
		{
			BEY_PROFILE_FUNC("VulkanRenderer::EndRenderPass");

			uint32_t frameIndex = Renderer::RT_GetCurrentFrameIndex();
			VkCommandBuffer commandBuffer = renderCommandBuffer.As<VulkanRenderCommandBuffer>()->GetActiveCommandBuffer();

			vkCmdEndRenderPass(commandBuffer);
			vkCmdEndDebugUtilsLabelEXT(commandBuffer);
		});
	}

	void VulkanRenderer::SubmitBarrier(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<Image2D> image, const RendererUtils::ImageBarrier& barrier)
	{
		Utils::InsertImageMemoryBarrier(renderCommandBuffer.As<VulkanRenderCommandBuffer>()->GetActiveCommandBuffer(), image.As<VulkanImage2D>()->GetImageInfo().Image, barrier.srcAccessMask, barrier.dstAccessMask, std::bit_cast<VkImageLayout>(barrier.oldImageLayout), std::bit_cast<VkImageLayout>(barrier.newImageLayout), barrier.srcStageMask, barrier.dstStageMask, std::bit_cast<VkImageSubresourceRange>(barrier.subresourceRange));
	}

	void VulkanRenderer::BeginComputePass(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<ComputePass> computePass)
	{
		Renderer::Submit([renderCommandBuffer, computePass]() mutable
		{
			Ref<VulkanComputePass> vulkanComputePass = computePass.As<VulkanComputePass>();
			Ref<VulkanComputePipeline> pipeline = computePass->GetSpecification().Pipeline.As<VulkanComputePipeline>();

			const uint32_t frameIndex = Renderer::RT_GetCurrentFrameIndex();
			VkCommandBuffer commandBuffer = renderCommandBuffer.As<VulkanRenderCommandBuffer>()->GetCommandBuffer(frameIndex);

			VkDebugUtilsLabelEXT debugLabel{};
			debugLabel.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
			memcpy(&debugLabel.color, glm::value_ptr(vulkanComputePass->GetSpecification().MarkerColor), sizeof(float) * 4);
			debugLabel.pLabelName = vulkanComputePass->GetSpecification().DebugName.c_str();
			vkCmdBeginDebugUtilsLabelEXT(commandBuffer, &debugLabel);

			pipeline->RT_Begin(renderCommandBuffer); // bind pipeline

			// Bind compute pass descriptor set(s)
			vulkanComputePass->Prepare();
			if (vulkanComputePass->HasDescriptorSets())
			{
				const auto& descriptorSets = vulkanComputePass->GetDescriptorSets(frameIndex);
				vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline->GetLayout(), vulkanComputePass->GetFirstSetIndex(), (uint32_t)descriptorSets.size(), descriptorSets.data(), 0, 0);
			}

			if (s_VulkanRendererData->BindlessDescriptorSetManager.HasDescriptorSets(vulkanComputePass->GetPipeline()->GetShader()))
			{
				const auto& descriptorSets = s_VulkanRendererData->BindlessDescriptorSetManager.GetDescriptorSets(vulkanComputePass->GetShader(), frameIndex);
				vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline->GetLayout(), s_VulkanRendererData->BindlessDescriptorSetManager.GetFirstSetIndex(vulkanComputePass->GetShader()->GetRootSignature()), (uint32_t)descriptorSets.size(), descriptorSets.data(), 0, nullptr);
			}
		});
	}

	void VulkanRenderer::EndComputePass(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<ComputePass> computePass)
	{
		Renderer::Submit([renderCommandBuffer, computePass]() mutable
		{
			Ref<VulkanComputePass> vulkanComputePass = computePass.As<VulkanComputePass>();
			Ref<VulkanComputePipeline> pipeline = computePass->GetSpecification().Pipeline.As<VulkanComputePipeline>();
			pipeline->End();
			vkCmdEndDebugUtilsLabelEXT(renderCommandBuffer.As<VulkanRenderCommandBuffer>()->GetActiveCommandBuffer());
		});
	}

	void VulkanRenderer::DispatchCompute(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<ComputePass> computePass, Ref<Material> material, const glm::uvec3& workGroups, Buffer constants)
	{
		Buffer constantsBuffer;
		if (constants)
			constantsBuffer = Buffer::Copy(constants);

		Renderer::Submit([renderCommandBuffer, computePass, material, workGroups, constantsBuffer]() mutable
		{
			Ref<VulkanComputePass> vulkanComputePass = computePass.As<VulkanComputePass>();
			Ref<VulkanComputePipeline> pipeline = computePass->GetSpecification().Pipeline.As<VulkanComputePipeline>();

			const uint32_t frameIndex = Renderer::RT_GetCurrentFrameIndex();
			VkCommandBuffer commandBuffer = renderCommandBuffer.As<VulkanRenderCommandBuffer>()->GetCommandBuffer(frameIndex);

			// Bind material descriptor set if exists
			if (material)
			{
				Ref<VulkanMaterial> vulkanMaterial = material.As<VulkanMaterial>();
				VkDescriptorSet descriptorSet = vulkanMaterial->GetDescriptorSet(frameIndex);
				if (descriptorSet)
					vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline->GetLayout(), 0, 1, &descriptorSet, 0, nullptr);
			}

			if (constantsBuffer)
			{
				pipeline->SetPushConstants(constantsBuffer);
				constantsBuffer.Release();
			}

			SET_VULKAN_CHECKPOINT(commandBuffer, fmt::eastl_format("VulkanComputePipeline::Execute({})", computePass->GetShader()->GetName()));
			pipeline->Dispatch(workGroups);
			SET_VULKAN_CHECKPOINT(commandBuffer, fmt::eastl_format("VulkanComputePipeline::Execute After Dispatch({})", computePass->GetShader()->GetName()));
		});
	}

	void VulkanRenderer::SetDDGITextureResources()
	{
		const auto& volumes = s_VulkanRendererData->Volumes;


		// 9: Texture2DArray UAVs
		{
			std::vector<std::tuple<VkDescriptorImageInfo, VkImage, eastl::string>> rwTex2DArray;
			for (auto& volume : volumes)
			{
				// Add the DDGIVolume texture arrays
				rwTex2DArray.push_back({ { VK_NULL_HANDLE, volume->GetProbeRayDataView(), VK_IMAGE_LAYOUT_GENERAL }, volume->GetProbeRayData(), "ProbeRayData" });
				rwTex2DArray.push_back({ { VK_NULL_HANDLE, volume->GetProbeIrradianceView(), VK_IMAGE_LAYOUT_GENERAL }, volume->GetProbeIrradiance(), "ProbeIrradiance" });
				rwTex2DArray.push_back({ { VK_NULL_HANDLE, volume->GetProbeDistanceView(), VK_IMAGE_LAYOUT_GENERAL }, volume->GetProbeDistance(), "ProbeDistance" });
				rwTex2DArray.push_back({ { VK_NULL_HANDLE, volume->GetProbeDataView(), VK_IMAGE_LAYOUT_GENERAL }, volume->GetProbeData(), "ProbeData" });
				rwTex2DArray.push_back({ { VK_NULL_HANDLE, volume->GetProbeVariabilityView(), VK_IMAGE_LAYOUT_GENERAL }, volume->GetProbeVariability(), "ProbeVariability" });
				rwTex2DArray.push_back({ { VK_NULL_HANDLE, volume->GetProbeVariabilityAverageView(), VK_IMAGE_LAYOUT_GENERAL }, volume->GetProbeVariabilityAverage(), "ProbeVariabilityAverage" });
			}

			ImageViewSpecification spec{};
			spec.IsRWImage = true;
			spec.CreateBindlessDescriptor = true;

			uint32_t index = 0;
			RenderPassInput input;
			for (auto& [info, vkImage, name] : rwTex2DArray)
			{
				spec.DebugName = name;
				Ref<ImageView> image = Ref<VulkanImageView>::Create(spec, info, vkImage);

				input.Input[index++] = (image);
				input.Type = RenderPassResourceType::Image2D; // Should be Image2D
				input.Name = "RWTex2DArray";
			}
			Renderer::AddBindlessDescriptor(std::move(input));
		}

		// 12: Texture2DArray SRVs
		{
			std::vector<std::tuple<VkDescriptorImageInfo, VkImage, eastl::string>> tex2DArray;
			for (auto& volume : volumes)
			{
				// Add the DDGIVolume texture arrays
				tex2DArray.push_back({ { VK_NULL_HANDLE, volume->GetProbeRayDataView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL }, volume->GetProbeRayData(), "ProbeRayData" });
				tex2DArray.push_back({ { VK_NULL_HANDLE, volume->GetProbeIrradianceView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL }, volume->GetProbeIrradiance(), "ProbeIrradiance" });
				tex2DArray.push_back({ { VK_NULL_HANDLE, volume->GetProbeDistanceView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL }, volume->GetProbeDistance(), "ProbeDistance" });
				tex2DArray.push_back({ { VK_NULL_HANDLE, volume->GetProbeDataView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL }, volume->GetProbeData(), "ProbeData" });
				tex2DArray.push_back({ { VK_NULL_HANDLE, volume->GetProbeVariabilityView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL }, volume->GetProbeVariability(), "ProbeVariability" });
				tex2DArray.push_back({ { VK_NULL_HANDLE, volume->GetProbeVariabilityAverageView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL }, volume->GetProbeVariabilityAverage(), "ProbeVariabilityAverage" });
			}

			ImageViewSpecification spec{};
			spec.CreateBindlessDescriptor = true;

			uint32_t index = 0;
			RenderPassInput input;
			for (auto& [info, vkImage, name] : tex2DArray)
			{
				spec.DebugName = name;
				Ref<ImageView> image = Ref<VulkanImageView>::Create(spec, info, vkImage);

				input.Input[index++] = (image);
				input.Type = RenderPassResourceType::Texture2D; // Should be Image2D
				input.Name = "Tex2DArray";
			}
			Renderer::AddBindlessDescriptor(std::move(input));

			Renderer::UpdateBindlessDescriptorSet(false);
		}
	}

	std::vector<std::unique_ptr<rtxgi::vulkan::DDGIVolume>>& VulkanRenderer::GetDDGIVolumes()
	{
		return s_VulkanRendererData->Volumes;
	}

	//rtxgi::vulkan::DDGIVolumeResources& VulkanRenderer::GetVulkanDDGIResources()
	//{
		//return s_VulkanRendererData->DDGIResources;
	//}

	void VulkanRenderer::SetDDGIResources(Ref<StorageBuffer> constantBuffer, Ref<StorageBuffer> indicesBuffer)
	{
		Renderer::Submit([&, constantBuffer, indicesBuffer]() mutable
		{
			Ref<VulkanDevice> device = VulkanContext::GetCurrentDevice();
			auto& resources = s_VulkanRendererData->DDGIResources;
			auto& volumes = s_VulkanRendererData->Volumes;

			resources.constantsBuffer = constantBuffer.As<VulkanStorageBuffer>()->GetVulkanBuffer();
			resources.constantsBufferUpload = constantBuffer.As<VulkanStorageBuffer>()->GetUploadBuffer();
			resources.constantsBufferSizeInBytes = constantBuffer.As<VulkanStorageBuffer>()->GetSize() / Renderer::GetConfig().FramesInFlight;
			resources.constantsBufferUploadMemory = constantBuffer.As<VulkanStorageBuffer>()->GetUploadMemory();

			for (auto& volume : volumes)
			{
				volume->SetConstantsBuffer(constantBuffer.As<VulkanStorageBuffer>()->GetVulkanBuffer());
				volume->SetConstantsBufferSizeInBytes(constantBuffer.As<VulkanStorageBuffer>()->GetSize() / Renderer::GetConfig().FramesInFlight);
				volume->SetConstantsBufferUpload(constantBuffer.As<VulkanStorageBuffer>()->GetUploadBuffer());
				volume->SetConstantsBufferUploadMemory(constantBuffer.As<VulkanStorageBuffer>()->GetUploadMemory());

				volume->SetResourceIndicesBuffer(indicesBuffer.As<VulkanStorageBuffer>()->GetVulkanBuffer());
				volume->SetResourceIndicesBufferSizeInBytes(indicesBuffer.As<VulkanStorageBuffer>()->GetSize());
				volume->SetResourceIndicesBufferUpload(indicesBuffer.As<VulkanStorageBuffer>()->GetUploadBuffer());
				volume->SetResourceIndicesBufferUploadMemory(indicesBuffer.As<VulkanStorageBuffer>()->GetUploadMemory());

				volume->GetDesc().ClearProbes = true;
			}

		}
		);
	}

	void VulkanRenderer::UpdateDDGIData(Ref<RenderCommandBuffer> commandBuffer)
	{

		Renderer::Submit([=]() mutable
		{
			if (!s_VulkanRendererData->Volumes.empty())
			{
				const VkCommandBuffer vkCommandBuffer = commandBuffer.As<VulkanRenderCommandBuffer>()->GetCommandBuffer(Renderer::RT_GetCurrentFrameIndex());

				std::vector<rtxgi::vulkan::DDGIVolume*> volumes;
				for (const auto& volume : s_VulkanRendererData->Volumes)
				{
					volumes.emplace_back(volume.get());
					volume->Update();
				}

				rtxgi::ERTXGIStatus res = rtxgi::OK;
				res = rtxgi::vulkan::UpdateDDGIVolumeProbes(vkCommandBuffer, (uint32_t)s_VulkanRendererData->Volumes.size(), volumes.data());
				BEY_CORE_VERIFY(res == rtxgi::OK);
				res = rtxgi::vulkan::RelocateDDGIVolumeProbes(vkCommandBuffer, (uint32_t)s_VulkanRendererData->Volumes.size(), volumes.data());
				BEY_CORE_VERIFY(res == rtxgi::OK);
				res = rtxgi::vulkan::ClassifyDDGIVolumeProbes(vkCommandBuffer, (uint32_t)s_VulkanRendererData->Volumes.size(), volumes.data());
				BEY_CORE_VERIFY(res == rtxgi::OK);
				res = rtxgi::vulkan::CalculateDDGIVolumeVariability(vkCommandBuffer, (uint32_t)s_VulkanRendererData->Volumes.size(), volumes.data());
				BEY_CORE_VERIFY(res == rtxgi::OK);
			}
		});
	}

	void VulkanRenderer::InitDDGI(Ref<RenderCommandBuffer> commandBuffer, const std::vector<rtxgi::DDGIVolumeDesc>& ddgiVolumeDescs)
	{
		if (ddgiVolumeDescs.empty())
			return;

		Renderer::Submit([commandBuffer = commandBuffer.As<VulkanRenderCommandBuffer>(), ddgiVolumeDescs, this]() mutable
		{

			const VkCommandBuffer vkCommandBuffer = commandBuffer->GetCommandBuffer(Renderer::RT_GetCurrentFrameIndex());

			auto& volumes = s_VulkanRendererData->Volumes;
			if (volumes.size() != ddgiVolumeDescs.size())
			{
				for (auto& volume : s_VulkanRendererData->Volumes)
					volume->Destroy();
				volumes.clear();

				for (const auto& volumeDesc : ddgiVolumeDescs)
				{
					auto& volume = volumes.emplace_back(std::make_unique<rtxgi::vulkan::DDGIVolume>());

#if RTXGI_DDGI_RESOURCE_MANAGEMENT
					rtxgi::ERTXGIStatus status = volume->Create(vkCommandBuffer, false, volumeDesc, s_VulkanRendererData->DDGIResources);
#else
					rtxgi::ERTXGIStatus status = volume->Create(volumeDesc, s_VulkanRendererData->DDGIResources);
#endif
					BEY_CORE_VERIFY(status == rtxgi::OK);


					status = volume->ClearProbes(vkCommandBuffer);
					BEY_CORE_VERIFY(status == rtxgi::ERTXGIStatus::OK, "Failed to create the DDGIVolume!");

					SetDDGITextureResources();
				}
			}
			else
			{
				for (uint32_t i = 0; const auto & desc : ddgiVolumeDescs)
				{
					volumes[i]->SetDesc(desc);
					i++;
				}
				SetDDGITextureResources();

				for (uint32_t i = 0; const auto & desc : ddgiVolumeDescs)
				{
					auto& volume = s_VulkanRendererData->Volumes[i];

					const bool updatedShaders = Renderer::UpdatedShaders();
					if (desc.ClearProbes || updatedShaders)
					{
						rtxgi::ERTXGIStatus status = rtxgi::OK;
						volume->Destroy();
						if (updatedShaders)
						{
							const auto& probeBlendingDistance = Renderer::GetShaderLibrary()->Get("ProbeBlendingDistanceCS").As<VulkanShader>()->GetSpirvData().at(VK_SHADER_STAGE_COMPUTE_BIT);
							const auto& probeBlendingIrradiance = Renderer::GetShaderLibrary()->Get("ProbeBlendingIrradianceCS").As<VulkanShader>()->GetSpirvData().at(VK_SHADER_STAGE_COMPUTE_BIT);

							const auto& probeClassificationUpdate = Renderer::GetShaderLibrary()->Get("ProbeClassificationCS", 0).As<VulkanShader>()->GetSpirvData().at(VK_SHADER_STAGE_COMPUTE_BIT);
							const auto& probeClassificationReset = Renderer::GetShaderLibrary()->Get("ProbeClassificationCS", 1).As<VulkanShader>()->GetSpirvData().at(VK_SHADER_STAGE_COMPUTE_BIT);

							const auto& probeRelocationUpdate = Renderer::GetShaderLibrary()->Get("ProbeRelocationCS", 0).As<VulkanShader>()->GetSpirvData().at(VK_SHADER_STAGE_COMPUTE_BIT);
							const auto& probeRelocationReset = Renderer::GetShaderLibrary()->Get("ProbeRelocationCS", 1).As<VulkanShader>()->GetSpirvData().at(VK_SHADER_STAGE_COMPUTE_BIT);

							const auto& probeReductionUpdate = Renderer::GetShaderLibrary()->Get("ReductionCS", 0).As<VulkanShader>()->GetSpirvData().at(VK_SHADER_STAGE_COMPUTE_BIT);
							const auto& probeReductionReset = Renderer::GetShaderLibrary()->Get("ReductionCS", 1).As<VulkanShader>()->GetSpirvData().at(VK_SHADER_STAGE_COMPUTE_BIT);

							auto& ddgi = s_VulkanRendererData->DDGIResources;
							ddgi.managed.probeBlendingIrradianceCS = { probeBlendingIrradiance.data(), probeBlendingIrradiance.size() * sizeof(uint32_t) };
							ddgi.managed.probeBlendingDistanceCS = { probeBlendingDistance.data(), probeBlendingDistance.size() * sizeof(uint32_t) };
							ddgi.managed.probeRelocation = { {probeRelocationUpdate.data(), probeRelocationUpdate.size() * sizeof(uint32_t)}, {probeRelocationReset.data(), probeRelocationReset.size() * sizeof(uint32_t)} };
							ddgi.managed.probeClassification = { {probeClassificationUpdate.data(), probeClassificationUpdate.size() * sizeof(uint32_t)}, {probeClassificationReset.data(), probeClassificationReset.size() * sizeof(uint32_t)} };
							ddgi.managed.probeVariability = { {probeReductionUpdate.data(), probeReductionUpdate.size() * sizeof(uint32_t)}, {probeReductionReset.data(), probeReductionReset.size() * sizeof(uint32_t)} };
						}
						status = volumes[i]->Create(vkCommandBuffer, updatedShaders, desc, s_VulkanRendererData->DDGIResources);
						BEY_CORE_VERIFY(status == rtxgi::OK);

						SetDDGITextureResources();

						status = volume->ClearProbes(vkCommandBuffer);
						BEY_CORE_VERIFY(status == rtxgi::OK);
					}

					i++;
				}
			}

		});
	}

	void VulkanRenderer::SetDDGIStorage(Ref<StorageBuffer> constantsBuffer, Ref<StorageBuffer> resourceIndices)
	{
		Renderer::Submit([constantsBuffer, resourceIndices, this]() mutable
		{
			const auto& volumes = s_VulkanRendererData->Volumes;
			for (uint32_t volumeIndex = 0; volumeIndex < volumes.size(); volumeIndex++)
			{
				// Get the volume
				const auto& volume = volumes[volumeIndex];
				{
					// Offset to the constants data to write to (e.g. double buffering)
					uint32_t bufferOffset = (uint32_t)volume->GetConstantsBufferSizeInBytes() * Renderer::RT_GetCurrentFrameIndex();

					// Offset to the volume in current constants buffer
					uint32_t volumeOffset = (volume->GetIndex() * (uint32_t)sizeof(rtxgi::DDGIVolumeDescGPUPacked));

					// Offset to the volume constants in the upload buffer
					uint32_t srcOffset = (bufferOffset + volumeOffset);


					// Get the packed DDGIVolume GPU descriptor
					const rtxgi::DDGIVolumeDescGPUPacked gpuDesc = volume->GetDescGPUPacked();

#if _DEBUG
					volume->ValidatePackedData(gpuDesc);
#endif

					constantsBuffer->RT_SetData(&gpuDesc, sizeof(rtxgi::DDGIVolumeDescGPUPacked), srcOffset);
				}
				{
					rtxgi::DDGIVolumeResourceIndices gpuDescIndices = volume->GetResourceIndices();
					gpuDescIndices.rayDataUAVIndex = (volume->GetDesc().index * rtxgi::GetDDGIVolumeNumTex2DArrayDescriptors());
					gpuDescIndices.rayDataSRVIndex = (volume->GetDesc().index * rtxgi::GetDDGIVolumeNumTex2DArrayDescriptors());
					gpuDescIndices.probeIrradianceUAVIndex = (volume->GetDesc().index * rtxgi::GetDDGIVolumeNumTex2DArrayDescriptors()) + 1;
					gpuDescIndices.probeIrradianceSRVIndex = (volume->GetDesc().index * rtxgi::GetDDGIVolumeNumTex2DArrayDescriptors()) + 1;
					gpuDescIndices.probeDistanceUAVIndex = (volume->GetDesc().index * rtxgi::GetDDGIVolumeNumTex2DArrayDescriptors()) + 2;
					gpuDescIndices.probeDistanceSRVIndex = (volume->GetDesc().index * rtxgi::GetDDGIVolumeNumTex2DArrayDescriptors()) + 2;
					gpuDescIndices.probeDataUAVIndex = (volume->GetDesc().index * rtxgi::GetDDGIVolumeNumTex2DArrayDescriptors()) + 3;
					gpuDescIndices.probeDataSRVIndex = (volume->GetDesc().index * rtxgi::GetDDGIVolumeNumTex2DArrayDescriptors()) + 3;
					gpuDescIndices.probeVariabilityUAVIndex = (volume->GetDesc().index * rtxgi::GetDDGIVolumeNumTex2DArrayDescriptors()) + 4;
					gpuDescIndices.probeVariabilitySRVIndex = (volume->GetDesc().index * rtxgi::GetDDGIVolumeNumTex2DArrayDescriptors()) + 4;
					gpuDescIndices.probeVariabilityAverageUAVIndex = (volume->GetDesc().index * rtxgi::GetDDGIVolumeNumTex2DArrayDescriptors()) + 5;
					gpuDescIndices.probeVariabilityAverageSRVIndex = (volume->GetDesc().index * rtxgi::GetDDGIVolumeNumTex2DArrayDescriptors()) + 5;


					// Offset to the resource indices data to write to (e.g. double buffering)
					uint32_t bufferOffset = (uint32_t)sizeof(rtxgi::DDGIVolumeResourceIndices) * Renderer::RT_GetCurrentFrameIndex();

					// Offset to the volume in current resource indices buffer
					uint32_t volumeOffset = (volume->GetIndex() * (uint32_t)sizeof(rtxgi::DDGIVolumeResourceIndices));

					// Offset to the volume resource indices in the upload buffer
					uint32_t srcOffset = (bufferOffset + volumeOffset);

					resourceIndices->RT_SetData(&gpuDescIndices, sizeof(rtxgi::DDGIVolumeResourceIndices), srcOffset);

				}
			}
		});
	}

	void VulkanRenderer::BeginRaytracingPass(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<RaytracingPass> raytracingPass)
	{
		Renderer::Submit([renderCommandBuffer, raytracingPass]() mutable
		{
			Ref<VulkanRaytracingPass> vulkanRaytracingPass = raytracingPass.As<VulkanRaytracingPass>();
			Ref<VulkanRaytracingPipeline> pipeline = raytracingPass->GetSpecification().Pipeline.As<VulkanRaytracingPipeline>();

			const uint32_t frameIndex = Renderer::RT_GetCurrentFrameIndex();
			VkCommandBuffer commandBuffer = renderCommandBuffer.As<VulkanRenderCommandBuffer>()->GetCommandBuffer(frameIndex);

			VkDebugUtilsLabelEXT debugLabel{};
			debugLabel.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
			memcpy(&debugLabel.color, glm::value_ptr(vulkanRaytracingPass->GetSpecification().MarkerColor), sizeof(float) * 4);
			debugLabel.pLabelName = vulkanRaytracingPass->GetSpecification().DebugName.c_str();
			vkCmdBeginDebugUtilsLabelEXT(commandBuffer, &debugLabel);

			pipeline->RT_Begin(commandBuffer); // bind pipeline

			// Bind ray tracing pass descriptor set(s)
			vulkanRaytracingPass->Prepare();
			if (vulkanRaytracingPass->HasDescriptorSets())
			{
				const auto& descriptorSets = vulkanRaytracingPass->GetDescriptorSets(frameIndex);
				vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipeline->GetLayout(), vulkanRaytracingPass->GetFirstSetIndex(), (uint32_t)descriptorSets.size(), descriptorSets.data(), 0, 0);
			}


			if (s_VulkanRendererData->BindlessDescriptorSetManager.HasDescriptorSets(vulkanRaytracingPass->GetPipeline()->GetShader()))
			{
				const auto& descriptorSets = s_VulkanRendererData->BindlessDescriptorSetManager.GetDescriptorSets(vulkanRaytracingPass->GetPipeline()->GetShader(), frameIndex);
				vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipeline->GetLayout(), s_VulkanRendererData->BindlessDescriptorSetManager.GetFirstSetIndex(vulkanRaytracingPass->GetShader()->GetRootSignature()), (uint32_t)descriptorSets.size(), descriptorSets.data(), 0, nullptr);
			}
		});
	}

	void VulkanRenderer::EndRaytracingPass(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<RaytracingPass> raytracingPass)
	{
		Renderer::Submit([renderCommandBuffer, raytracingPass]() mutable
		{
			Ref<VulkanComputePass> vulkanComputePass = raytracingPass.As<VulkanComputePass>();
			Ref<VulkanRaytracingPipeline> pipeline = raytracingPass->GetSpecification().Pipeline.As<VulkanRaytracingPipeline>();
			pipeline->End();
			vkCmdEndDebugUtilsLabelEXT(renderCommandBuffer.As<VulkanRenderCommandBuffer>()->GetActiveCommandBuffer());
		});
	}

	void VulkanRenderer::DispatchRays(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<RaytracingPass> raytracingPass, Ref<Material> material, uint32_t width, uint32_t height, uint32_t depth)
	{

		Renderer::Submit([renderCommandBuffer, raytracingPass, material, width, height, depth]() mutable
		{
			BEY_PROFILE_FUNC("VulkanRenderer::DispatchRays");
			BEY_SCOPE_PERF("VulkanRenderer::DispatchRays");


			Ref<VulkanRaytracingPipeline> pipeline = raytracingPass->GetSpecification().Pipeline.As<VulkanRaytracingPipeline>();
			const uint32_t frameIndex = Renderer::RT_GetCurrentFrameIndex();
			const VkCommandBuffer commandBuffer = renderCommandBuffer.As<VulkanRenderCommandBuffer>()->GetCommandBuffer(frameIndex);

			// Bind material descriptor set if exists
			if (material)
			{
				Ref<VulkanMaterial> vulkanMaterial = material.As<VulkanMaterial>();
				VkDescriptorSet descriptorSet = vulkanMaterial->GetDescriptorSet(frameIndex);
				if (descriptorSet)
					vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipeline->GetLayout(), 0, 1, &descriptorSet, 0, nullptr);
			}
			SET_VULKAN_CHECKPOINT(commandBuffer, fmt::eastl_format("VulkanRaytracingPipeline::Dispatch, Shader: {}", raytracingPass->GetShader()->GetName()));

			pipeline->Dispatch(width, height, depth);
			SET_VULKAN_CHECKPOINT(commandBuffer, fmt::eastl_format("VulkanRaytracingPipeline::Dispatch, after dispatch, Shader: {}", raytracingPass->GetShader()->GetName()));
		});
		s_VulkanRendererData->RaytraceCount++;
	}

	void VulkanRenderer::SetPushConstant(Ref<RaytracingPass> raytracingPass, Buffer pushConstant, ShaderStage stages)
	{
		Buffer copy = Buffer::Copy(pushConstant);
		Renderer::Submit([copy, stages, pipeline = raytracingPass->GetPipeline().As<VulkanRaytracingPipeline>()]() mutable
		{
			pipeline->RT_SetPushConstants(copy, (VkShaderStageFlagBits)stages);
			copy.Release();
		});
	}

	std::pair<Ref<TextureCube>, Ref<TextureCube>> VulkanRenderer::CreateEnvironmentMap(const std::string& filepath)
	{
		if (!Renderer::GetConfig().ComputeEnvironmentMaps)
			return { Renderer::GetBlackCubeTexture(), Renderer::GetBlackCubeTexture() };

		const uint32_t cubemapSize = Renderer::GetConfig().EnvironmentMapResolution;
		const uint32_t irradianceMapSize = 32;

		Ref<Texture2D> envEquirect = Texture2D::Create(TextureSpecification(), filepath);
		BEY_CORE_ASSERT(envEquirect->GetFormat() == ImageFormat::RGBA32F, "Texture is not HDR!");

		TextureSpecification cubemapSpec;
		cubemapSpec.Format = ImageFormat::RGBA16F;
		cubemapSpec.Width = cubemapSize;
		cubemapSpec.Height = cubemapSize;
		cubemapSpec.DebugName = "envUnfiltered";

		Ref<TextureCube> envUnfiltered = TextureCube::Create(cubemapSpec);
		cubemapSpec.DebugName = "envFiltered";
		Ref<TextureCube> envFiltered = TextureCube::Create(cubemapSpec);

		// Convert equirectangular to cubemap
		Ref<Shader> equirectangularConversionShader = Renderer::GetShaderLibrary()->Get("EquirectangularToCubeMap");
		Ref<VulkanComputePipeline> equirectangularConversionPipeline = Ref<VulkanComputePipeline>::Create(equirectangularConversionShader);

		Renderer::Submit([equirectangularConversionPipeline, envEquirect, envUnfiltered, cubemapSize]() mutable
		{
			VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();
			Ref<VulkanShader> shader = equirectangularConversionPipeline->GetShader();

			std::array<VkWriteDescriptorSet, 2> writeDescriptors;
			auto descriptorSet = shader->CreateDescriptorSets();
			Ref<VulkanTextureCube> envUnfilteredVK = envUnfiltered.As<VulkanTextureCube>();
			writeDescriptors[0] = *shader->GetDescriptorSet("o_CubeMap");
			writeDescriptors[0].dstSet = descriptorSet.DescriptorSets[0]; // Should this be set inside the shader?
			writeDescriptors[0].pImageInfo = &envUnfilteredVK->GetVulkanDescriptorInfo();

			Ref<VulkanTexture2D> envEquirectVK = envEquirect.As<VulkanTexture2D>();
			writeDescriptors[1] = *shader->GetDescriptorSet("u_EquirectangularTex");
			writeDescriptors[1].dstSet = descriptorSet.DescriptorSets[0]; // Should this be set inside the shader?
			writeDescriptors[1].pImageInfo = &envEquirectVK->GetVulkanDescriptorInfo();

			vkUpdateDescriptorSets(device, (uint32_t)writeDescriptors.size(), writeDescriptors.data(), 0, NULL);
			equirectangularConversionPipeline->Execute(descriptorSet.DescriptorSets.data(), (uint32_t)descriptorSet.DescriptorSets.size(), cubemapSize / 32, cubemapSize / 32, 6);

			envUnfilteredVK->GenerateMips(true);
		});

		// Copy environment map as-is to filtered mip level 0.  This level is used for perfectly reflective materials
		Renderer::Submit([equirectangularConversionPipeline, envEquirect, envFiltered, cubemapSize]() mutable
		{
			VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();
			Ref<VulkanShader> shader = equirectangularConversionPipeline->GetShader();

			std::array<VkWriteDescriptorSet, 2> writeDescriptors;
			auto descriptorSet = shader->CreateDescriptorSets();
			Ref<VulkanTextureCube> envFilteredVK = envFiltered.As<VulkanTextureCube>();
			writeDescriptors[0] = *shader->GetDescriptorSet("o_CubeMap");
			writeDescriptors[0].dstSet = descriptorSet.DescriptorSets[0]; // Should this be set inside the shader?
			writeDescriptors[0].pImageInfo = &envFilteredVK->GetVulkanDescriptorInfo();

			Ref<VulkanTexture2D> envEquirectVK = envEquirect.As<VulkanTexture2D>();
			writeDescriptors[1] = *shader->GetDescriptorSet("u_EquirectangularTex");
			writeDescriptors[1].dstSet = descriptorSet.DescriptorSets[0]; // Should this be set inside the shader?
			writeDescriptors[1].pImageInfo = &envEquirectVK->GetVulkanDescriptorInfo();

			vkUpdateDescriptorSets(device, (uint32_t)writeDescriptors.size(), writeDescriptors.data(), 0, NULL);
			equirectangularConversionPipeline->Execute(descriptorSet.DescriptorSets.data(), (uint32_t)descriptorSet.DescriptorSets.size(), cubemapSize / 32, cubemapSize / 32, 6);
		});

		Ref<Shader> environmentMipFilterShader = Renderer::GetShaderLibrary()->Get("EnvironmentMipFilter");
		Ref<VulkanComputePipeline> environmentMipFilterPipeline = Ref<VulkanComputePipeline>::Create(environmentMipFilterShader);

		Renderer::Submit([environmentMipFilterPipeline, envUnfiltered, envFiltered, cubemapSize]() mutable
		{
			VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();
			Ref<VulkanShader> shader = environmentMipFilterPipeline->GetShader();

			Ref<VulkanTextureCube> envFilteredCubemap = envFiltered.As<VulkanTextureCube>();
			VkDescriptorImageInfo imageInfo = envFilteredCubemap->GetVulkanDescriptorInfo();

			uint32_t mipCount = Utils::CalculateMipCount(cubemapSize, cubemapSize);

			std::vector<VkWriteDescriptorSet> writeDescriptors(mipCount * 2);
			std::vector<VkDescriptorImageInfo> mipImageInfos(mipCount);
			auto descriptorSet = shader->CreateDescriptorSets(0, mipCount);
			for (uint32_t i = 0; i < mipCount; i++)
			{
				VkDescriptorImageInfo& mipImageInfo = mipImageInfos[i];
				mipImageInfo = imageInfo;
				mipImageInfo.imageView = envFilteredCubemap->CreateImageViewSingleMip(i);

				writeDescriptors[i * 2 + 0] = *shader->GetDescriptorSet("outputTexture");
				writeDescriptors[i * 2 + 0].dstSet = descriptorSet.DescriptorSets[i]; // Should this be set inside the shader?
				writeDescriptors[i * 2 + 0].pImageInfo = &mipImageInfo;

				Ref<VulkanTextureCube> envUnfilteredCubemap = envUnfiltered.As<VulkanTextureCube>();
				writeDescriptors[i * 2 + 1] = *shader->GetDescriptorSet("inputTexture");
				writeDescriptors[i * 2 + 1].dstSet = descriptorSet.DescriptorSets[i]; // Should this be set inside the shader?
				writeDescriptors[i * 2 + 1].pImageInfo = &envUnfilteredCubemap->GetVulkanDescriptorInfo();
			}

			vkUpdateDescriptorSets(device, (uint32_t)writeDescriptors.size(), writeDescriptors.data(), 0, NULL);

			environmentMipFilterPipeline->RT_Begin(); // begin compute pass
			const float deltaRoughness = 1.0f / glm::max((float)envFiltered->GetMipLevelCount() - 1.0f, 1.0f);

			// note: mip level 0 is a copy of the unfiltered env map (see above)
			for (uint32_t i = 1, size = cubemapSize; i < mipCount; i++, size /= 2)
			{
				uint32_t numGroups = glm::max(1u, size / 32);
				float roughness = i * deltaRoughness;
				//roughness = glm::max(roughness, 0.05f);  // not needed, since roughness is always > 0.0f
				vkCmdBindDescriptorSets(environmentMipFilterPipeline->GetActiveCommandBuffer(), VK_PIPELINE_BIND_POINT_COMPUTE, environmentMipFilterPipeline->GetLayout(), 0, 1, &descriptorSet.DescriptorSets[i], 0, nullptr);
				environmentMipFilterPipeline->SetPushConstants(Buffer{ &roughness, sizeof(float) });
				environmentMipFilterPipeline->Dispatch({ numGroups, numGroups, 6 });
			}
			environmentMipFilterPipeline->End();
		});

		Ref<Shader> environmentIrradianceShader = Renderer::GetShaderLibrary()->Get("EnvironmentIrradiance");
		Ref<VulkanComputePipeline> environmentIrradiancePipeline = Ref<VulkanComputePipeline>::Create(environmentIrradianceShader);

		cubemapSpec.Width = irradianceMapSize;
		cubemapSpec.Height = irradianceMapSize;
		Ref<TextureCube> irradianceMap = TextureCube::Create(cubemapSpec);

		Renderer::Submit([environmentIrradiancePipeline, irradianceMap, envFiltered]() mutable
		{
			VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();
			Ref<VulkanShader> shader = environmentIrradiancePipeline->GetShader();

			Ref<VulkanTextureCube> envFilteredCubemap = envFiltered.As<VulkanTextureCube>();
			Ref<VulkanTextureCube> irradianceCubemap = irradianceMap.As<VulkanTextureCube>();
			auto descriptorSet = shader->CreateDescriptorSets();

			std::array<VkWriteDescriptorSet, 2> writeDescriptors;
			writeDescriptors[0] = *shader->GetDescriptorSet("o_IrradianceMap");
			writeDescriptors[0].dstSet = descriptorSet.DescriptorSets[0];
			writeDescriptors[0].pImageInfo = &irradianceCubemap->GetVulkanDescriptorInfo();

			writeDescriptors[1] = *shader->GetDescriptorSet("u_RadianceMap");
			writeDescriptors[1].dstSet = descriptorSet.DescriptorSets[0];
			writeDescriptors[1].pImageInfo = &envFilteredCubemap->GetVulkanDescriptorInfo();

			vkUpdateDescriptorSets(device, (uint32_t)writeDescriptors.size(), writeDescriptors.data(), 0, NULL);
			environmentIrradiancePipeline->RT_Begin();
			vkCmdBindDescriptorSets(environmentIrradiancePipeline->GetActiveCommandBuffer(), VK_PIPELINE_BIND_POINT_COMPUTE, environmentIrradiancePipeline->GetLayout(), 0, 1, &descriptorSet.DescriptorSets[0], 0, nullptr);
			environmentIrradiancePipeline->SetPushConstants(Buffer(&Renderer::GetConfig().IrradianceMapComputeSamples, sizeof(uint32_t)));
			environmentIrradiancePipeline->Dispatch({ irradianceMap->GetWidth() / 32, irradianceMap->GetHeight() / 32, 6 });
			environmentIrradiancePipeline->End();

			irradianceCubemap->GenerateMips();
		});

		return { envFiltered, irradianceMap };
	}

	Ref<TextureCube> VulkanRenderer::CreatePreethamSky(float turbidity, float azimuth, float inclination)
	{
		const uint32_t cubemapSize = Renderer::GetConfig().EnvironmentMapResolution;
		const uint32_t irradianceMapSize = 32;

		TextureSpecification cubemapSpec;
		cubemapSpec.Format = ImageFormat::RGBA32F;
		cubemapSpec.Width = cubemapSize;
		cubemapSpec.Height = cubemapSize;

		Ref<TextureCube> environmentMap = TextureCube::Create(cubemapSpec);

		Ref<Shader> preethamSkyShader = Renderer::GetShaderLibrary()->Get("PreethamSky");
		Ref<VulkanComputePipeline> preethamSkyComputePipeline = Ref<VulkanComputePipeline>::Create(preethamSkyShader);

		glm::vec3 params = { turbidity, azimuth, inclination };
		Renderer::Submit([preethamSkyComputePipeline, environmentMap, cubemapSize, params]() mutable
		{
			VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();
			Ref<VulkanShader> shader = preethamSkyComputePipeline->GetShader();

			std::array<VkWriteDescriptorSet, 1> writeDescriptors;
			auto descriptorSet = shader->CreateDescriptorSets();
			Ref<VulkanTextureCube> envUnfilteredCubemap = environmentMap.As<VulkanTextureCube>();
			writeDescriptors[0] = *shader->GetDescriptorSet("o_CubeMap");
			writeDescriptors[0].dstSet = descriptorSet.DescriptorSets[0]; // Should this be set inside the shader?
			writeDescriptors[0].pImageInfo = &envUnfilteredCubemap->GetVulkanDescriptorInfo();

			vkUpdateDescriptorSets(device, (uint32_t)writeDescriptors.size(), writeDescriptors.data(), 0, NULL);

			// BIND DES descriptorSet.DescriptorSets[0]
			preethamSkyComputePipeline->RT_Begin();
			vkCmdBindDescriptorSets(preethamSkyComputePipeline->GetActiveCommandBuffer(), VK_PIPELINE_BIND_POINT_COMPUTE, preethamSkyComputePipeline->GetLayout(), 0, 1, &descriptorSet.DescriptorSets[0], 0, nullptr);
			preethamSkyComputePipeline->SetPushConstants(Buffer(&params, sizeof(glm::vec3)));
			preethamSkyComputePipeline->Dispatch({ cubemapSize / 32, cubemapSize / 32, 6 });
			preethamSkyComputePipeline->End();

			envUnfilteredCubemap->GenerateMips(true);
		});

		return environmentMap;
	}

	uint32_t VulkanRenderer::GetDescriptorAllocationCount(uint32_t frameIndex)
	{
		return s_VulkanRendererData->DescriptorPoolAllocationCount[frameIndex];
	}

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
			VkImageSubresourceRange subresourceRange)
		{
			VkImageMemoryBarrier imageMemoryBarrier{};
			imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

			imageMemoryBarrier.srcAccessMask = srcAccessMask;
			imageMemoryBarrier.dstAccessMask = dstAccessMask;
			imageMemoryBarrier.oldLayout = oldImageLayout;
			imageMemoryBarrier.newLayout = newImageLayout;
			imageMemoryBarrier.image = image;
			imageMemoryBarrier.subresourceRange = subresourceRange;

			vkCmdPipelineBarrier(
				cmdbuffer,
				srcStageMask,
				dstStageMask,
				0,
				0, nullptr,
				0, nullptr,
				1, &imageMemoryBarrier);
		}

		void InsertBufferMemoryBarrier(
			VkCommandBuffer cmdbuffer,
			VkBuffer buffer,
			VkAccessFlags srcAccessMask,
			VkAccessFlags dstAccessMask,
			VkPipelineStageFlags srcStageMask,
			VkPipelineStageFlags dstStageMask,
			uint32_t size,
			uint32_t offset)
		{
			VkBufferMemoryBarrier bufferMemoryBarrier{};

			bufferMemoryBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
			bufferMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			bufferMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

			bufferMemoryBarrier.srcAccessMask = srcAccessMask;
			bufferMemoryBarrier.dstAccessMask = dstAccessMask;
			bufferMemoryBarrier.buffer = buffer;
			bufferMemoryBarrier.size = size;
			bufferMemoryBarrier.offset = offset;

			vkCmdPipelineBarrier(
				cmdbuffer,
				srcStageMask,
				dstStageMask,
				0,
				0, nullptr,
				1, &bufferMemoryBarrier,
				0, nullptr);
		}

		void SetImageLayout(
			VkCommandBuffer cmdbuffer,
			VkImage image,
			VkImageLayout oldImageLayout,
			VkImageLayout newImageLayout,
			VkImageSubresourceRange subresourceRange,
			VkPipelineStageFlags srcStageMask,
			VkPipelineStageFlags dstStageMask)
		{
			// Create an image barrier object
			VkImageMemoryBarrier imageMemoryBarrier = {};
			imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			imageMemoryBarrier.oldLayout = oldImageLayout;
			imageMemoryBarrier.newLayout = newImageLayout;
			imageMemoryBarrier.image = image;
			imageMemoryBarrier.subresourceRange = subresourceRange;

			// Source layouts (old)
			// Source access mask controls actions that have to be finished on the old layout
			// before it will be transitioned to the new layout
			switch (oldImageLayout)
			{
				case VK_IMAGE_LAYOUT_UNDEFINED:
					// Image layout is undefined (or does not matter)
					// Only valid as initial layout
					// No flags required, listed only for completeness
					imageMemoryBarrier.srcAccessMask = 0;
					break;

				case VK_IMAGE_LAYOUT_PREINITIALIZED:
					// Image is preinitialized
					// Only valid as initial layout for linear images, preserves memory contents
					// Make sure host writes have been finished
					imageMemoryBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
					break;

				case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
					// Image is a color attachment
					// Make sure any writes to the color buffer have been finished
					imageMemoryBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
					break;

				case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
					// Image is a depth/stencil attachment
					// Make sure any writes to the depth/stencil buffer have been finished
					imageMemoryBarrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
					break;

				case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
					// Image is a transfer source
					// Make sure any reads from the image have been finished
					imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
					break;

				case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
					// Image is a transfer destination
					// Make sure any writes to the image have been finished
					imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
					break;

				case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
					// Image is read by a shader
					// Make sure any shader reads from the image have been finished
					imageMemoryBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
					break;
				default:
					// Other source layouts aren't handled (yet)
					break;
			}

			// Target layouts (new)
			// Destination access mask controls the dependency for the new image layout
			switch (newImageLayout)
			{
				case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
					// Image will be used as a transfer destination
					// Make sure any writes to the image have been finished
					imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
					break;

				case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
					// Image will be used as a transfer source
					// Make sure any reads from the image have been finished
					imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
					break;

				case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
					// Image will be used as a color attachment
					// Make sure any writes to the color buffer have been finished
					imageMemoryBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
					break;

				case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
					// Image layout will be used as a depth/stencil attachment
					// Make sure any writes to depth/stencil buffer have been finished
					imageMemoryBarrier.dstAccessMask = imageMemoryBarrier.dstAccessMask | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
					break;

				case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
					// Image will be read in a shader (sampler, input attachment)
					// Make sure any writes to the image have been finished
					if (imageMemoryBarrier.srcAccessMask == 0)
					{
						imageMemoryBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
					}
					imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
					break;
				default:
					// Other source layouts aren't handled (yet)
					break;
			}

			// Put barrier inside setup command buffer
			vkCmdPipelineBarrier(
				cmdbuffer,
				srcStageMask,
				dstStageMask,
				0,
				0, nullptr,
				0, nullptr,
				1, &imageMemoryBarrier);
		}

		void SetImageLayout(
			VkCommandBuffer cmdbuffer,
			VkImage image,
			VkImageAspectFlags aspectMask,
			VkImageLayout oldImageLayout,
			VkImageLayout newImageLayout,
			VkPipelineStageFlags srcStageMask,
			VkPipelineStageFlags dstStageMask)
		{
			VkImageSubresourceRange subresourceRange = {};
			subresourceRange.aspectMask = aspectMask;
			subresourceRange.baseMipLevel = 0;
			subresourceRange.levelCount = 1;
			subresourceRange.layerCount = 1;
			SetImageLayout(cmdbuffer, image, oldImageLayout, newImageLayout, subresourceRange, srcStageMask, dstStageMask);
		}

	}

}
