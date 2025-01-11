#include "pch.h"


#include "VulkanDLSS.h"
#include <nvsdk_ngx_defs.h>
#include <nvsdk_ngx.h>
#include <nvsdk_ngx_vk.h>
#include <nvsdk_ngx_helpers.h>
#include <nvsdk_ngx_helpers_vk.h>

#include "VulkanContext.h"
#include "VulkanImage.h"
#include "VulkanRenderCommandBuffer.h"
#include "Beyond/Core/TimeStep.h"
#include "Beyond/Renderer/RenderCommandBuffer.h"
#include "Beyond/Renderer/Renderer.h"

namespace Beyond {

	bool VulkanDLSS::CheckSupport()
	{
		bool bShouldDestroyCapabilityParams = true;
		NVSDK_NGX_Result Result = NVSDK_NGX_VULKAN_GetCapabilityParameters(&m_Params);
		if (Result != NVSDK_NGX_Result_Success)
		{
			bShouldDestroyCapabilityParams = false;
			NVSDK_NGX_VULKAN_AllocateParameters(&m_Params);
		}
		NVSDK_NGX_Result resultUpdatedDriver = m_Params->Get(NVSDK_NGX_Parameter_SuperSampling_NeedsUpdatedDriver, &m_NeedsUpdatedDriver);
		NVSDK_NGX_Result resultMinDriverVersionMajor = m_Params->Get(NVSDK_NGX_Parameter_SuperSampling_MinDriverVersionMajor, &m_MinDriverVersionMajor);
		NVSDK_NGX_Result resultMinDriverVersionMinor = m_Params->Get(NVSDK_NGX_Parameter_SuperSampling_MinDriverVersionMinor, &m_MinDriverVersionMinor);

		if (NVSDK_NGX_SUCCEED(resultUpdatedDriver))
		{
			if (m_NeedsUpdatedDriver)
			{
				// NVIDIA DLSS cannot be loaded due to outdated driver. 
				if (NVSDK_NGX_SUCCEED(resultMinDriverVersionMajor) &&
					NVSDK_NGX_SUCCEED(resultMinDriverVersionMinor))
				{
					// Min Driver Version required: m_MinDriverVersionMajor.m_MinDriverVersionMinor 
					BEY_CORE_ASSERT(false, "DLSS Minimum Driver Version: {}.{}!\nPlease Update Your Nvidia GPU Driver.", m_MinDriverVersionMajor, m_MinDriverVersionMinor);
				}
				// Fallback to default AA solution (TAA etc) 
			}
			else
			{
				// driver update is not required - so application is not expected to
				// query minDriverVersion in this case
			}
		}

		NVSDK_NGX_Result resultDlssSupported = m_Params->Get(NVSDK_NGX_Parameter_SuperSampling_Available, &m_DLSS_Supported);
		if (NVSDK_NGX_FAILED(resultDlssSupported) || !m_DLSS_Supported)
		{
			// NVIDIA DLSS not available on this hardware/platform.
			// Fallback to default AA solution (TAA etc)
		}

		resultDlssSupported = m_Params->Get(NVSDK_NGX_Parameter_SuperSampling_FeatureInitResult, &m_DLSS_Supported);
		if (NVSDK_NGX_FAILED(resultDlssSupported) || !m_DLSS_Supported)
		{
			// NVIDIA DLSS is denied on for this application.
			// Fallback to default AA solution (TAA etc)
		}

		if (bShouldDestroyCapabilityParams)
		{
			//NVSDK_NGX_VULKAN_DestroyParameters(m_Params);
			//m_Params = nullptr;
		}

		return m_DLSS_Supported;
	}

	glm::uvec2 VulkanDLSS::GetOptimalSettings(uint32_t targetWidth, uint32_t targetHegiht, DLSSQualityValue quality)
	{
		// Assuming the game is looping through all combinations for the following:
		//
		// Resolution (TargetWidth, TargetHeight)
		// PerfQualityValue 
		// [0 MaxPerformance, 1 Balance, 2 MaxQuality, 3 UltraPerformance, 4 UltraQuality]
		float Sharpness = 0.0f; // Sharpening is deprecated, see section 3.11
		NVSDK_NGX_PerfQuality_Value dlssQuality = GetQuality(quality);
		auto res = NGX_DLSS_GET_OPTIMAL_SETTINGS(m_Params, targetWidth, targetHegiht, dlssQuality, &m_RenderWidth, &m_RenderHeight,
			&m_MaxRenderWidth, &m_MaxRenderHeight, &m_MinRenderWidth, &m_MinRenderHeight, &Sharpness);

		BEY_CORE_ASSERT(res == NVSDK_NGX_Result_Success);

		m_TargetWidth = targetWidth;
		m_TargetHeight = targetHegiht;

		if (m_RenderWidth == 0 || m_RenderHeight == 0)
		{
			// This PerfQuality mode has not been made available yet.
			// Please request another PerfQuality mode.
			BEY_CORE_VERIFY(false);
		}
		else
		{
			// Use DLSS for this combination
			// - Create feature with RecommendedOptimalRenderWidth, RecommendedOptimalRenderHeight
			// - Render to (RenderWidth, RenderHeight) between Min and Max inclusive
			// - Call DLSS to upscale to (TargetWidth, TargetHeight)
		}

		return { m_RenderWidth, m_RenderHeight };
	}

	void VulkanDLSS::CreateDLSS(Ref<RenderCommandBuffer> commandBuffer, const DLSSSettings& settings)
	{
		Renderer::Submit([commandBuffer = commandBuffer.As<VulkanRenderCommandBuffer>(), instance = Ref(this), settings]() mutable
		{
			if (!settings.Enable)
				return;
			/*
			if (m_Params)
			{
				NVSDK_NGX_VULKAN_DestroyParameters(m_Params);
				m_Params = nullptr;
			}*/
			if (instance->m_Handle)
				NVSDK_NGX_VULKAN_ReleaseFeature(instance->m_Handle);

			VkCommandBuffer   cmdBuf = commandBuffer ? commandBuffer->GetActiveCommandBuffer() : VulkanContext::GetCurrentDevice()->CreateCommandBuffer("DLSS", true, false);

			NVSDK_NGX_Feature_Create_Params featureCreateParams{ instance->m_RenderWidth, instance->m_RenderHeight, glm::max(32u, instance->m_TargetWidth), glm::max(32u, instance->m_TargetHeight), GetQuality(settings.Mode) };
			NVSDK_NGX_DLSS_Create_Params createParams{};
			createParams.Feature = featureCreateParams;
			createParams.InFeatureCreateFlags = NVSDK_NGX_DLSS_Feature_Flags_DepthInverted | NVSDK_NGX_DLSS_Feature_Flags_MVLowRes;

			if (settings.JitteredMotionVectors)
				createParams.InFeatureCreateFlags |= NVSDK_NGX_DLSS_Feature_Flags_MVJittered;

			if (settings.IsHDR)
				createParams.InFeatureCreateFlags |= NVSDK_NGX_DLSS_Feature_Flags_IsHDR;

			createParams.InEnableOutputSubrects = false;
			auto res = NGX_VULKAN_CREATE_DLSS_EXT(cmdBuf, 1, 1, &instance->m_Handle, instance->m_Params, &createParams);
			BEY_CORE_REL_ASSERT(res == NVSDK_NGX_Result_Success);

			VulkanContext::GetCurrentDevice()->FlushCommandBuffer(cmdBuf);
		});
	}

	void VulkanDLSS::Destroy()
	{
		NVSDK_NGX_VULKAN_DestroyParameters(m_Params);
		m_Params = nullptr;
	}

	void VulkanDLSS::Evaluate(const Ref<RenderCommandBuffer>& commandBuffer, uint32_t frame, const glm::vec2& jitter, Timestep timeStep, Ref<Image2D> outputColor, Ref<Image2D> inputColor, Ref<Image2D> hitTImage, Ref<Image2D> exposureImage, Ref<Image2D> motionVectors, Ref<Image2D> depthImage, Ref<Image2D> albedoImage, Ref<Image2D> metalRough, Ref<Image2D> normals)
	{
		Renderer::Submit([commandBuffer = commandBuffer.As<VulkanRenderCommandBuffer>(), instance = Ref(this), frame, jitter, outputColor, inputColor, hitTImage, motionVectors, depthImage, exposureImage, timeStep, albedoImage, metalRough, normals]() mutable
		{
			NVSDK_NGX_VK_Feature_Eval_Params featureCreateParams{ inputColor.As<VulkanImage2D>()->GetNVXResourceInfo(), outputColor.As<VulkanImage2D>()->GetNVXResourceInfo(), 0.9f };
			NVSDK_NGX_VK_DLSS_Eval_Params evalParams{};
			evalParams.Feature = featureCreateParams;
			//evalParams.pInMotionVectors3D = motionVectors.As<VulkanImage2D>()->GetNVXResourceInfo();
			evalParams.pInDepth = depthImage.As<VulkanImage2D>()->GetNVXResourceInfo();
			if (hitTImage)
				evalParams.pInRayTracingHitDistance = hitTImage.As<VulkanImage2D>()->GetNVXResourceInfo();
			evalParams.GBufferSurface.pInAttrib[NVSDK_NGX_GBUFFER_ALBEDO] = albedoImage.As<VulkanImage2D>()->GetNVXResourceInfo();
			evalParams.GBufferSurface.pInAttrib[NVSDK_NGX_GBUFFER_ROUGHNESS] = metalRough.As<VulkanImage2D>()->GetNVXResourceInfo();
			evalParams.GBufferSurface.pInAttrib[NVSDK_NGX_GBUFFER_METALLIC] = metalRough.As<VulkanImage2D>()->GetNVXResourceInfo();
			evalParams.GBufferSurface.pInAttrib[NVSDK_NGX_GBUFFER_NORMALS] = normals.As<VulkanImage2D>()->GetNVXResourceInfo();
			evalParams.pInExposureTexture = exposureImage.As<VulkanImage2D>()->GetNVXResourceInfo();
			evalParams.InReset = frame == 0;

			evalParams.InJitterOffsetX = jitter.x;
			evalParams.InJitterOffsetY = jitter.y;

			evalParams.InRenderSubrectDimensions = std::bit_cast<NVSDK_NGX_Dimensions>(inputColor->GetSize());
			evalParams.InToneMapperType = NVSDK_NGX_TONEMAPPER_ACES;
			evalParams.InFrameTimeDeltaInMsec = timeStep.GetMilliseconds();
			evalParams.pInMotionVectors = motionVectors.As<VulkanImage2D>()->GetNVXResourceInfo();
			evalParams.InIndicatorInvertYAxis = 1;

			evalParams.InRenderSubrectDimensions.Width = inputColor->GetWidth();
			evalParams.InRenderSubrectDimensions.Height = inputColor->GetHeight();
			auto res = NGX_VULKAN_EVALUATE_DLSS_EXT(commandBuffer->GetActiveCommandBuffer(), instance->m_Handle, instance->m_Params, &evalParams);
			BEY_CORE_REL_ASSERT(res == NVSDK_NGX_Result_Success);
		});
	}

	VulkanDLSS::~VulkanDLSS()
	{
		Destroy();
	}
}
