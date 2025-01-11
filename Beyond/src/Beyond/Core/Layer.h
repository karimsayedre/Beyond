#pragma once

#include "Beyond/Core/Events/Event.h"
#include "Beyond/Core/TimeStep.h"

#include <string>

namespace Beyond {

	class Layer
	{
	public:
		Layer(const eastl::string& name = "Layer");
		virtual ~Layer();

		virtual void OnAttach() {}
		virtual void OnDetach() {}
		virtual void OnUpdate(Timestep ts) {}
		virtual bool OnImGuiRender() { return false; }
		virtual void OnEvent(Event& event) {}

		inline const eastl::string& GetName() const { return m_DebugName; }
	protected:
		eastl::string m_DebugName;
	};

}
