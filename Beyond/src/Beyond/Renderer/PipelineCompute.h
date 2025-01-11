#pragma once

#include "Beyond/Core/Base.h"
#include "Beyond/Renderer/Shader.h"
#include "Beyond/Renderer/RenderCommandBuffer.h"
#include "Beyond/Renderer/StorageBuffer.h"

namespace Beyond {

	class PipelineCompute : public RefCounted
	{
	public:
		virtual void Begin(Ref<RenderCommandBuffer> renderCommandBuffer = nullptr) = 0;
		virtual void RT_Begin(Ref<RenderCommandBuffer> renderCommandBuffer = nullptr) = 0;
		virtual void End() = 0;

		virtual void BufferMemoryBarrier(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<StorageBuffer> storageBuffer, ResourceAccessFlags fromAccess, ResourceAccessFlags toAccess) = 0;
		virtual void BufferMemoryBarrier(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<StorageBuffer> storageBuffer, PipelineStage fromStage, ResourceAccessFlags fromAccess, PipelineStage toStage, ResourceAccessFlags toAccess) = 0;

		virtual void ImageMemoryBarrier(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<Image2D> image, ResourceAccessFlags fromAccess, ResourceAccessFlags toAccess) = 0;
		virtual void ImageMemoryBarrier(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<Image2D> image, PipelineStage fromStage, ResourceAccessFlags fromAccess, PipelineStage toStage, ResourceAccessFlags toAccess) = 0;

		virtual Ref<Shader> GetShader() const = 0;

		static Ref<PipelineCompute> Create(Ref<Shader> computeShader);
	};

}
