#pragma once

#include "Beyond/Editor/EditorPanel.h"
#include "Beyond/Scene/Entity.h"
#include "Beyond/Renderer/Texture.h"

namespace Beyond {

	class MaterialsPanel : public EditorPanel
	{
	public:
		MaterialsPanel();
		~MaterialsPanel();
		bool HasValidMesh(const Entity& entity) const;
		std::pair<Ref<MaterialTable>, Ref<MaterialTable>> GetMaterialTables(const Entity& entity);

		virtual void SetSceneContext(const Ref<Scene>& context) override;
		virtual bool OnImGuiRender(bool& isOpen) override;

	private:
		bool RenderMaterial(size_t materialIndex, AssetHandle materialAssetHandle, bool open = false);

	private:
		Ref<Scene> m_Context;
		Entity m_SelectedEntity;
		Ref<Texture2D> m_CheckerBoardTexture;
	};

}
