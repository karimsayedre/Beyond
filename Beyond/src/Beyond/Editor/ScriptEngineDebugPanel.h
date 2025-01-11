#pragma once

#include "Beyond/Script/ScriptEngine.h"

#include "EditorPanel.h"

namespace Beyond {

	class ScriptEngineDebugPanel : public EditorPanel
	{
	public:
		ScriptEngineDebugPanel();
		~ScriptEngineDebugPanel();

		virtual void OnProjectChanged(const Ref<Project>& project) override;
		virtual bool OnImGuiRender(bool& open) override;
	};

}
