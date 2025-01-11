#pragma once

#include "Beyond/Editor/EditorPanel.h"

namespace Beyond {

	class AssetManagerPanel : public EditorPanel
	{
	public:
		AssetManagerPanel() = default;
		virtual ~AssetManagerPanel() = default;

		virtual bool OnImGuiRender(bool& isOpen) override;
	};

}
