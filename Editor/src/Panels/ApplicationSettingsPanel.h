#pragma once

#include "Beyond/Editor/EditorPanel.h"
#include "Beyond/Renderer/Texture.h"

#include <functional>

namespace Beyond {

	struct SettingsPage
	{
		using PageRenderFunction = std::function<void()>;

		const char* Name;
		PageRenderFunction RenderFunction;
	};

	class ApplicationSettingsPanel : public EditorPanel
	{
	public:
		ApplicationSettingsPanel();
		~ApplicationSettingsPanel();

		virtual bool OnImGuiRender(bool& isOpen) override;

	private:
		void DrawPageList();

		void DrawRendererPage();
		void DrawScriptingPage();
		void DrawEditorPage();
	private:
		uint32_t m_CurrentPage = 0;
		std::vector<SettingsPage> m_Pages;
	};

}
