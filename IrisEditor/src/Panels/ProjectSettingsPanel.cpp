#include "ProjectSettingsPanel.h"

#include "ImGui/ImGuiUtils.h"
#include "Project/Project.h"
#include "Project/ProjectSerializer.h"
#include "Renderer/Renderer.h"
#include "Renderer/StorageBufferSet.h"
#include "Renderer/Texture.h"
#include "Renderer/UniformBufferSet.h"

namespace Iris {

	// TODO: Physics Settings

	void ProjectSettingsPanel::OnImGuiRender(bool& isOpen)
	{
		if (!m_Context)
		{
			isOpen = false;
			return;
		}

		bool serializeProject = false;

		if (ImGui::Begin("Project Settings", &isOpen, ImGuiWindowFlags_NoCollapse))
		{
			// General Settings
			{
				ImGui::PushID("General Project Settings");

				if (UI::PropertyGridHeader("General"))
				{
					UI::BeginPropertyGrid(2, 200.0f);

					ImGui::BeginDisabled();

					UI::PropertyStringReadOnly("Name", m_Context->m_Config.Name.c_str());
					UI::PropertyStringReadOnly("Project File Name", m_Context->m_Config.ProjectFileName.c_str());
					UI::PropertyStringReadOnly("Asset Directory", m_Context->m_Config.AssetDirectory.c_str());
					UI::PropertyStringReadOnly("Asset Registry Path", m_Context->m_Config.AssetRegistryPath.c_str());
					UI::PropertyStringReadOnly("Mesh Path", m_Context->m_Config.MeshPath.c_str());
					UI::PropertyStringReadOnly("Mesh Source Path", m_Context->m_Config.MeshSourcePath.c_str());
					UI::PropertyStringReadOnly("Project Directory", m_Context->m_Config.ProjectDirectory.c_str());

					ImGui::EndDisabled();

					if (UI::Property("Auto-save active scene", m_Context->m_Config.EnableAutoSave))
						serializeProject = true;

					// 5 minute to 2 hours allowed auto save interval
					if (UI::Property("Auto-save interval (seconds)", m_Context->m_Config.AutoSaveIntervalSeconds, 1, 300, 7200))
						serializeProject = true;

					if (UI::Property("Save Scene on Editor close", m_Context->m_Config.EnableSceneSaveOnEditorClose))
						serializeProject = true;

					if (UI::PropertyAssetReference<Scene>("Startup Scene", m_StartScene))
					{
						const AssetMetaData& metaData = Project::GetEditorAssetManager()->GetMetaData(m_StartScene);
						if (metaData.IsValid())
						{
							m_Context->m_Config.StartScene = metaData.FilePath.string();
							serializeProject = true;
						}
					}
					
					UI::EndPropertyGrid();
					ImGui::TreePop(); // For the property grid header
				}

				ImGui::PopID();
			}

			// TODO: Physics Settings

			// Renderer Settings
			{
				ImGui::PushID("Renderer Settings");

				if (UI::PropertyGridHeader("Renderer", false))
				{
					bool serializeRendererConfig = false;

					UI::BeginPropertyGrid(2, 250.0f);

					RendererConfiguration& rendererConfig = Renderer::GetConfig();

					if (UI::Property("Create HDR Environment Maps", rendererConfig.ComputeEnvironmentMaps, "Uncheck this to disable compute HDR environment maps in your scenes"))
						serializeRendererConfig = true;

					ImGui::BeginDisabled(!rendererConfig.ComputeEnvironmentMaps);

					static const char* mapSize[6] = { "128", "256", "512", "1024", "2048", "4096" };
					int currentEnvMapSize = static_cast<int>(glm::log2(static_cast<float>(rendererConfig.EnvironmentMapResolution))) - 7;
					if (UI::PropertyDropdown("Environment Map Size", mapSize, 6, &currentEnvMapSize, "Size of the generated environment map texture"))
					{
						rendererConfig.EnvironmentMapResolution = static_cast<uint32_t>(glm::pow(2, currentEnvMapSize + 7));

						serializeRendererConfig = true;
					}

					int currentIrradianceMapSamples = static_cast<int>(glm::log2(static_cast<float>(rendererConfig.IrradianceMapComputeSamples))) - 7;
					if (UI::PropertyDropdown("Irradiance Map Compute Samples", mapSize, 6, &currentIrradianceMapSamples))
					{
						rendererConfig.IrradianceMapComputeSamples = static_cast<uint32_t>(glm::pow(2, currentEnvMapSize + 7));

						serializeRendererConfig = true;
					}

					ImGui::EndDisabled();

					UI::PropertySlider("Max Point Lights Limit", rendererConfig.MaxNumberOfPointLights, 1, 500, "Sets the maximum number of Point Lights allowed in the scene.");
					if (ImGui::IsItemDeactivatedAfterEdit())
					{
						Renderer::AddGlobalShaderMacro("IR_MAX_POINT_LIGHT_COUNT", fmt::format("{}", rendererConfig.MaxNumberOfPointLights));

						serializeRendererConfig = true;
					}

					UI::PropertySlider("Max Spot Lights Limit", rendererConfig.MaxNumberOfSpotLights, 1, 500, "Sets the maximum number of Spot Lights allowed in the scene.");
					if (ImGui::IsItemDeactivatedAfterEdit())
					{
						Renderer::AddGlobalShaderMacro("IR_MAX_SPOT_LIGHT_COUNT", fmt::format("{}", rendererConfig.MaxNumberOfSpotLights));
						
						serializeRendererConfig = true;
					}

					if (UI::PropertyColor4("Viewport Outline Color", m_Context->m_Config.ViewportSelectionOutlineColor))
						serializeProject = true;

					if (UI::PropertyColor4("2D Collider Debug Color", m_Context->m_Config.Viewport2DColliderOutlineColor))
						serializeProject = true;

					if (UI::PropertyColor4("Simple 3D Collider Debug Color", m_Context->m_Config.ViewportSimple3DColliderOutlineColor))
						serializeProject = true;

					if (UI::PropertyColor4("Complex 3D Collider Debug Color", m_Context->m_Config.ViewportComplex3DColliderOutlineColor))
						serializeProject = true;

					if (serializeRendererConfig)
						RendererConfigurationSerialzier::Serialize(rendererConfig);

					UI::EndPropertyGrid();
					ImGui::TreePop();
				}

				ImGui::PopID();
			}

			// Logging Settings
			{
				ImGui::PushID("Logging Settings");

				if (UI::PropertyGridHeader("Logging", false))
				{
					UI::BeginPropertyGrid(3, 0.0f, false);

					std::map<std::string, Logging::Log::TagDetails>& tags = Logging::Log::GetEnabledTags();
					for (auto& [name, details] : tags)
					{
						// Unnamed tags will not be displayed
						if (!name.empty())
						{
							ImGui::PushID(name.c_str());

							if (UI::Property(name.c_str(), details.Enabled, "", true))
								serializeProject = true;

							static const char* levelStrings[6] = { "Trace", "Info", "Warn", "Error", "Fatal", "Debug" };
							int currentLevel = static_cast<int>(details.LevelFilter);
							if (UI::PropertyDropdownNoLabel("LevelFilter", levelStrings, 6, &currentLevel))
							{
								details.LevelFilter = static_cast<Logging::Log::Level>(currentLevel);
								serializeProject = true;
							}

							ImGui::PopID();
						}
					}

					UI::EndPropertyGrid();
					ImGui::TreePop();
				}

				ImGui::PopID();
			}
		}

		ImGui::End();

		if (serializeProject)
			ProjectSerializer::Serialize(m_Context, fmt::format("{}/{}", m_Context->m_Config.ProjectDirectory, m_Context->m_Config.ProjectFileName));
	}

	void ProjectSettingsPanel::OnProjectChanged(const Ref<Project>& project)
	{
		m_Context = project;
		m_StartScene = Project::GetEditorAssetManager()->GetAssetHandleFromFilePath(project->GetAssetDirectory().string() + "/" + project->GetConfig().StartScene);
	}

}