#pragma once

#include "Beyond/Asset/Asset.h"
#include "Beyond/Renderer/Material.h"

#include <map>

#include "EASTL/map.h"

namespace Beyond {
	class StaticMesh;
	class Mesh;

	class MaterialAsset : public Asset
	{
	public:
		MaterialAsset();
		MaterialAsset(Ref<Material> material);

		glm::vec4& GetAlbedoColor();
		void SetAlbedoColor(const glm::vec4& color);

		float& GetMetalness();
		void SetMetalness(float value);

		float& GetRoughness();
		void SetRoughness(float value);

		float& GetEmission();
		void SetEmission(float value);

		void SetSpecular(float value);
		float& GetSpecular();

		void SetSpecularColor(const glm::vec3& color);
		glm::vec3& GetSpecularColor();

		void SetTransmission(float value);
		float& GetTransmission();

		void SetAttenuationColor(const glm::vec3& color);
		glm::vec3& GetAttenuationColor();

		void SetAttenuationDistance(float distance);
		float& GetAttenuationDistance();

		void SetThickness(float thickness);
		float& GetThickness();

		void SetClearcoat(float value);
		float& GetClearcoat();

		void SetClearcoatRoughness(float value);
		float& GetClearcoatRoughness();

		void SetIridescence(float value);
		float& GetIridescence();

		void SetIridescenceIor(float value);
		float& GetIridescenceIor();

		void SetIridescenceThickness(float value);
		float& GetIridescenceThickness();

		void SetSheenColor(const glm::vec3& color);
		glm::vec3& GetSheenColor();

		void SetSheenRoughness(float value);
		float& GetSheenRoughness();

		float& GetIOR();
		void SetIOR(float value);

		// Textures
		Ref<Texture2D> GetAlbedoMap();
		void SetAlbedoMap(Ref<Texture2D> texture);
		void ClearAlbedoMap();

		Ref<Texture2D> GetNormalMap();
		void SetNormalMap(Ref<Texture2D> texture);
		bool& IsUsingNormalMap();

		void SetUseNormalMap(bool value);
		void ClearNormalMap();

		Ref<Texture2D> GetMetalnessMap();
		void SetMetalnessMap(Ref<Texture2D> texture);
		void ClearMetalnessMap();

		Ref<Texture2D> GetRoughnessMap();
		void SetRoughnessMap(Ref<Texture2D> texture);
		void ClearRoughnessMap();

		void SetClearcoatMap(Ref<Texture2D> texture);
		Ref<Texture2D> GetClearcoatMap();
		void ClearClearcoatMap();

		void SetTransmissionMap(Ref<Texture2D> texture);
		Ref<Texture2D> GetTransmissionMap();
		void ClearTransmissionMap();

		void SetEmissionMap(Ref<Texture2D> texture);
		Ref<Texture2D> GetEmissionMap();
		void ClearEmissionMap();

		bool IsShadowCasting() const { return !m_Material->GetFlag(MaterialFlag::DisableShadowCasting); }
		void SetShadowCasting(bool castsShadows) { return m_Material->SetFlag(MaterialFlag::DisableShadowCasting, !castsShadows); }

		bool IsBlended() const { return m_Material->GetFlag(MaterialFlag::Blend); }
		void SetBlending(bool blend);

		bool IsDepthTested() const { return m_Material->GetFlag(MaterialFlag::DepthTest); }
		void SetIsDepthTested(bool depthTest) { return m_Material->SetFlag(MaterialFlag::DepthTest, depthTest); }

		bool IsTranslucent() const { return m_Material->GetFlag(MaterialFlag::Translucent); }
		void SetTranslucency(bool translucent) { m_Material->SetFlag(MaterialFlag::Translucent, translucent); }

		bool IsTwoSided() const { return m_Material->GetFlag(MaterialFlag::TwoSided); }
		void SetTwoSided(bool twoSided) { m_Material->SetFlag(MaterialFlag::TwoSided, twoSided); }



		static AssetType GetStaticType() { return AssetType::Material; }
		virtual AssetType GetAssetType() const override { return GetStaticType(); }

		Ref<Material> GetMaterial() const { return m_Material; }
		const Material* GetMaterialRaw() const { return m_Material.Raw(); }
		void SetMaterial(Ref<Material> material) { m_Material = material; }

		void SetDefaults();
	private:
	private:
		Ref<Material> m_Material;

		friend class MaterialEditor;
	};

	class MaterialTable : public RefCounted
	{
	public:
		MaterialTable(uint32_t materialCount = 1);
		MaterialTable(Ref<MaterialTable> other);
		~MaterialTable() = default;

		bool HasMaterial(uint32_t materialIndex) const { return m_Materials.find(materialIndex) != m_Materials.end(); }
		void SetMaterial(uint32_t index, AssetHandle material);
		void ClearMaterial(uint32_t index);

		AssetHandle GetMaterial(uint32_t materialIndex) const
		{
			BEY_CORE_ASSERT(HasMaterial(materialIndex));
			return m_Materials.at(materialIndex);
		}
		eastl::map<uint32_t, AssetHandle>& GetMaterials() { return m_Materials; }
		const eastl::map<uint32_t, AssetHandle>& GetMaterials() const { return m_Materials; }

		uint32_t GetMaterialCount() const { return m_MaterialCount; }
		void SetMaterialCount(uint32_t materialCount) { m_MaterialCount = materialCount; }

		void Clear();
	private:
		eastl::map<uint32_t, AssetHandle> m_Materials;
		uint32_t m_MaterialCount;
	};

}
