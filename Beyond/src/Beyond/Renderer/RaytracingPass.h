#pragma once
#include "RaytracingPipeline.h"
#include "AccelerationStructureSet.h"
#include "RandomGenerator.h"
#include "StorageBufferSet.h"
#include "UniformBufferSet.h"
#include "Texture.h"

namespace Beyond {

	struct RaytracingPassSpecification
	{
		Ref<RaytracingPipeline> Pipeline;
		eastl::string DebugName;
		glm::vec4 MarkerColor{ RandomGen::s_RandomGen.GetVec4InRange(0.1f, 0.9f) };
	};

	class RaytracingPass : public RefCounted
	{
	public:
		virtual ~RaytracingPass() override = default;

		virtual RaytracingPassSpecification& GetSpecification() = 0;
		virtual const RaytracingPassSpecification& GetSpecification() const = 0;

		virtual Ref<Shader> GetShader() const = 0;

		virtual void SetInput(const eastl::string& name, Ref<UniformBufferSet> uniformBufferSet) = 0;
		virtual void SetInput(const eastl::string& name, Ref<UniformBuffer> uniformBuffer) = 0;
		virtual void SetInput(const eastl::string& name, Ref<StorageBufferSet> storageBufferSet) = 0;
		virtual void SetInput(const eastl::string& name, Ref<StorageBuffer> storageBuffer) = 0;
		virtual void SetInput(const eastl::string& name, Ref<AccelerationStructureSet> accelerationStructureSet) = 0;
		virtual void SetInput(const eastl::string& name, Ref<TLAS> accelerationStructure) = 0;
		virtual void SetInput(const eastl::string& name, Ref<Texture2D> texture) = 0;
		virtual void SetInput(const eastl::string& name, Ref<TextureCube> textureCube) = 0;
		virtual void SetInput(const eastl::string& name, Ref<Image2D> image) = 0;
		virtual void SetInput(const eastl::string& name, Ref<Sampler> sampler) = 0;
		virtual void SetInput(const eastl::string& name, Ref<Sampler> sampler, uint32_t index) = 0;

		virtual Ref<Image2D> GetOutput(uint32_t index) = 0;
		virtual Ref<Image2D> GetDepthOutput() = 0;
		virtual bool HasDescriptorSets() const = 0;
		virtual uint32_t GetFirstSetIndex() const = 0;

		virtual bool Validate() = 0;
		virtual void Bake() = 0;
		virtual bool Baked() const = 0;
		virtual void Prepare() = 0;

		virtual Ref<RaytracingPipeline> GetPipeline() const = 0;

		static Ref<RaytracingPass> Create(const RaytracingPassSpecification& spec);
	};

}
