#pragma once

#include "SelectionManager.h"

#include "EditorPanel.h"

namespace Beyond {

	class ECSDebugPanel : public EditorPanel
	{
	public:
		ECSDebugPanel(Ref<Scene> context);
		~ECSDebugPanel();

		virtual bool OnImGuiRender(bool& open) override;

		virtual void SetSceneContext(const Ref<Scene>& context) { m_Context = context; }
	private:
		Ref<Scene> m_Context;
	};

}
