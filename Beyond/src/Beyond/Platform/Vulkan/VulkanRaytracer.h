#pragma once
#include <memory_resource>

#include "VulkanBLAS.h"
#include "VulkanContext.h"
#include "Beyond/Renderer/Raytracer.h"
#include "Beyond/Renderer/Mesh.h"

namespace Beyond
{
	class VulkanIndexBuffer;
	class VulkanVertexBuffer;
}

namespace Beyond {

	struct BLASKey
	{
		const MeshSource* Mesh;
		bool Translucent;
		bool TwoSided;

		BLASKey(const MeshSource* meshSource, bool translucent, bool twoSided)
			: Mesh(meshSource), Translucent(translucent), TwoSided(twoSided)
		{
		}

		bool operator==(const BLASKey& other) const
		{
			return Mesh == other.Mesh &&
				Translucent == other.Translucent &&
				TwoSided == other.TwoSided;
		}
	};
}

namespace eastl {
	template <>
	struct hash<Beyond::BLASKey>
	{
		size_t operator()(const Beyond::BLASKey& key) const noexcept
		{
			const size_t h1 = eastl::hash<const Beyond::MeshSource*>{}(key.Mesh);
			size_t h2 = (key.Translucent ? 1 : 0) | ((key.TwoSided ? 1 : 0) << 1);
			return h1 ^ (std::hash<size_t>{}(h2) << 1); // Combine the hash values
		}
	};
}

namespace Beyond {


	struct VulkanBufferResources
	{
		Ref<VertexBuffer> VertexBuffer;
		Ref<IndexBuffer> IndexBuffer;
		uint32_t VertexIndex;
		uint32_t IndexIndex;
	};


	class VulkanRaytracer : public Raytracer
	{
	public:
		explicit VulkanRaytracer(const Ref<AccelerationStructureSet> as);

	public:
		void AddDrawCommand(const StaticDrawCommand& dc, const MaterialAsset* material, const glm::mat3x4& transform) override;
		void AddDrawCommand(const DrawCommand& dc, const MaterialAsset* material, const glm::mat3x4& transform) override;

		void AddInstancedDrawCommand(const StaticDrawCommand& dc, const glm::mat3x4& transform) override;

	private:
		void BuildTlas(Ref<RenderCommandBuffer> commandBuffer) override;
		void BuildTlas(Ref<RenderCommandBuffer> commandBuffer, Ref<StorageBuffer> storageBuffer) override;

		const std::vector<MaterialBuffer>& GetMaterials() const override { return m_Materials; }
		const eastl::vector<ObjDesc>& GetObjDescs() const override { return m_objDesc; }
		eastl::vector<ObjDesc>& GetObjDescs() override { return m_objDesc; }

		const eastl::vector<SceneInstance>& GetSceneInstances() const override { return m_SceneInstances; }
		eastl::vector<SceneInstance>& GetSceneInstances()  override { return m_SceneInstances; }

		eastl::vector<VkAccelerationStructureInstanceKHR>& GetVulkanInstances() override { return m_VulkanInstances; }
		const eastl::vector<VkAccelerationStructureInstanceKHR>& GetVulkanInstances() const override { return m_VulkanInstances; }

	public:
		void Clear() override
		{
			m_SceneInstances.clear();
			m_VulkanInstances.clear();
			m_objDesc.clear();
			m_Materials.clear();
			m_DescriptorBufferInfos.clear();
			//m_VulkanBuffers.clear();
			//m_DynamicBufferIndex = 0;
		}

		void ClearInternalInstances() override
		{
			m_VulkanInstances.clear();
		}


	private:
		Ref<MaterialAsset> m_DefaultMaterial;

		eastl::unordered_map<AssetHandle, VulkanBufferResources> m_VulkanBuffers;
		mutable uint32_t m_DynamicBufferIndex = 0;
		eastl::vector<ObjDesc>     m_objDesc;    // Model description for device access
		std::vector<MaterialBuffer>     m_Materials;    // Materials for device access
		eastl::vector<SceneInstance> m_SceneInstances;  // Scene model instances
		eastl::vector<VkAccelerationStructureInstanceKHR> m_VulkanInstances;  // Scene model instances
		eastl::vector<VkDescriptorBufferInfo> m_DescriptorBufferInfos;
		//std::vector<Ref<RefCounted>> m_Buffers;
		eastl::unordered_map<BLASKey, Ref<VulkanBLAS>> m_BLASes;


		Ref<AccelerationStructureSet> m_AccelerationStructureSet;
	};

}


