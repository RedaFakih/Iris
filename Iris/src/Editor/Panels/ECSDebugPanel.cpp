#include "IrisPCH.h"
#include "ECSDebugPanel.h"

#include "Editor/SelectionManager.h"
#include "Project/Project.h"
#include "Scene/Scene.h"

#include <imgui/imgui.h>

namespace Iris {

	Ref<ECSDebugPanel> ECSDebugPanel::Create(Ref<Scene> context)
	{
		return CreateRef<ECSDebugPanel>(context);
	}

	ECSDebugPanel::ECSDebugPanel(Ref<Scene> context)
		: m_Context(context)
	{
	}

	ECSDebugPanel::~ECSDebugPanel()
	{
		m_Context = nullptr;
	}

	void ECSDebugPanel::OnImGuiRender(bool& open)
	{
		if (ImGui::Begin("ECS Debug", &open, ImGuiWindowFlags_NoCollapse) && m_Context)
		{
			auto view = m_Context->m_Registry.view<IDComponent>();
			ImGui::Text("Entities: %i", view.size());
			for (auto e : view)
			{
				Entity entity = { e, m_Context.Raw() };
				const auto& name = entity.Name();

				std::string label = fmt::format("{0} - {1}", name, entity.GetUUID());

				bool selected = SelectionManager::IsSelected(SelectionContext::Scene, entity.GetUUID());
				if (ImGui::Selectable(label.c_str(), &selected, 0))
					SelectionManager::Select(SelectionContext::Scene, entity.GetUUID());
			}

		}

		ImGui::End();
	}

}