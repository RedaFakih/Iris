#pragma once

#include "Events/Events.h"
#include "TimeStep.h"

#include <string>

namespace Iris {

	class Layer
	{
	public:
		Layer(const std::string& name = "Layer")
			: m_DebugName(name) {}
		virtual ~Layer() {}

		virtual void OnAttach() {}
		virtual void OnDetach() {}
		virtual void OnUpdate(TimeStep ts) { (void)ts; }
		virtual void OnImGuiRender() {}
		virtual void OnEvent(Events::Event& event) { (void)event; }

		inline const std::string& GetName() const { return m_DebugName; }

	protected:
		std::string m_DebugName;

	};

}