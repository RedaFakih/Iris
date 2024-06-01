#include "IrisPCH.h"
#include "AssetManagerPanel.h"

#include "ImGui/ImGuiUtils.h"
#include "Renderer/StorageBufferSet.h"
#include "Renderer/Texture.h"
#include "Renderer/UniformBufferSet.h"

namespace Iris {

	void AssetManagerPanel::OnImGuiRender(bool& open)
	{
		if (ImGui::Begin("Asset Manager", &open, ImGuiWindowFlags_NoCollapse))
		{
			// Render Asset registry entries and a search bar for them and everything
			UI::ImGuiScopedStyle headerPaddingAndHeight(ImGuiStyleVar_FramePadding, ImVec2{ 6.0f, 6.0f });

			ImGuiTreeNodeFlags treeNodeFlags = ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_DefaultOpen;
			if (ImGui::TreeNodeEx("Registry", treeNodeFlags))
			{
				constexpr float edgeOffset = 4.0f;
				UI::ShiftCursor(edgeOffset * 3.0f, edgeOffset * 2.0f);

				ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - edgeOffset * 3.0f);
				static std::string searchString;
				UI::SearchWidget(searchString);
				
				UI::BeginPropertyGrid();

				static const char* filterOptions[6] = { "None", "Texture", "StaticMesh", "MeshSource", "Material", "Scene" };
				static int selectedIndex = 0;
				UI::PropertyDropdown("Type Filter", filterOptions, 6, &selectedIndex, "Set a filter for the asset types to display");
				AssetType assetTypeFilter = Utils::AssetTypeFromString(filterOptions[selectedIndex]);

				static bool s_AllowEditing = false;
				UI::Property("Allow Editing", s_AllowEditing, "WARNING! This will directly edit Asset Registry UUIDs. Only use if you know what you are doing");
				UI::SetToolTip("WARNING! This will directly edit Asset Registry UUIDs. Only use if you know what you are\ndoing.");

				ImGui::NextColumn();
				ImGui::SameLine();
				UI::ShiftCursor(ImGui::GetContentRegionAvail().x - 186.0f, 4.0f); // 186.0f = ImGui::CalcTextSize("Number of tracked Assets: 9999").x
				ImVec2 cursorPos = ImGui::GetCursorPos();

				UI::EndPropertyGrid();

				uint32_t numberOfTrackedAssets = 0;
				ImGui::BeginChild("RegistryEntries");
				{
					UI::BeginPropertyGrid();
					ImGui::SetColumnWidth(0, ImGui::CalcTextSize("File Path").x * 2.0f);

					const AssetRegistry& assetRegistry = Project::GetEditorAssetManager()->GetAssetRegistry();

					int id = 0;
					for (const auto& [h, metaData] : assetRegistry)
					{
						if (assetTypeFilter != AssetType::None && assetTypeFilter != metaData.Type)
							continue;

						std::string handle = fmt::format("{}", metaData.Handle);

						if (!UI::IsMatchingSearch(handle, searchString) && !UI::IsMatchingSearch(metaData.FilePath.string(), searchString))
							continue;

						std::string filePath = metaData.FilePath.string();
						std::string type = Utils::AssetTypeToString(metaData.Type);

						ImGui::PushID(id++);

						if (s_AllowEditing)
						{
							AssetHandle& displayHandle = const_cast<AssetHandle&>(metaData.Handle);
							uint64_t& displayHandleUInt = reinterpret_cast<uint64_t&>(displayHandle);
							UI::PropertyInputU64(" - Handle", displayHandleUInt);
						}
						else
							UI::PropertyStringReadOnly(" - Handle", fmt::format("{}", metaData.Handle).c_str());
						UI::PropertyStringReadOnly("   File Path", filePath.empty() ? "Renderer Only" : filePath.c_str(), filePath.empty());
						UI::PropertyStringReadOnly("   Type", type.c_str());

						numberOfTrackedAssets++;
						ImGui::PopID();
					}

					UI::EndPropertyGrid();
				}
				ImGui::EndChild();

				ImGui::SetCursorPos(cursorPos);
				ImGui::TextUnformatted(fmt::format("Number of tracked Assets: {}", numberOfTrackedAssets).c_str());

				ImGui::TreePop();
			}
		}

		ImGui::End();
	}

}