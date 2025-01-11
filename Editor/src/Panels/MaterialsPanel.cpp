#include "MaterialsPanel.h"

#include "Beyond/ImGui/ImGui.h"

#include "Beyond/Renderer/Renderer.h"
#include "Beyond/Asset/AssetManager.h"
#include "Beyond/Editor/SelectionManager.h"

namespace Beyond {


	template<TextureUsageType TextureKind, bool HasValue = true>
	void RenderMaterialTextureProperty(const char* label, Ref<Material>& material, Ref<MaterialAsset>& materialAsset, const char* uniformName, const char* uniformTexIndex, Ref<Texture2D> checkerboardTexture, bool& changed)
	{
		if (ImGui::CollapsingHeader(label, nullptr, ImGuiTreeNodeFlags_DefaultOpen))
		{

			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 10));

			// Value and Texture Retrieval
			Ref<Texture2D> propertyMap = material->GetBindlessTexture2D(uniformTexIndex);
			bool hasPropertyMap = propertyMap && !propertyMap.EqualsObject(Renderer::GetWhiteTexture()) && propertyMap->GetImage();
			Ref<Texture2D> propertyUITexture = hasPropertyMap ? propertyMap : checkerboardTexture;
			ImGui::BeginColumns(label, 2, ImGuiOldColumnFlags_NoBorder | ImGuiOldColumnFlags_NoResize);
			ImVec2 textureCursorPos = ImGui::GetCursorPos();
			float imageWidth = 64 + textureCursorPos.x / 2.0f;
			ImGui::SetColumnWidth(0, imageWidth);
			UI::Image(propertyUITexture, ImVec2(64, 64));
			//ImGui::NextColumn();
			// Drag-and-Drop Support
			if (ImGui::BeginDragDropTarget())
			{
				if (auto data = ImGui::AcceptDragDropPayload("asset_payload"))
				{
					int count = data->DataSize / sizeof(AssetHandle);
					for (int i = 0; i < count; i++)
					{
						if (count > 1) break;

						AssetHandle assetHandle = *(((AssetHandle*)data->Data) + i);
						Ref<Asset> asset = AssetManager::GetAsset<Asset>(assetHandle);
						if (!asset || (asset->GetAssetType() != AssetType::Texture && !asset.As<Texture2D>()->IsStillLoading()))
							break;

						propertyMap = asset.As<Texture2D>();
						material->Set(uniformTexIndex, propertyMap);
						if constexpr (TextureKind == TextureUsageType::Normal)
						{
							material->Set("u_MaterialUniforms.UseNormalMap", true);
						}
						changed = true;
					}
				}
				ImGui::EndDragDropTarget();
			}

			ImGui::PopStyleVar();

			// Tooltip and Hover Behavior
			if (ImGui::IsItemHovered())
			{
				if (hasPropertyMap)
				{
					ImGui::BeginTooltip();
					ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
					std::string pathString = propertyMap->GetPath().string();
					ImGui::TextUnformatted(pathString.c_str());
					ImGui::PopTextWrapPos();
					UI::Image(propertyUITexture, ImVec2(384, 384));
					ImGui::EndTooltip();
				}

				if (ImGui::IsItemClicked() && ImGui::GetHoveredID() == ImGui::GetActiveID())
				{
					std::string filepath = FileSystem::OpenFileDialog().string();
					if (!filepath.empty())
					{
						TextureSpecification spec{ .CreateBindlessDescriptor = true, .Compress = true, .UsageType = TextureKind };
						if constexpr (TextureKind == TextureUsageType::Albedo)
							spec.Format = ImageFormat::SRGBA;

						AssetHandle textureHandle = AssetManager::CreateMemoryOnlyRendererAsset<Texture2D>(spec, filepath);
						propertyMap = AssetManager::GetAsset<Texture2D>(textureHandle);
						material->Set(uniformTexIndex, propertyMap);
						if constexpr (TextureKind == TextureUsageType::Normal)
						{
							material->Set("u_MaterialUniforms.UseNormalMap", true);
						}
						changed = true;
					}
				}
			}

			// Reset Button
			//ImVec2 nextRowCursorPos = ImGui::GetCursorPos();
			//ImGui::SameLine();
			//ImVec2 properCursorPos = ImGui::GetCursorPos();
			ImGui::SetCursorPos(textureCursorPos);
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
			if (hasPropertyMap && ImGui::Button(("X##" + std::string(label)).c_str(), ImVec2(18, 18)))
			{
				if constexpr (TextureKind == TextureUsageType::Albedo)
					materialAsset->ClearAlbedoMap();
				else if constexpr (TextureKind == TextureUsageType::Normal)
					materialAsset->ClearNormalMap();
				else if constexpr (TextureKind == TextureUsageType::MetalnessRoughness)
					materialAsset->ClearMetalnessMap();
				else if constexpr (TextureKind == TextureUsageType::Roughness)
					materialAsset->ClearRoughnessMap();
				else if constexpr (TextureKind == TextureUsageType::Clearcoat)
					materialAsset->ClearClearcoatMap();


				changed = true;
			}
			ImGui::PopStyleVar();




#define SLIDER_FLOAT_MAT_PROP(GetProperty, label, min, max)									\
			do {																			\
				float& GetProperty = materialAsset->GetProperty();							\
				ImGui::SetNextItemWidth(150.0f);											\
				changed |= (UI::SliderFloat("##"#label, &(GetProperty), min, max));			\
				ImGui::SameLine();															\
				ImGui::TextUnformatted(label);												\
			} while(false)

#define SLIDER_COLOR_MAT_PROP(GetProperty, label, colorCount)								\
			do {																			\
				auto& GetProperty = materialAsset->GetProperty();							\
				ImGui::SetNextItemWidth(150.0f);											\
				changed |= (UI::ColorEdit##colorCount("##"#label, &((GetProperty).x)));		\
				ImGui::SameLine();															\
				ImGui::TextUnformatted(label);												\
			} while (false)

#define CHECKBOX_MAT_PROP(GetProperty, label)												\
			do {																			\
				bool& GetProperty = materialAsset->GetProperty();							\
				ImGui::SetNextItemWidth(150.0f);											\
				changed |= (UI::Checkbox("##"#label, &(GetProperty)));						\
				ImGui::SameLine();															\
				ImGui::TextUnformatted(label);												\
			} while (false)


			ImGui::NextColumn();


			// Color Property (e.g., Albedo Color)
			if constexpr (TextureKind == TextureUsageType::Albedo)
			{
				SLIDER_COLOR_MAT_PROP(GetAlbedoColor, "Albedo Color", 4);
				SLIDER_COLOR_MAT_PROP(GetSheenColor, "Sheen Color", 3);
				SLIDER_FLOAT_MAT_PROP(GetSheenRoughness, "Sheen Roughness", 0.0f, 1.0);
			}

			if constexpr (TextureKind == TextureUsageType::Transmission)
			{
				SLIDER_FLOAT_MAT_PROP(GetTransmission, "Transmission", 0.0f, 1.0f);
				SLIDER_FLOAT_MAT_PROP(GetIOR, "IOR", 1.0f, 50.0f);
				SLIDER_COLOR_MAT_PROP(GetAttenuationColor, "Attenuation Color", 3);
				SLIDER_FLOAT_MAT_PROP(GetAttenuationDistance, "Attenuation Distance", 0.0f, 500.0f);

			}

			if constexpr (TextureKind == TextureUsageType::Emission)
			{
				SLIDER_FLOAT_MAT_PROP(GetEmission, "Emission", 0.0f, 500.0f);
				ImGui::Dummy(ImVec2(0.0f, ImGui::GetFontSize())); // Spacer row
				ImGui::Dummy(ImVec2(0.0f, ImGui::GetFontSize())); // Spacer row
			}

			if constexpr (TextureKind == TextureUsageType::Normal)
			{
				CHECKBOX_MAT_PROP(IsUsingNormalMap, "Use Normal Map");
				ImGui::Dummy(ImVec2(0.0f, ImGui::GetFontSize())); // Spacer row
				ImGui::Dummy(ImVec2(0.0f, ImGui::GetFontSize())); // Spacer row
			}

			if constexpr (TextureKind == TextureUsageType::MetalnessRoughness)
			{
				SLIDER_FLOAT_MAT_PROP(GetMetalness, "Metalness", 0.0f, 1.0f);
				ImGui::Dummy(ImVec2(0.0f, ImGui::GetFontSize())); // Spacer row
				ImGui::Dummy(ImVec2(0.0f, ImGui::GetFontSize())); // Spacer row
			}

			if constexpr (TextureKind == TextureUsageType::Roughness)
			{
				SLIDER_FLOAT_MAT_PROP(GetRoughness, "Roughness", 0.0f, 1.0f);
				SLIDER_FLOAT_MAT_PROP(GetSpecular, "Specular", 0.0f, 1.0f);
				SLIDER_COLOR_MAT_PROP(GetSpecularColor, "Specular Color", 3);
			}

			if constexpr (TextureKind == TextureUsageType::Clearcoat)
			{
				SLIDER_FLOAT_MAT_PROP(GetClearcoat, "Clearcoat", 0.0f, 1.0f);
				SLIDER_FLOAT_MAT_PROP(GetClearcoatRoughness, "Clearcoat Roughness", 0.0f, 1.0f);
				SLIDER_FLOAT_MAT_PROP(GetThickness, "Thickness", 0.0f, 1.0f);
				SLIDER_FLOAT_MAT_PROP(GetIridescence, "Iridescence", 0.0f, 1.0f);
				SLIDER_FLOAT_MAT_PROP(GetIridescenceIor, "Iridescence IOR", 0.0f, 30.0f);
				SLIDER_FLOAT_MAT_PROP(GetIridescenceThickness, "Iridescence Thickness", 0.0f, 30.0f);

			}
			ImGui::EndColumns();
		}
	}



	MaterialsPanel::MaterialsPanel()
	{
		m_CheckerBoardTexture = Texture2D::Create(TextureSpecification(), std::filesystem::path("Resources/Editor/Checkerboard.tga"));
	}

	MaterialsPanel::~MaterialsPanel() {}

	// Helper function to validate if a selected entity has a valid mesh
	bool MaterialsPanel::HasValidMesh(const Entity& entity) const
	{
		return (entity.HasComponent<MeshComponent>() && AssetManager::IsAssetHandleValid(entity.GetComponent<MeshComponent>().MeshAssetHandle)) ||
			(entity.HasComponent<StaticMeshComponent>() && AssetManager::IsAssetHandleValid(entity.GetComponent<StaticMeshComponent>().StaticMeshAssetHandle));
	}

	// Helper function to get material tables based on component type
	std::pair<Ref<MaterialTable>, Ref<MaterialTable>> MaterialsPanel::GetMaterialTables(const Entity& entity)
	{
		Ref<MaterialTable> meshMaterialTable, componentMaterialTable;

		if (entity.HasComponent<MeshComponent>())
		{
			const auto& meshComponent = entity.GetComponent<MeshComponent>();
			componentMaterialTable = meshComponent.MaterialTable;
			auto mesh = AssetManager::GetAsset<Mesh>(meshComponent.MeshAssetHandle);
			if (mesh)
			{
				meshMaterialTable = mesh->GetMaterials();
			}
		}
		else if (entity.HasComponent<StaticMeshComponent>())
		{
			const auto& staticMeshComponent = entity.GetComponent<StaticMeshComponent>();
			componentMaterialTable = staticMeshComponent.MaterialTable;
			auto mesh = AssetManager::GetAsset<StaticMesh>(staticMeshComponent.StaticMeshAssetHandle);
			if (mesh)
			{
				meshMaterialTable = mesh->GetMaterials();
			}
		}

		return { meshMaterialTable, componentMaterialTable };
	}


	void MaterialsPanel::SetSceneContext(const Ref<Scene>& context)
	{
		m_Context = context;
	}

	bool MaterialsPanel::OnImGuiRender(bool& isOpen)
	{
		bool changed = false;
		if (SelectionManager::GetSelectionCount(SelectionContext::Scene) > 0)
		{
			m_SelectedEntity = m_Context->GetEntityWithUUID(SelectionManager::GetSelections(SelectionContext::Scene).front());
		}
		else
		{
			m_SelectedEntity = {};
		}

		const bool hasValidEntity = m_SelectedEntity && (m_SelectedEntity.HasAny<MeshComponent, StaticMeshComponent>());

		ImGui::SetNextWindowSize(ImVec2(200.0f, 300.0f), ImGuiCond_Appearing);
		if (ImGui::Begin("Materials", &isOpen) && hasValidEntity)
		{
			bool hasValidMesh = HasValidMesh(m_SelectedEntity);
			AssetHandle selectedMaterial{};
			if (hasValidMesh)
			{
				auto [meshMaterialTable, componentMaterialTable] = GetMaterialTables(m_SelectedEntity);

				if (componentMaterialTable && meshMaterialTable)
				{
					if (componentMaterialTable->GetMaterialCount() < meshMaterialTable->GetMaterialCount())
						componentMaterialTable->SetMaterialCount(meshMaterialTable->GetMaterialCount());
				}

				{
					if (m_SelectedEntity.HasComponent<MeshComponent>())
					{
						const auto& meshComponent = m_SelectedEntity.GetComponent<MeshComponent>();
						auto mesh = AssetManager::GetAsset<Mesh>(meshComponent.MeshAssetHandle);
						if (mesh)
						{
							const Submesh& submesh = mesh->GetMeshSource()->GetSubmeshes()[meshComponent.SubmeshIndex];
							selectedMaterial = meshMaterialTable->GetMaterial(submesh.MaterialIndex);
						}
					}
				}

				static eastl::string searchedString;
				UI::Widgets::SearchWidget(searchedString);
				for (uint32_t i = 0; i < componentMaterialTable->GetMaterialCount(); i++)
				{
					bool hasComponentMaterial = componentMaterialTable->HasMaterial(i);
					bool hasMeshMaterial = meshMaterialTable && meshMaterialTable->HasMaterial(i);

					if (hasMeshMaterial && !hasComponentMaterial)
					{
						Ref<MaterialAsset> materialAsset = AssetManager::GetAsset<MaterialAsset>(meshMaterialTable->GetMaterial(i));
						auto material = materialAsset->GetMaterial();
						bool open = selectedMaterial == meshMaterialTable->GetMaterial(i);
						if ((open || !AssetManager::IsAssetHandleValid(selectedMaterial)) && UI::IsMatchingSearch(material->GetName().empty() ? "UNNAMED MATERIAL" : material->GetName().c_str(), searchedString))
							changed |= RenderMaterial(i, meshMaterialTable->GetMaterial(i), open);
					}
					else if (hasComponentMaterial)
					{
						Ref<MaterialAsset> materialAsset = AssetManager::GetAsset<MaterialAsset>(componentMaterialTable->GetMaterial(i));
						auto material = materialAsset->GetMaterial();
						if (UI::IsMatchingSearch(material->GetName().empty() ? "UNNAMED MATERIAL" : material->GetName().c_str(), searchedString))
							changed |= RenderMaterial(i, componentMaterialTable->GetMaterial(i));
					}
				}


			}
		}
		ImGui::End();
		return changed;
	}

	bool MaterialsPanel::RenderMaterial(size_t materialIndex, AssetHandle materialAssetHandle, bool open)
	{
		bool changed = false;

		Ref<MaterialAsset> materialAsset = AssetManager::GetAsset<MaterialAsset>(materialAssetHandle);
		auto material = materialAsset->GetMaterial();
		auto shader = material->GetShader();

		Ref<Shader> transparentShader = Renderer::GetShaderLibrary()->Get("PBR_Transparent");
		bool transparent = shader == transparentShader;

		eastl::string name = material->GetName().c_str();
		if (name.empty())
			name = "Unnamed Material";

		name = fmt::format("{0} ({1}", name, material->GetShader()->GetName()).c_str();
		ImGui::PushID(fmt::format("{}#{}", name, materialIndex).c_str());
		if (!UI::PropertyGridHeader(name, materialIndex == 0 || open))
		{
			ImGui::PopID();
			return changed;
		}

		// Flags
		{
			if (UI::PropertyGridHeader("Properties"))
			{
				bool blend = material->GetFlag(MaterialFlag::Blend);
				bool twoSided = material->GetFlag(MaterialFlag::TwoSided);
				bool depthTested = material->GetFlag(MaterialFlag::DepthTest);
				bool ShadowCasting = !material->GetFlag(MaterialFlag::DisableShadowCasting); // Notice the "!"
				bool translucent = material->GetFlag(MaterialFlag::Translucent);
				UI::BeginPropertyGrid();

				if (UI::Property("Blend", blend))
				{
					materialAsset->SetBlending(blend);
					transparent = blend;
					changed = true;
				}
				if (UI::Property("Translucency", translucent))
				{
					materialAsset->SetTranslucency(translucent);
					changed = true;
				}
				if (UI::Property("Two-Sided", twoSided))
				{
					materialAsset->SetTwoSided(twoSided);
					changed = true;
				}
				if (UI::Property("DepthTested", depthTested))
				{
					materialAsset->SetIsDepthTested(depthTested);
					changed = true;
				}
				if (UI::Property("Shadow Casting", ShadowCasting))
				{
					materialAsset->SetShadowCasting(ShadowCasting);
					changed = true;
				}

				UI::EndPropertyGrid();
				UI::EndTreeNode();
			}
		}

		// Textures ------------------------------------------------------------------------------
		RenderMaterialTextureProperty<TextureUsageType::Albedo, false>("Albedo", material, materialAsset, "u_MaterialUniforms.AlbedoColor", "u_MaterialUniforms.AlbedoTexIndex", m_CheckerBoardTexture, changed);
		RenderMaterialTextureProperty<TextureUsageType::Emission>("Emission", material, materialAsset, "u_MaterialUniforms.Emission", "u_MaterialUniforms.EmissionTexIndex", m_CheckerBoardTexture, changed);
		RenderMaterialTextureProperty<TextureUsageType::Transmission>("Transmission", material, materialAsset, "u_MaterialUniforms.Transmission", "u_MaterialUniforms.TransmissionTexIndex", m_CheckerBoardTexture, changed);
		RenderMaterialTextureProperty<TextureUsageType::Normal, false>("Normals", material, materialAsset, "", "u_MaterialUniforms.NormalTexIndex", m_CheckerBoardTexture, changed);
		RenderMaterialTextureProperty<TextureUsageType::MetalnessRoughness>("Metalness", material, materialAsset, "u_MaterialUniforms.Metalness", "u_MaterialUniforms.MetalnessTexIndex", m_CheckerBoardTexture, changed);
		RenderMaterialTextureProperty<TextureUsageType::Roughness>("Roughness", material, materialAsset, "u_MaterialUniforms.Roughness", "u_MaterialUniforms.RoughnessTexIndex", m_CheckerBoardTexture, changed);
		RenderMaterialTextureProperty<TextureUsageType::Clearcoat>("Clearcoat", material, materialAsset, "u_MaterialUniforms.Clearcoat", "u_MaterialUniforms.ClearcoatTexIndex", m_CheckerBoardTexture, changed);

		ImGui::PopID();
		UI::EndTreeNode();
		return changed;
	}

}
