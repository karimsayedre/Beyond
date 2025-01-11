#include "pch.h"
#include "VulkanComputePipeline.h"

#include "Beyond/Renderer/Renderer.h"

#include "Beyond/Platform/Vulkan/VulkanContext.h"
#include "Beyond/Platform/Vulkan/VulkanDiagnostics.h"
#include "Beyond/Platform/Vulkan/VulkanStorageBuffer.h"

#include "Beyond/Core/Timer.h"

namespace Beyond {


	VulkanComputePipeline::VulkanComputePipeline(Ref<Shader> computeShader)
		: m_Shader(computeShader.As<VulkanShader>())
	{
		Ref<VulkanComputePipeline> instance = this;
		Renderer::Submit([instance]() mutable
		{
			instance->RT_CreatePipeline();
		});
		Renderer::RegisterShaderDependency(computeShader, this);
	}

	void VulkanComputePipeline::CreatePipeline()
	{
		Renderer::Submit([instance = Ref(this)]() mutable
		{
			instance->RT_CreatePipeline();
		});
	}

	void VulkanComputePipeline::BufferMemoryBarrier(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<StorageBuffer> storageBuffer, ResourceAccessFlags fromAccess, ResourceAccessFlags toAccess)
	{
		BufferMemoryBarrier(renderCommandBuffer, storageBuffer, PipelineStage::ComputeShader, fromAccess, PipelineStage::ComputeShader, toAccess);
	}

	void VulkanComputePipeline::BufferMemoryBarrier(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<StorageBuffer> storageBuffer, PipelineStage fromStage, ResourceAccessFlags fromAccess, PipelineStage toStage, ResourceAccessFlags toAccess)
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

	void VulkanComputePipeline::ImageMemoryBarrier(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<Image2D> image, ResourceAccessFlags fromAccess, ResourceAccessFlags toAccess)
	{
		ImageMemoryBarrier(renderCommandBuffer, image, PipelineStage::ComputeShader, fromAccess, PipelineStage::ComputeShader, toAccess);
	}

	void VulkanComputePipeline::ImageMemoryBarrier(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<Image2D> image, PipelineStage fromStage, ResourceAccessFlags fromAccess, PipelineStage toStage, ResourceAccessFlags toAccess)
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

	void VulkanComputePipeline::Release()
	{
		Renderer::SubmitResourceFree([pipeline = m_ComputePipeline, cache = m_PipelineCache, layout = m_ComputePipelineLayout]
		{
			const auto device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();
			vkDestroyPipeline(device, pipeline, nullptr);
			vkDestroyPipelineCache(device, cache, nullptr);
			vkDestroyPipelineLayout(device, layout, nullptr);
		});

		m_ComputePipeline = VK_NULL_HANDLE;
		m_PipelineCache = VK_NULL_HANDLE;
		m_ComputePipelineLayout = VK_NULL_HANDLE;
	}

	void VulkanComputePipeline::RT_CreatePipeline()
	{
		Release();
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();

		// TODO: Abstract into some sort of compute pipeline

		auto descriptorSetLayouts = m_Shader->GetAllDescriptorSetLayouts();

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

		VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &m_ComputePipelineLayout));
		VKUtils::SetDebugUtilsObjectName(device, VK_OBJECT_TYPE_PIPELINE_LAYOUT, m_Shader->GetName().c_str(), m_ComputePipelineLayout);

		VkComputePipelineCreateInfo computePipelineCreateInfo{};
		computePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
		computePipelineCreateInfo.layout = m_ComputePipelineLayout;
		computePipelineCreateInfo.flags = 0;
		const auto& shaderStages = m_Shader->GetPipelineShaderStageCreateInfos();
		computePipelineCreateInfo.stage = shaderStages[0];

		/*VkPipelineCacheCreateInfo pipelineCacheCreateInfo = {};
		pipelineCacheCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;

		VK_CHECK_RESULT(vkCreatePipelineCache(device, &pipelineCacheCreateInfo, nullptr, &m_PipelineCache));*/
		VK_CHECK_RESULT(vkCreateComputePipelines(device, nullptr, 1, &computePipelineCreateInfo, nullptr, &m_ComputePipeline));

		VKUtils::SetDebugUtilsObjectName(device, VK_OBJECT_TYPE_PIPELINE, m_Shader->GetName().c_str(), m_ComputePipeline);
	}

	void VulkanComputePipeline::Execute(VkDescriptorSet* descriptorSets, uint32_t descriptorSetCount, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ)
	{
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();

		VkQueue computeQueue = VulkanContext::GetCurrentDevice()->GetComputeQueue();
		//vkQueueWaitIdle(computeQueue); // TODO: don't

		VkCommandBuffer computeCommandBuffer = VulkanContext::GetCurrentDevice()->CreateCommandBuffer(fmt::eastl_format("Executing compute shader named: {}", m_Shader->GetName()), true, true);


		vkCmdBindPipeline(computeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_ComputePipeline);
		for (uint32_t i = 0; i < descriptorSetCount; i++)
		{
			vkCmdBindDescriptorSets(computeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_ComputePipelineLayout, 0, 1, &descriptorSets[i], 0, 0);
			SET_VULKAN_CHECKPOINT(computeCommandBuffer, fmt::eastl_format("VulkanComputePipeline::Execute({})", m_Shader->GetName()));
			vkCmdDispatch(computeCommandBuffer, groupCountX, groupCountY, groupCountZ);
			SET_VULKAN_CHECKPOINT(computeCommandBuffer, fmt::eastl_format("VulkanComputePipeline::Dispatch, after dispatch, Shader: {}", m_Shader->GetName()));

		}

		vkEndCommandBuffer(computeCommandBuffer);
		if (!s_ComputeFence)
		{

			VkFenceCreateInfo fenceCreateInfo{};
			fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
			fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
			VK_CHECK_RESULT(vkCreateFence(device, &fenceCreateInfo, nullptr, &s_ComputeFence));

			VKUtils::SetDebugUtilsObjectName(device, VK_OBJECT_TYPE_FENCE, fmt::eastl_format("Compute pipeline fence"), s_ComputeFence);
		}

		// Make sure previous compute shader in pipeline has completed (TODO: this shouldn't be needed for all cases)
		vkWaitForFences(device, 1, &s_ComputeFence, VK_TRUE, UINT64_MAX);
		vkResetFences(device, 1, &s_ComputeFence);

		VkSubmitInfo computeSubmitInfo{};
		computeSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		computeSubmitInfo.commandBufferCount = 1;
		computeSubmitInfo.pCommandBuffers = &computeCommandBuffer;
		VK_CHECK_RESULT(vkQueueSubmit(computeQueue, 1, &computeSubmitInfo, s_ComputeFence));

		// Wait for execution of compute shader to complete
		// Currently this is here for "safety"
		{
			BEY_SCOPE_TIMER("Compute shader execution");
			vkWaitForFences(device, 1, &s_ComputeFence, VK_TRUE, UINT64_MAX);
		}
	}

	void VulkanComputePipeline::Begin(Ref<RenderCommandBuffer> renderCommandBuffer)
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
			m_ActiveComputeCommandBuffer = VulkanContext::GetCurrentDevice()->CreateCommandBuffer(fmt::eastl_format("Beginning compute pipeline with shader named: {}", m_Shader->GetName()), true, true);
			m_UsingGraphicsQueue = false;
		}
		vkCmdBindPipeline(m_ActiveComputeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_ComputePipeline);
	}

	void VulkanComputePipeline::RT_Begin(Ref<RenderCommandBuffer> renderCommandBuffer)
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
			m_ActiveComputeCommandBuffer = VulkanContext::GetCurrentDevice()->CreateCommandBuffer(fmt::eastl_format("RT Beginning compute pipeline with shader named: {}", m_Shader->GetName()), true, true);
			m_UsingGraphicsQueue = false;
		}
		vkCmdBindPipeline(m_ActiveComputeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_ComputePipeline);
	}


	void VulkanComputePipeline::Dispatch(const glm::uvec3& workGroups) const
	{
		BEY_CORE_ASSERT(m_ActiveComputeCommandBuffer);

		vkCmdDispatch(m_ActiveComputeCommandBuffer, workGroups.x, workGroups.y, workGroups.z);
	}

	void VulkanComputePipeline::End()
	{
		BEY_CORE_ASSERT(m_ActiveComputeCommandBuffer);

		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();
		if (!m_UsingGraphicsQueue)
		{
			VkQueue computeQueue = VulkanContext::GetCurrentDevice()->GetComputeQueue();

			vkEndCommandBuffer(m_ActiveComputeCommandBuffer);

			if (!s_ComputeFence)
			{
				VkFenceCreateInfo fenceCreateInfo{};
				fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
				fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
				VK_CHECK_RESULT(vkCreateFence(device, &fenceCreateInfo, nullptr, &s_ComputeFence));
				VKUtils::SetDebugUtilsObjectName(device, VK_OBJECT_TYPE_FENCE, "Compute pipeline fence", s_ComputeFence);
			}
			vkWaitForFences(device, 1, &s_ComputeFence, VK_TRUE, UINT64_MAX);
			vkResetFences(device, 1, &s_ComputeFence);

			VkSubmitInfo computeSubmitInfo{};
			computeSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
			computeSubmitInfo.commandBufferCount = 1;
			computeSubmitInfo.pCommandBuffers = &m_ActiveComputeCommandBuffer;
			VK_CHECK_RESULT(vkQueueSubmit(computeQueue, 1, &computeSubmitInfo, s_ComputeFence));

			// Wait for execution of compute shader to complete
			// Currently this is here for "safety"
			{
				BEY_SCOPE_TIMER("Compute shader execution");
				vkWaitForFences(device, 1, &s_ComputeFence, VK_TRUE, UINT64_MAX);
			}
		}
		m_ActiveComputeCommandBuffer = nullptr;
	}

	void VulkanComputePipeline::SetPushConstants(Buffer constants) const
	{
		vkCmdPushConstants(m_ActiveComputeCommandBuffer, m_ComputePipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, (uint32_t)constants.Size, constants.Data);
	}


}
