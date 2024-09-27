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

			if (UI::PropertyGridHeader("Shadows"))
			{
				SceneRenderer::UBRendererData& rendererData = m_Context->m_RendererDataUB;

				UI::BeginPropertyGrid();

				UI::Property("Soft Shadows", rendererData.SoftShadows, "Set between soft or hard shadow edges");
				UI::Property("Light Size", rendererData.LightSize, 0.01f, 0.0f, 10.0f, "Changes the softness of the shadows");
				UI::Property("Max Distance", rendererData.MaxShadowDistance, 1.0f, 0.0f, FLT_MAX, "Max distance that shadows will appear within");
				UI::Property("Shadow Fade", rendererData.ShadowFade, 1.0f, 0.0f, 100.0f, "Fade of shadows near the max distance");

				UI::EndPropertyGrid();

				if (ImGui::TreeNodeEx("Cascade Settings", ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_FramePadding))
				{
					UI::BeginPropertyGrid();

					UI::Property("Show Cascades", rendererData.ShowCascades, "Visualize shadow cascades");
					UI::Property("Fading", rendererData.CascadeFading, "Fade between cascades");
					UI::Property("Transition Fade", rendererData.CascadeTransitionFade, 0.05f, 0.0f, FLT_MAX, "Change the amount of fading between cascades");
					UI::Property("Split Lambda", m_Context->m_CascadeSplitLambda, 0.01f, 0.0f, 10.0f, "Lower values concentrates more detail in the near region in the\ncamera's view frustum. While a larger value spreads detail more evenly.\nA value of 0 results in linear splits while a value of 1 result in logarithmic splits.");
					UI::Property("Near Plane Offset", m_Context->m_CascadeNearPlaneOffset, 0.1f, -FLT_MAX, 0.0f);
					UI::Property("Far Plane Offset", m_Context->m_CascadeFarPlaneOffset, 0.1f, 0.0f, FLT_MAX);
					UI::Property("Scale To Origin", m_Context->m_ScaleShadowCascadesToOrigin, 0.1f, 0.0f, 1.0f, "Higher value makes shadows around scene origin higher quality");

					UI::EndPropertyGrid();

					ImGui::TreePop();
				}

				if (ImGui::TreeNodeEx("Shadow Map", ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_FramePadding))
				{
					Ref<Texture2D> image = m_Context->m_DirectionalShadowPasses[0]->GetTargetFramebuffer()->GetDepthImage();

					static uint32_t cascadeIndex = 0;
					float size = ImGui::GetContentRegionAvail().x;
					UI::BeginPropertyGrid();
					UI::PropertySlider("Cascade Index", cascadeIndex, 0, m_Context->GetSpecification().NumberOfShadowCascades - 1);
					UI::EndPropertyGrid();
					if (m_Context->m_ResourcesCreated)
					{
						UI::Image(image, cascadeIndex, { size, size }, { 0, 1 }, { 1, 0 });
					}

					ImGui::TreePop();
				}

				ImGui::TreePop();
			}

			if (UI::PropertyGridHeader("Depth of Field"))
			{
				SceneRendererOptions& rendererOptions = m_Context->m_Options;

				UI::BeginPropertyGrid();

				UI::Property("Enabled", rendererOptions.DOFEnabled, "Enable/Disable Depth of Field effect");
				UI::Property("Focus Distance", rendererOptions.DOFFocusDistance, 0.1f, 0.0f, std::numeric_limits<float>::max(), "Change the distance of the focus point.");
				UI::Property("Blur Size", rendererOptions.DOFBlurSize, 0.025f, 0.0f, 20.0f, "Change the amount of blur.");

				UI::EndPropertyGrid();

				ImGui::TreePop();
			}
		}

		ImGui::End();
	}

}