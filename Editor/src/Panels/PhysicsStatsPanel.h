#pragma once

#include "Beyond/Editor/EditorPanel.h"
#include "Beyond/Physics/PhysicsScene.h"

namespace Beyond {

	class PhysicsStatsPanel : public EditorPanel
	{
	public:
		PhysicsStatsPanel();
		~PhysicsStatsPanel();

		virtual void SetSceneContext(const Ref<Scene>& context) override;
		virtual bool OnImGuiRender(bool& isOpen) override;

	private:
		Ref<PhysicsScene> m_PhysicsScene;
	};

}
