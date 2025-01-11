#pragma once

#include <cstdlib>
#include "Beyond/Core/Ref.h"
#include "Beyond/Project/Project.h"
#include "Beyond/Core/Events/Event.h"

namespace Beyond {

	class EditorPanel : public RefCounted
	{
	public:
		virtual ~EditorPanel() = default;

		virtual bool OnImGuiRender(bool& isOpen) = 0;
		virtual void OnEvent(Event& e) {}
		virtual void OnProjectChanged(const Ref<Project>& project){}
		virtual void SetSceneContext(const Ref<Scene>& context){}
	};

}
