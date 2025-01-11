#pragma once

#include "Beyond/Core/Log.h"
#include "Beyond/Project/Project.h"

#include "Beyond/Editor/EditorPanel.h"

namespace Beyond {

	class ProjectSettingsWindow : public EditorPanel
	{
	public:
		ProjectSettingsWindow();
		~ProjectSettingsWindow();

		virtual bool OnImGuiRender(bool& isOpen) override;
		virtual void OnProjectChanged(const Ref<Project>& project) override;

	private:
		void RenderGeneralSettings();
		void RenderRendererSettings();
		void RenderAudioSettings();
		void RenderScriptingSettings();
		void RenderPhysicsSettings();
		void RenderLogSettings();
	private:
		Ref<Project> m_Project;
		AssetHandle m_DefaultScene;
		int32_t m_SelectedLayer = -1;
		char m_NewLayerNameBuffer[255];

	};

}
