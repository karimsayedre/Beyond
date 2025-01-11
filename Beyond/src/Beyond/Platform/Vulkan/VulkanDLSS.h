#pragma once
#include <Volk/volk.h>
#include <nvsdk_ngx_defs.h>
#include <nvsdk_ngx_params.h>

#include "Beyond/Renderer/DLSS.h"
//#include "VulkanImage.h"

namespace Beyond {
	class VulkanImage2D;
	class Timestep;
	class RenderCommandBuffer;


	class VulkanDLSS : public DLSS
	{
	public:
		bool CheckSupport() override;

		inline static NVSDK_NGX_PerfQuality_Value GetQuality(DLSSQualityValue quality)
		{
			switch (quality)
			{
				case DLSSQualityValue::Balanced:return NVSDK_NGX_PerfQuality_Value_Balanced;
				case DLSSQualityValue::DLAA: return NVSDK_NGX_PerfQuality_Value_DLAA;
				case DLSSQualityValue::MaxPerf: return NVSDK_NGX_PerfQuality_Value_MaxPerf;
				case DLSSQualityValue::UltraPerformance: return NVSDK_NGX_PerfQuality_Value_UltraPerformance;
				case DLSSQualityValue::MaxQuality: return NVSDK_NGX_PerfQuality_Value_MaxQuality;
			}
			BEY_CORE_ASSERT(false);
			return NVSDK_NGX_PerfQuality_Value_DLAA;
		}

		glm::uvec2 GetOptimalSettings(uint32_t targetWidth, uint32_t targetHegiht, DLSSQualityValue quality) override;
		void CreateDLSS(Ref<RenderCommandBuffer> commandBuffer, const DLSSSettings& settings) override;

		void Destroy();
		void Evaluate(const Ref<RenderCommandBuffer>& commandBuffer, uint32_t frame, const glm::vec2& jitter, Timestep timeStep, Ref<Image2D> outputColor,
							 Ref<Image2D> inputColor, Ref<Image2D> hitTImage, Ref<Image2D> exposureImage,
							 Ref<Image2D> motionVectors, Ref<Image2D> depthImage, Ref<Image2D> albedoImage, Ref<Image2D> metalRough, Ref<Image2D> normals) override;

		~VulkanDLSS() override;

	private:
		int m_DLSS_Supported = 0;
		int m_NeedsUpdatedDriver = 0;
		uint32_t m_MinDriverVersionMajor = 0;
		uint32_t m_MinDriverVersionMinor = 0;
		NVSDK_NGX_Result DLSSMode;
		NVSDK_NGX_Parameter* m_Params = nullptr;
		uint32_t m_MaxRenderWidth;
		uint32_t m_MaxRenderHeight;
		uint32_t m_MinRenderWidth;
		uint32_t m_MinRenderHeight;

		uint32_t m_RenderWidth;
		uint32_t m_RenderHeight;
		NVSDK_NGX_Handle* m_Handle;
		uint32_t m_TargetWidth;
		uint32_t m_TargetHeight;
	};
}
