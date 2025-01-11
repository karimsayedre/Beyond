#include "pch.h"
#include "VulkanRaytracingPipeline.h"



#include "VulkanShader.h"
#include "VulkanContext.h"
#include "VulkanDiagnostics.h"
#include "VulkanFramebuffer.h"
#include "VulkanRenderCommandBuffer.h"
#include "VulkanStorageBuffer.h"
#include "VulkanUniformBuffer.h"

#include "Beyond/Renderer/Renderer.h"
#include "VulkanComputePipeline.h"
#include "Beyond/Core/Timer.h"

namespace Beyond {

	VulkanRaytracingPipeline::VulkanRaytracingPipeline(Ref<Shader> shader)
		: m_Shader(shader.As<VulkanShader>())
	{
		m_Specification.DebugName = eastl::string(m_Shader->GetName().c_str(), m_Shader->GetName().size());

		const auto device = VulkanContext::GetCurrentDevice();

		m_RaytracingSBT.setup(device->GetVulkanDevice(), VulkanContext::GetCurrentDevice()->GetPhysicalDevice()->GetRaytracingPipelineProperties());

		BEY_CORE_ASSERT(shader);
		Ref<VulkanRaytracingPipeline> instance = this;
		Renderer::Submit([instance]() mutable
		{
			instance->RT_CreatePipeline();
		});
		Renderer::RegisterShaderDependency(shader, this);
	}

	void VulkanRaytracingPipeline::CreatePipeline()
	{
		Renderer::Submit([instance = Ref(this)]() mutable
		{
			instance->RT_CreatePipeline();
		});
	}

	void VulkanRaytracingPipeline::BufferMemoryBarrier(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<StorageBuffer> storageBuffer, ResourceAccessFlags fromAccess, ResourceAccessFlags toAccess)
	{
		BufferMemoryBarrier(renderCommandBuffer, storageBuffer, PipelineStage::RaytracingShader, fromAccess, PipelineStage::RaytracingShader, toAccess);
	}

	void VulkanRaytracingPipeline::BufferMemoryBarrier(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<StorageBuffer> storageBuffer, PipelineStage fromStage, ResourceAccessFlags fromAccess, PipelineStage toStage, ResourceAccessFlags toAccess)
	{
		Renderer::Submit([vulkanRenderCommandBuffer = renderCommandBuffer.As<VulkanRenderCommandBuffer>(), vulkanStorageBuffer = storageBuffer.As<VulkanStorageBuffer>(), fromStage, fromAccess, toStage, toAccess]() mutable
		{
			VkBufferMemoryBarrier bufferMemoryBarrier = {};
			bufferMemoryBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
			bufferMemoryBarrier.buffer = vulkanStorageBuffer->GetVulkanBuffer();
			bufferMemoryBarrier.offset = 0;
			bufferMemoryBarrier.size = VK_WHOLE_SIZE;
			bufferMemoryBarrier.srcAccessMask = (VkAccessFlags)fromAccess;
			bufferMemoryBarrier.dstAccessMask = (VkAccessFlags)toAccess;
			vkCmdPipelineBarrier(
				vulkanRenderCommandBuffer->GetActiveCommandBuffer(),
				(VkPipelineStageFlagBits)fromStage,
				(VkPipelineStageFlagBits)toStage,
				0,
				0, nullptr,
				1, &bufferMemoryBarrier,
				0, nullptr);
		});
	}

	void VulkanRaytracingPipeline::ImageMemoryBarrier(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<Image2D> image, ResourceAccessFlags fromAccess, ResourceAccessFlags toAccess)
	{
		ImageMemoryBarrier(renderCommandBuffer, image, PipelineStage::RaytracingShader, fromAccess, PipelineStage::RaytracingShader, toAccess);
	}

	void VulkanRaytracingPipeline::ImageMemoryBarrier(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<Image2D> image, PipelineStage fromStage, ResourceAccessFlags fromAccess, PipelineStage toStage, ResourceAccessFlags toAccess)
	{
		Renderer::Submit([vulkanRenderCommandBuffer = renderCommandBuffer.As<VulkanRenderCommandBuffer>(), vulkanImage = image.As<VulkanImage2D>(), fromStage, fromAccess, toStage, toAccess]() mutable
		{
			VkImageLayout imageLayout = vulkanImage->GetVulkanDescriptorInfo().imageLayout;

			VkImageMemoryBarrier imageMemoryBarrier = {};
			imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			imageMemoryBarrier.oldLayout = imageLayout;
			imageMemoryBarrier.newLayout = imageLayout;
			imageMemoryBarrier.image = vulkanImage->GetImageInfo().Image;
			// TODO: get layer count from image; also take SubresourceRange as parameter
			imageMemoryBarrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, vulkanImage->GetSpecification().Mips, 0, 1 };
			imageMemoryBarrier.srcAccessMask = (VkAccessFlags)fromAccess;
			imageMemoryBarrier.dstAccessMask = (VkAccessFlags)toAccess;
			vkCmdPipelineBarrier(
				vulkanRenderCommandBuffer->GetActiveCommandBuffer(),
				(VkPipelineStageFlagBits)fromStage,
				(VkPipelineStageFlagBits)toStage,
				0,
				0, nullptr,
				0, nullptr,
				1, &imageMemoryBarrier);
		});
	}

	void VulkanRaytracingPipeline::RT_CreatePipeline()
	{
		Release();
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();


		const auto& descriptorSetLayouts = m_Shader->GetAllDescriptorSetLayouts();

		VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
		pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutCreateInfo.setLayoutCount = (uint32_t)descriptorSetLayouts.size();
		pipelineLayoutCreateInfo.pSetLayouts = descriptorSetLayouts.data();

		const auto& pushConstantRanges = m_Shader->GetPushConstantRanges();
		std::vector<VkPushConstantRange> vulkanPushConstantRanges(pushConstantRanges.size());
		if (pushConstantRanges.size())
		{
			// TODO: should come from shader
			for (uint32_t i = 0; i < pushConstantRanges.size(); i++)
			{
				const auto& pushConstantRange = pushConstantRanges[i];
				auto& vulkanPushConstantRange = vulkanPushConstantRanges[i];

				vulkanPushConstantRange.stageFlags = pushConstantRange.ShaderStage;
				vulkanPushConstantRange.offset = pushConstantRange.Offset;
				vulkanPushConstantRange.size = pushConstantRange.Size;
			}

			pipelineLayoutCreateInfo.pushConstantRangeCount = (uint32_t)vulkanPushConstantRanges.size();
			pipelineLayoutCreateInfo.pPushConstantRanges = vulkanPushConstantRanges.data();
		}

		VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &m_PipelineLayout));
		VKUtils::SetDebugUtilsObjectName(device, VK_OBJECT_TYPE_PIPELINE_LAYOUT, m_Specification.DebugName, m_PipelineLayout);


		VkRayTracingPipelineCreateInfoKHR pipelineCreateInfo = {};
		pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
		// The layout used for this pipeline (can be shared among multiple pipelines using the same layout)
		pipelineCreateInfo.layout = m_PipelineLayout;
		pipelineCreateInfo.flags = 0;

		const auto& shaderGroups = m_Shader->GetRaytracingShaderGroupCreateInfos();
		pipelineCreateInfo.groupCount = (uint32_t)shaderGroups.size();
		pipelineCreateInfo.pGroups = shaderGroups.data();

		const auto& shaderStages = m_Shader->GetPipelineShaderStageCreateInfos();
		// Set pipeline shader stage info
		pipelineCreateInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
		pipelineCreateInfo.pStages = shaderStages.data();

		std::vector<VkDynamicState> dynamicStateEnables;
		//dynamicStateEnables.push_back(VK_DYNAMIC_STATE_RAY_TRACING_PIPELINE_STACK_SIZE_KHR);

		VkPipelineDynamicStateCreateInfo dynamicState = {};
		dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		dynamicState.pDynamicStates = dynamicStateEnables.data();
		dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStateEnables.size());
		pipelineCreateInfo.pDynamicState = &dynamicState;
		pipelineCreateInfo.maxPipelineRayRecursionDepth = 1;  // Ray depth

		// NOTE: we can use pipeline caches to serialize pipelines to disk for faster loading
		VkPipelineCacheCreateInfo pipelineCacheCreateInfo = {};
		pipelineCacheCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
		VK_CHECK_RESULT(vkCreatePipelineCache(device, &pipelineCacheCreateInfo, nullptr, &m_PipelineCache));

		// Create rendering pipeline using the specified states
		VK_CHECK_RESULT(vkCreateRayTracingPipelinesKHR(device, nullptr, m_PipelineCache, 1, &pipelineCreateInfo, nullptr, &m_VulkanPipeline));
		VKUtils::SetDebugUtilsObjectName(device, VK_OBJECT_TYPE_PIPELINE, m_Specification.DebugName, m_VulkanPipeline);
		m_RaytracingSBT.create(m_VulkanPipeline, pipelineCreateInfo);
	}

	void VulkanRaytracingPipeline::Release()
	{
		Renderer::SubmitResourceFree([pipeline = m_VulkanPipeline, pipelineCache = m_PipelineCache, pipelineLayout = m_PipelineLayout]()
		{
			const auto vulkanDevice = VulkanContext::GetCurrentDevice()->GetVulkanDevice();
			vkDestroyPipeline(vulkanDevice, pipeline, nullptr);
			vkDestroyPipelineCache(vulkanDevice, pipelineCache, nullptr);
			vkDestroyPipelineLayout(vulkanDevice, pipelineLayout, nullptr);
		});

		m_VulkanPipeline = nullptr;
		m_PipelineCache = nullptr;
		m_PipelineLayout = nullptr;
	}

	void VulkanRaytracingPipeline::Begin(Ref<RenderCommandBuffer> renderCommandBuffer)
	{
		BEY_CORE_ASSERT(!m_ActiveComputeCommandBuffer);

		if (renderCommandBuffer)
		{
			uint32_t frameIndex = Renderer::GetCurrentFrameIndex();
			m_ActiveComputeCommandBuffer = renderCommandBuffer.As<VulkanRenderCommandBuffer>()->GetCommandBuffer(frameIndex);
			m_UsingGraphicsQueue = true;
		}
		else
		{
			m_ActiveComputeCommandBuffer = VulkanContext::GetCurrentDevice()->CreateCommandBuffer(fmt::eastl_format("Beginning raytracing pipeline with shader named: {}", m_Shader->GetName()), true, false);
			m_UsingGraphicsQueue = false;
		}
		vkCmdBindPipeline(m_ActiveComputeCommandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, m_VulkanPipeline);
	}

	void VulkanRaytracingPipeline::RT_Begin(Ref<RenderCommandBuffer> renderCommandBuffer)
	{
		BEY_CORE_ASSERT(!m_ActiveComputeCommandBuffer);

		if (renderCommandBuffer)
		{
			uint32_t frameIndex = Renderer::RT_GetCurrentFrameIndex();
			m_ActiveComputeCommandBuffer = renderCommandBuffer.As<VulkanRenderCommandBuffer>()->GetCommandBuffer(frameIndex);
			m_UsingGraphicsQueue = true;
		}
		else
		{
			m_ActiveComputeCommandBuffer = VulkanContext::GetCurrentDevice()->CreateCommandBuffer(fmt::eastl_format("RT Beginning raytracing pipeline with shader named: {}", m_Shader->GetName()), true, false);
			m_UsingGraphicsQueue = false;
		}
		vkCmdBindPipeline(m_ActiveComputeCommandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, m_VulkanPipeline);
	}

	void VulkanRaytracingPipeline::RT_Begin(VkCommandBuffer commandBuffer)
	{
		BEY_CORE_ASSERT(!m_ActiveComputeCommandBuffer);
		m_ActiveComputeCommandBuffer = commandBuffer;
		m_UsingGraphicsQueue = true;
		vkCmdBindPipeline(m_ActiveComputeCommandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, m_VulkanPipeline);
	}

	void VulkanRaytracingPipeline::Dispatch(const uint32_t width, const uint32_t height, const uint32_t depth) const
	{
		BEY_CORE_ASSERT(m_ActiveComputeCommandBuffer);

		const auto regions = m_RaytracingSBT.getRegions();
		vkCmdTraceRaysKHR(m_ActiveComputeCommandBuffer, regions.data(), &regions[1], &regions[2], &regions[3], width, height, depth);
	}

	void VulkanRaytracingPipeline::End()
	{
		BEY_CORE_ASSERT(m_ActiveComputeCommandBuffer);

		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();
		//if (!m_UsingGraphicsQueue)
		//{

		//	vkEndCommandBuffer(m_ActiveComputeCommandBuffer);
		//	//Renderer::Submit([device, activeCommandBuffer = m_ActiveComputeCommandBuffer]
		//	{
		//		VkQueue computeQueue = VulkanContext::GetCurrentDevice()->GetComputeQueue();

		//		if (!s_ComputeFence)
		//		{
		//			VkFenceCreateInfo fenceCreateInfo{};
		//			fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		//			fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
		//			VK_CHECK_RESULT(vkCreateFence(device, &fenceCreateInfo, nullptr, &s_ComputeFence));
		//			VKUtils::SetDebugUtilsObjectName(device, VK_OBJECT_TYPE_FENCE, "Compute pipeline fence", s_ComputeFence);
		//		}
		//		//vkWaitForFences(device, 1, &s_ComputeFence, VK_TRUE, UINT64_MAX);
		//		//vkResetFences(device, 1, &s_ComputeFence);

		//		VkSubmitInfo computeSubmitInfo{};
		//		computeSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		//		computeSubmitInfo.commandBufferCount = 1;
		//		computeSubmitInfo.pCommandBuffers = &m_ActiveComputeCommandBuffer;
		//		VK_CHECK_RESULT(vkQueueSubmit(computeQueue, 1, &computeSubmitInfo, s_ComputeFence));

		//		// Wait for execution of compute shader to complete
		//		// Currently this is here for "safety"
		//		{
		//			//BEY_SCOPE_TIMER("Compute shader execution");
		//			//vkWaitForFences(device, 1, &s_ComputeFence, VK_TRUE, UINT64_MAX);
		//		}
		//	}//);
		//}
		m_ActiveComputeCommandBuffer = nullptr;
	}

	void VulkanRaytracingPipeline::RT_SetPushConstants(Buffer constants) const
	{
		vkCmdPushConstants(m_ActiveComputeCommandBuffer, m_PipelineLayout, VK_SHADER_STAGE_ALL, 0, (uint32_t)constants.Size, constants.Data);
	}

	void VulkanRaytracingPipeline::RT_SetPushConstants(Buffer constants, VkShaderStageFlagBits stages) const
	{
		BEY_CORE_ASSERT(m_ActiveComputeCommandBuffer, "Setting a push constant must be between a RT_Begin and RT_End.");
		vkCmdPushConstants(m_ActiveComputeCommandBuffer, m_PipelineLayout, stages, 0, (uint32_t)constants.Size, constants.Data);
	}

	VulkanRaytracingPipeline::~VulkanRaytracingPipeline()
	{
		VulkanRaytracingPipeline::Release();
	}




}


