#pragma once

#include "Beyond/Editor/EditorPanel.h"

namespace Beyond {
	class SceneRenderer;

	class SceneRendererPanel : public EditorPanel
	{
	public:
		SceneRendererPanel() = default;
		virtual ~SceneRendererPanel() = default;

		void SetContext(const Ref<SceneRenderer>& context) { m_Context = context; }
		virtual bool OnImGuiRender(bool& isOpen) override;
	private:
		Ref<SceneRenderer> m_Context;
	};

}
