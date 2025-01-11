#pragma once

#include "Beyond/Editor/EditorPanel.h"

namespace Beyond {

	class PhysicsCapturesPanel : public EditorPanel
	{
	public:
		virtual bool OnImGuiRender(bool& isOpen) override;

	};

}
