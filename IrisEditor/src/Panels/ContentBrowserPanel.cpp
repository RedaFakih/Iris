#include "ContentBrowserPanel.h"

#include "Editor/EditorResources.h"
#include "Renderer/StorageBufferSet.h"
#include "Renderer/Texture.h"
#include "Renderer/UniformBufferSet.h"

#include "ImGui/ImGuiUtils.h"

namespace Iris {

	ContentBrowserPanel::ContentBrowserPanel()
		: m_Project(nullptr)
	{
		s_Instance = this;
	}

	void ContentBrowserPanel::OnImGuiRender(bool& isOpen)
	{
		if (ImGui::Begin("Content Browser", &isOpen, /* ImGuiWindowFlags_NoScrollWithMouse | */ ImGuiWindowFlags_NoScrollbar))
		{
			m_IsContentBrowserHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);
			m_IsContentBrowserFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

			UI::ImGuiScopedStyle spacing(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 8.0f));
			UI::ImGuiScopedStyle framePadding(ImGuiStyleVar_FramePadding, ImVec2(4.0f, 4.0f));
			UI::ImGuiScopedStyle cellPadding(ImGuiStyleVar_CellPadding, ImVec2(10.0f, 2.0f));

			if (m_CurrentDirectory != std::filesystem::path(m_BaseDirectory))
			{
				if (ImGui::Button("<-"))
				{
					m_CurrentDirectory = m_CurrentDirectory.parent_path();
				}
			}

			static float padding = 16.0f;
			static float thumbnailSize = 128.0f;
			float cellSize = thumbnailSize + padding;

			float panelWidth = ImGui::GetContentRegionAvail().x;
			int columnCount = (int)(panelWidth / cellSize);
			if (columnCount < 1)
				columnCount = 1;

			ImGui::Columns(columnCount, 0, false);

			for (auto& directoryEntry : std::filesystem::directory_iterator(m_CurrentDirectory))
			{
				const auto& path = directoryEntry.path();
				std::string filenameString = path.filename().string();

				ImGui::PushID(filenameString.c_str());
				Ref<Texture2D> icon = directoryEntry.is_directory() ? EditorResources::DirectoryIcon : EditorResources::FileIcon;
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
				ImGui::ImageButton("##content_browser_button_directory", UI::GetTextureID(icon), { thumbnailSize, thumbnailSize });

				if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
				{
					std::filesystem::path relativePath(path);
					AssetHandle handle = Project::GetEditorAssetManager()->GetAssetHandleFromFilePath(path);
					ImGui::SetDragDropPayload("asset_payload", &handle, 1 * sizeof(AssetHandle));
					ImGui::EndDragDropSource();
				}

				ImGui::PopStyleColor();
				if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
				{
					if (directoryEntry.is_directory())
						m_CurrentDirectory /= path.filename();

				}
				ImGui::TextWrapped(filenameString.c_str());

				ImGui::NextColumn();

				ImGui::PopID();
			}
		}

		ImGui::End();
	}

	void ContentBrowserPanel::OnEvent(Events::Event& e)
	{
		Events::EventDispatcher dispatcher(e);
		dispatcher.Dispatch<Events::KeyPressedEvent>([this](Events::KeyPressedEvent& event) { return OnKeyPressed(event); });
		dispatcher.Dispatch<Events::MouseButtonPressedEvent>([this](Events::MouseButtonPressedEvent& event) { return OnMouseButtonPressed(event); });
	}

	void ContentBrowserPanel::OnProjectChanged(const Ref<Project>& project)
	{
	}

	bool ContentBrowserPanel::OnKeyPressed(Events::KeyPressedEvent& e)
	{
		// TODO:
		return false;
	}

	bool ContentBrowserPanel::OnMouseButtonPressed(Events::MouseButtonPressedEvent& e)
	{
		// TODO:
		return false;
	}

}