#include "IrisPCH.h"
#include "SceneRendererPanel.h"

#include "ImGui/ImGuiUtils.h"
#include "Renderer/SceneRenderer.h"
#include "Renderer/StorageBufferSet.h"
#include "Renderer/Text/Font.h"
#include "Renderer/Texture.h"
#include "Renderer/UniformBufferSet.h"

namespace Iris {

	void SceneRendererPanel::OnImGuiRender(bool& isOpen)
	{
		if (ImGui::Begin("Scene Renderer", &isOpen, ImGuiWindowFlags_NoCollapse))
		{
			const SceneRenderer::Statistics& statistics = m_Context->GetStatistics();

			ImGui::PushStyleColor(ImGuiCol_Text, Colors::Theme::TextWarning);
			UI::TextWrapped("These are actually NOT the correct numbers of draws required to render one frame of your scene. Instead they are ONLY the number of color pass draw required to render the color pass ONLY");
			ImGui::PopStyleColor();

			UI::BeginPropertyGrid(2, 210.0f);

			UI::PropertyStringReadOnly("Mesh Count", fmt::format("{}", statistics.Meshes).c_str());
			UI::PropertyStringReadOnly("Instance Count", fmt::format("{}", statistics.Instances).c_str());
			UI::PropertyStringReadOnly("Total Draw Calls", fmt::format("{}", statistics.TotalDrawCalls).c_str());
			UI::PropertyStringReadOnly("Color Pass Draw Calls", fmt::format("{}", statistics.ColorPassDrawCalls).c_str());
			UI::PropertyStringReadOnly("Color Pass Saved Draw Calls", fmt::format("{}", statistics.ColorPassSavedDraws).c_str());

			UI::EndPropertyGrid();

			UI::Image(Font::GetDefaultFont()->GetFontAtlas(), ImGui::GetContentRegionAvail(), {0, 1}, {1, 0});
		}

		ImGui::End();
	}

}