#pragma once
#include "DrawCommands.h"
#include "RenderCommandBuffer.h"
#include <EASTL/vector.h>

namespace Beyond {
	class RaytracingPass;
	class AccelerationStructureSet;

	// Information of a obj model when referenced in a shader
	struct ObjDesc
	{
		uint32_t VertexBufferIndex;
		uint32_t IndexBufferIndex;
		uint32_t FirstVertex;
		uint32_t FirstIndex;

		uint32_t MaterialIndex;
		uint32_t TransformIndex;
		glm::uvec2 Padding;
	};

	struct SceneInstance
	{
		glm::mat3x4 Transform{};    // Matrix of the instance
		uint32_t      ObjIndex{ 0 };  // Model index reference
	};

	struct MaterialBuffer
	{
		// 16-byte aligned members
		glm::vec4 AlbedoColor;

		// 12-byte aligned float3, followed by 4-byte members to avoid padding
		glm::vec3 SpecularColor; // 12 bytes, aligned to 16 bytes
		float Specular;     // 4 bytes (fills the padding after SpecularColor)

		glm::vec3 AttenuationColor;     // 12 bytes, aligned to 16 bytes
		float AttenuationDistance; // 4 bytes (fills the padding after AttenuationColor)

		glm::vec3 SheenColor;      // 12 bytes, aligned to 16 bytes
		float SheenRoughness; // 4 bytes (fills the padding after SheenColor)

		// 8-byte aligned members
		float Roughness; // 8 bytes, aligned to 8 bytes
		float Metalness; // 4 bytes, aligned to 4 bytes
		float Emission;  // 4 bytes, aligned to 4 bytes
		bool UseNormalMap;      // 4 bytes, aligned to 4 bytes

		uint32_t AlbedoTexIndex;    // 4 bytes, aligned to 4 bytes
		uint32_t NormalTexIndex;    // 4 bytes, aligned to 4 bytes
		uint32_t RoughnessTexIndex; // 4 bytes, aligned to 4 bytes
		uint32_t ClearcoatTexIndex;    // 4 bytes, aligned to 4 bytes

		uint32_t TransmissionTexIndex; // 4 bytes, aligned to 4 bytes
		uint32_t MetalnessTexIndex;    // 4 bytes, aligned to 4 bytes
		uint32_t EmissionTexIndex;     // 4 bytes, aligned to 4 bytes
		float IOR;                 // 4 bytes, aligned to 4 bytes

		float Transmission;        // 4 bytes, aligned to 4 bytes
		float Thickness;           // 4 bytes, aligned to 4 bytes
		float Clearcoat;          // 4 bytes, aligned to 4 bytes
		float ClearcoatRoughness; // 4 bytes, aligned to 4 bytes

		float Iridescence;        // 4 bytes, aligned to 4 bytes
		float IridescenceIor;     // 4 bytes, aligned to 4 bytes
		float IridescenceThickness; // 4 bytes, aligned to 4 bytes
		uint32_t Flags;                 // 4 bytes, aligned to 4 bytes
	};

	class Raytracer : public RefCounted
	{
	public:
		virtual void BuildTlas(Ref<RenderCommandBuffer> commandBuffer) = 0;
		virtual void BuildTlas(Ref<RenderCommandBuffer> commandBuffer, Ref<StorageBuffer> storageBuffer) = 0;

		virtual void AddDrawCommand(const StaticDrawCommand& dc, const MaterialAsset* material, const glm::mat3x4& transform) = 0;
		virtual void AddDrawCommand(const DrawCommand& dc, const MaterialAsset* material, const glm::mat3x4& transform) = 0;
		virtual void AddInstancedDrawCommand(const StaticDrawCommand& dc, Ref<RaytracingPass> pass, const glm::mat3x4& transform) = 0;
		virtual const eastl::vector<ObjDesc>& GetObjDescs() const = 0;
		virtual eastl::vector<ObjDesc>& GetObjDescs() = 0;
		virtual const std::vector<MaterialBuffer>& GetMaterials() const = 0;

		virtual const eastl::vector<SceneInstance>& GetSceneInstances() const = 0;
		virtual eastl::vector<SceneInstance>& GetSceneInstances() = 0;

		virtual const eastl::vector<VkAccelerationStructureInstanceKHR>& GetVulkanInstances() const = 0;
		virtual eastl::vector<VkAccelerationStructureInstanceKHR>& GetVulkanInstances() = 0;

		virtual void Clear() = 0;
		virtual void ClearInternalInstances() = 0;

		static Ref<Raytracer> Create(const Ref<AccelerationStructureSet> as);
	};
}

