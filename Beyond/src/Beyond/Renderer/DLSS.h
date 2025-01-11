#pragma once
#include <nvsdk_ngx_defs.h>

namespace Beyond {
	class Image2D;
	class RenderCommandBuffer;
	class Timestep;

	//// Precomputed Halton sequence (base 2, 3)
	//static constexpr glm::vec2 s_HALTON_SEQUENCE[64] = {
	//		glm::vec2(0.000000, 0.000000), glm::vec2(0.500000, 0.333333), glm::vec2(0.250000, 0.666667), glm::vec2(0.750000, 0.111111), glm::vec2(0.125000, 0.444444), glm::vec2(0.625000, 0.777778),
	//		glm::vec2(0.375000, 0.222222), glm::vec2(0.875000, 0.555556), glm::vec2(0.062500, 0.888889), glm::vec2(0.562500, 0.037037), glm::vec2(0.312500, 0.370370), glm::vec2(0.812500, 0.703704),
	//		glm::vec2(0.187500, 0.148148), glm::vec2(0.687500, 0.481481), glm::vec2(0.437500, 0.814815), glm::vec2(0.937500, 0.259259), glm::vec2(0.031250, 0.592593), glm::vec2(0.531250, 0.925926),
	//		glm::vec2(0.281250, 0.074074), glm::vec2(0.781250, 0.407407), glm::vec2(0.156250, 0.740741), glm::vec2(0.656250, 0.185185), glm::vec2(0.406250, 0.518519), glm::vec2(0.906250, 0.851852),
	//		glm::vec2(0.093750, 0.296296), glm::vec2(0.593750, 0.629630), glm::vec2(0.343750, 0.962963), glm::vec2(0.843750, 0.012346), glm::vec2(0.218750, 0.345679), glm::vec2(0.718750, 0.679012),
	//		glm::vec2(0.468750, 0.123457), glm::vec2(0.968750, 0.456790), glm::vec2(0.015625, 0.790123), glm::vec2(0.515625, 0.234568), glm::vec2(0.265625, 0.567901), glm::vec2(0.765625, 0.901235),
	//		glm::vec2(0.140625, 0.049383), glm::vec2(0.640625, 0.382716), glm::vec2(0.390625, 0.716049), glm::vec2(0.890625, 0.160494), glm::vec2(0.078125, 0.493827), glm::vec2(0.578125, 0.827160),
	//		glm::vec2(0.328125, 0.271605), glm::vec2(0.828125, 0.604938), glm::vec2(0.203125, 0.938272), glm::vec2(0.703125, 0.086420), glm::vec2(0.453125, 0.419753), glm::vec2(0.953125, 0.753086),
	//		glm::vec2(0.046875, 0.197531), glm::vec2(0.546875, 0.530864), glm::vec2(0.296875, 0.864198), glm::vec2(0.796875, 0.308642), glm::vec2(0.171875, 0.641975), glm::vec2(0.671875, 0.975309),
	//		glm::vec2(0.421875, 0.024691), glm::vec2(0.921875, 0.358025), glm::vec2(0.109375, 0.691358), glm::vec2(0.609375, 0.135802), glm::vec2(0.359375, 0.469136), glm::vec2(0.859375, 0.802469),
	//		glm::vec2(0.234375, 0.246914), glm::vec2(0.734375, 0.580247), glm::vec2(0.484375, 0.913580), glm::vec2(0.984375, 0.061728) };


	inline glm::vec2 GetCurrentPixelOffset(uint32_t frameIndex)
	{
		// Halton jitter
		glm::vec2 Result(0.0f, 0.0f);

		constexpr int BaseX = 2;
		int Index = frameIndex + 1;
		float InvBase = 1.0f / BaseX;
		float Fraction = InvBase;
		while (Index > 0)
		{
			Result.x += (Index % BaseX) * Fraction;
			Index /= BaseX;
			Fraction *= InvBase;
		}

		constexpr int BaseY = 3;
		Index = frameIndex + 1;
		InvBase = 1.0f / BaseY;
		Fraction = InvBase;
		while (Index > 0)
		{
			Result.y += (Index % BaseY) * Fraction;
			Index /= BaseY;
			Fraction *= InvBase;
		}

		Result.x -= 0.5f;
		Result.y -= 0.5f;
		return Result;
	}

	enum class DLSSQualityValue
	{
		MaxPerf,
		Balanced,
		MaxQuality,
		// Extended PerfQuality modes
		UltraPerformance,
		//UltraQuality,
		DLAA,
	};

	struct DLSSSettings
	{
		bool Enable = true;
		bool EnableJitter = true;
		bool IsHDR = true;
		DLSSQualityValue Mode = DLSSQualityValue::MaxQuality;
		bool FakeDLSS = false;
		uint32_t BasePhases = 8;
		bool UseQuadrants = false;
		uint32_t Quadrant = 4;
		bool JitteredMotionVectors = false;
	};

	class DLSS : public RefCounted
	{
	public:
		static Ref<DLSS> Create();
		virtual bool CheckSupport() = 0;
		virtual glm::uvec2 GetOptimalSettings(uint32_t targetWidth, uint32_t targetHegiht, DLSSQualityValue quality) = 0;
		virtual void CreateDLSS(Ref<RenderCommandBuffer> commandBuffer, const DLSSSettings& settings) = 0;

		virtual void Evaluate(const Ref<RenderCommandBuffer>& commandBuffer, uint32_t frame, const glm::vec2& jitter, Timestep timeStep, Ref<Image2D> outputColor,
					 Ref<Image2D> inputColor, Ref<Image2D> hitTImage, Ref<Image2D> exposureImage,
					 Ref<Image2D> motionVectors, Ref<Image2D> depthImage, Ref<Image2D> albedoImage, Ref<Image2D> metalRough, Ref<Image2D> normals) = 0;
	};

}

