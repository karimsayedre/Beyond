#include "pch.h"
#include "MaterialAsset.h"

#include <any>

#include "Beyond/Asset/AssetManager.h"
#include "Beyond/Core/Events/MaterialEvent.h"
#include "Beyond/Renderer/Renderer.h"

namespace Beyond {

	MaterialAsset::MaterialAsset()
	{
		Handle = {};
		m_Material = Material::Create(Renderer::GetShaderLibrary()->Get("PBR_Static"));
		SetDefaults();
	}

	MaterialAsset::MaterialAsset(Ref<Material> material)
	{
		Handle = {};
		m_Material = Material::Copy(material);
	}

	void MaterialAsset::SetDefaults()
	{
		SetAlbedoColor(glm::vec4(0.8f, 0.8f, 0.8f, 1.0f));
		SetEmission(0.0f);
		SetUseNormalMap(false);
		SetMetalness(0.0f);
		SetRoughness(0.4f);
		SetIOR(1.0f);
		SetSpecular(0.0f);
		SetSpecularColor(glm::vec3(1.0f));
		SetTransmission(0.0f);
		SetAttenuationColor(glm::vec3(1.0f));
		SetAttenuationDistance(1.0f);
		SetThickness(1.0f);
		SetClearcoat(0.f);
		SetClearcoatRoughness(0.0f);
		SetIridescence(0.0f);
		SetIridescenceIor(1.5f);
		SetIridescenceThickness(0.0f);
		SetSheenColor(glm::vec3(0.0f));
		SetSheenRoughness(0.0f);

		SetBlending(false);
		SetIsDepthTested(true);
		SetShadowCasting(true);
		SetTranslucency(false);
		SetTwoSided(false);

		SetAlbedoMap(Renderer::GetWhiteTexture());
		SetNormalMap(Renderer::GetWhiteTexture());
		SetMetalnessMap(Renderer::GetWhiteTexture());
		SetRoughnessMap(Renderer::GetWhiteTexture());
		SetClearcoatMap(Renderer::GetWhiteTexture());
		SetTransmissionMap(Renderer::GetWhiteTexture());
		SetEmissionMap(Renderer::GetWhiteTexture());
	}

	void MaterialAsset::SetAlbedoColor(const glm::vec4& color)
	{
		m_Material->Set("u_MaterialUniforms.AlbedoColor", color);
	}

	glm::vec4& MaterialAsset::GetAlbedoColor()
	{
		return m_Material->GetVector4("u_MaterialUniforms.AlbedoColor");
	}

	void MaterialAsset::SetMetalness(float value)
	{
		m_Material->Set("u_MaterialUniforms.Metalness", value);
	}

	float& MaterialAsset::GetMetalness()
	{
		return m_Material->GetFloat("u_MaterialUniforms.Metalness");
	}

	void MaterialAsset::SetRoughness(float value)
	{
		m_Material->Set("u_MaterialUniforms.Roughness", value);
	}

	float& MaterialAsset::GetRoughness()
	{
		return m_Material->GetFloat("u_MaterialUniforms.Roughness");
	}

	void MaterialAsset::SetEmission(float value)
	{
		m_Material->Set("u_MaterialUniforms.Emission", value);
	}

	float& MaterialAsset::GetEmission()
	{
		return m_Material->GetFloat("u_MaterialUniforms.Emission");
	}

	void MaterialAsset::SetIOR(float value)
	{
		m_Material->Set("u_MaterialUniforms.IOR", value);
	}

	float& MaterialAsset::GetIOR()
	{
		return m_Material->GetFloat("u_MaterialUniforms.IOR");
	}

	void MaterialAsset::SetSpecular(float value)
	{
		m_Material->Set("u_MaterialUniforms.Specular", value);
	}

	float& MaterialAsset::GetSpecular()
	{
		return m_Material->GetFloat("u_MaterialUniforms.Specular");
	}

	void MaterialAsset::SetSpecularColor(const glm::vec3& color)
	{
		m_Material->Set("u_MaterialUniforms.SpecularColor", color);
	}

	glm::vec3& MaterialAsset::GetSpecularColor()
	{
		return m_Material->GetVector3("u_MaterialUniforms.SpecularColor");
	}

	void MaterialAsset::SetTransmission(float value)
	{
		m_Material->Set("u_MaterialUniforms.Transmission", value);
	}

	float& MaterialAsset::GetTransmission()
	{
		return m_Material->GetFloat("u_MaterialUniforms.Transmission");
	}

	void MaterialAsset::SetAttenuationColor(const glm::vec3& color)
	{
		m_Material->Set("u_MaterialUniforms.AttenuationColor", color);
	}

	glm::vec3& MaterialAsset::GetAttenuationColor()
	{
		return m_Material->GetVector3("u_MaterialUniforms.AttenuationColor");
	}

	void MaterialAsset::SetAttenuationDistance(float distance)
	{
		m_Material->Set("u_MaterialUniforms.AttenuationDistance", distance);
	}

	float& MaterialAsset::GetAttenuationDistance()
	{
		return m_Material->GetFloat("u_MaterialUniforms.AttenuationDistance");
	}

	void MaterialAsset::SetThickness(float thickness)
	{
		m_Material->Set("u_MaterialUniforms.Thickness", thickness);
	}

	float& MaterialAsset::GetThickness()
	{
		return m_Material->GetFloat("u_MaterialUniforms.Thickness");
	}

	void MaterialAsset::SetClearcoat(float value)
	{
		m_Material->Set("u_MaterialUniforms.Clearcoat", value);
	}

	float& MaterialAsset::GetClearcoat()
	{
		return m_Material->GetFloat("u_MaterialUniforms.Clearcoat");
	}

	void MaterialAsset::SetClearcoatRoughness(float value)
	{
		m_Material->Set("u_MaterialUniforms.ClearcoatRoughness", value);
	}

	float& MaterialAsset::GetClearcoatRoughness()
	{
		return m_Material->GetFloat("u_MaterialUniforms.ClearcoatRoughness");
	}

	void MaterialAsset::SetIridescence(float value)
	{
		m_Material->Set("u_MaterialUniforms.Iridescence", value);
	}

	float& MaterialAsset::GetIridescence()
	{
		return m_Material->GetFloat("u_MaterialUniforms.Iridescence");
	}

	void MaterialAsset::SetIridescenceIor(float value)
	{
		m_Material->Set("u_MaterialUniforms.IridescenceIor", value);
	}

	float& MaterialAsset::GetIridescenceIor()
	{
		return m_Material->GetFloat("u_MaterialUniforms.IridescenceIor");
	}

	void MaterialAsset::SetIridescenceThickness(float value)
	{
		m_Material->Set("u_MaterialUniforms.IridescenceThickness", value);
	}

	float& MaterialAsset::GetIridescenceThickness()
	{
		return m_Material->GetFloat("u_MaterialUniforms.IridescenceThickness");
	}

	void MaterialAsset::SetSheenColor(const glm::vec3& color)
	{
		m_Material->Set("u_MaterialUniforms.SheenColor", color);
	}

	glm::vec3& MaterialAsset::GetSheenColor()
	{
		return m_Material->GetVector3("u_MaterialUniforms.SheenColor");
	}

	void MaterialAsset::SetSheenRoughness(float value)
	{
		m_Material->Set("u_MaterialUniforms.SheenRoughness", value);
	}

	float& MaterialAsset::GetSheenRoughness()
	{
		return m_Material->GetFloat("u_MaterialUniforms.SheenRoughness");
	}

	void MaterialAsset::SetBlending(bool blend)
	{
		Ref<Shader> newShader = blend ? Renderer::GetShaderLibrary()->Get("PBR_Transparent") : Renderer::GetShaderLibrary()->Get("PBR_Static");
		if (m_Material->GetShader() != newShader)
			m_Material->SetShader(newShader);
		m_Material->SetFlag(MaterialFlag::Blend, blend);
	}

	Ref<Texture2D> MaterialAsset::GetAlbedoMap()
	{
		return m_Material->GetBindlessTexture2D("u_MaterialUniforms.AlbedoTexIndex");
	}

	void MaterialAsset::SetAlbedoMap(Ref<Texture2D> texture)
	{
		m_Material->Set("u_MaterialUniforms.AlbedoTexIndex", texture);
	}

	void MaterialAsset::ClearAlbedoMap()
	{
		m_Material->Set("u_MaterialUniforms.AlbedoTexIndex", Renderer::GetWhiteTexture());
	}

	Ref<Texture2D> MaterialAsset::GetNormalMap()
	{
		return m_Material->GetBindlessTexture2D("u_MaterialUniforms.NormalTexIndex");
	}

	void MaterialAsset::SetNormalMap(Ref<Texture2D> texture)
	{
		m_Material->Set("u_MaterialUniforms.NormalTexIndex", texture);
	}

	bool& MaterialAsset::IsUsingNormalMap()
	{
		return m_Material->GetBool("u_MaterialUniforms.UseNormalMap");
	}

	void MaterialAsset::SetUseNormalMap(bool value)
	{
		m_Material->Set("u_MaterialUniforms.UseNormalMap", value);
	}

	void MaterialAsset::ClearNormalMap()
	{
		m_Material->Set("u_MaterialUniforms.NormalTexIndex", Renderer::GetWhiteTexture());
	}

	Ref<Texture2D> MaterialAsset::GetMetalnessMap()
	{
		return m_Material->GetBindlessTexture2D("u_MaterialUniforms.MetalnessTexIndex");
	}

	void MaterialAsset::SetMetalnessMap(Ref<Texture2D> texture)
	{
		m_Material->Set("u_MaterialUniforms.MetalnessTexIndex", texture);
	}

	void MaterialAsset::ClearMetalnessMap()
	{
		m_Material->Set("u_MaterialUniforms.MetalnessTexIndex", Renderer::GetWhiteTexture());
	}

	Ref<Texture2D> MaterialAsset::GetRoughnessMap()
	{
		return m_Material->GetBindlessTexture2D("u_MaterialUniforms.RoughnessTexIndex");
	}

	void MaterialAsset::SetRoughnessMap(Ref<Texture2D> texture)
	{
		m_Material->Set("u_MaterialUniforms.RoughnessTexIndex", texture);
	}

	void MaterialAsset::ClearRoughnessMap()
	{
		m_Material->Set("u_MaterialUniforms.RoughnessTexIndex", Renderer::GetWhiteTexture());
	}

	Ref<Texture2D> MaterialAsset::GetClearcoatMap()
	{
		return m_Material->GetBindlessTexture2D("u_MaterialUniforms.ClearcoatTexIndex");
	}

	void MaterialAsset::SetClearcoatMap(Ref<Texture2D> texture)
	{
		m_Material->Set("u_MaterialUniforms.ClearcoatTexIndex", texture);
	}

	void MaterialAsset::ClearClearcoatMap()
	{
		m_Material->Set("u_MaterialUniforms.ClearcoatTexIndex", Renderer::GetWhiteTexture());
	}

	void MaterialAsset::SetTransmissionMap(Ref<Texture2D> texture)
	{
		m_Material->Set("u_MaterialUniforms.TransmissionTexIndex", texture);

	}

	Ref<Texture2D> MaterialAsset::GetTransmissionMap()
	{
		return m_Material->GetBindlessTexture2D("u_MaterialUniforms.TransmissionTexIndex");
	}

	void MaterialAsset::ClearTransmissionMap()
	{
		m_Material->Set("u_MaterialUniforms.TransmissionTexIndex", Renderer::GetWhiteTexture());
	}

	void MaterialAsset::SetEmissionMap(Ref<Texture2D> texture)
	{
		m_Material->Set("u_MaterialUniforms.EmissionTexIndex", texture);
	}

	Ref<Texture2D> MaterialAsset::GetEmissionMap()
	{
		return m_Material->GetBindlessTexture2D("u_MaterialUniforms.EmissionTexIndex");
	}

	void MaterialAsset::ClearEmissionMap()
	{
		m_Material->Set("u_MaterialUniforms.EmissionTexIndex", Renderer::GetWhiteTexture());

	}

	MaterialTable::MaterialTable(uint32_t materialCount)
		: m_MaterialCount(materialCount)
	{
	}

	MaterialTable::MaterialTable(Ref<MaterialTable> other)
		: m_MaterialCount(other->m_MaterialCount)
	{
		for (auto [index, materialAsset] : other->GetMaterials())
		{
			m_Materials[index] = materialAsset;
			if (index >= m_MaterialCount)
				m_MaterialCount = index + 1;
		}
	}

	void MaterialTable::SetMaterial(uint32_t index, AssetHandle materialHandle)
	{
		m_Materials[index] = materialHandle;
		if (index >= m_MaterialCount)
			m_MaterialCount = index + 1;
	}

	void MaterialTable::ClearMaterial(uint32_t index)
	{
		BEY_CORE_ASSERT(HasMaterial(index));
		m_Materials.erase(index);
		if (index >= m_MaterialCount)
			m_MaterialCount = index + 1;
	}

	void MaterialTable::Clear()
	{
		m_Materials.clear();
	}

}
