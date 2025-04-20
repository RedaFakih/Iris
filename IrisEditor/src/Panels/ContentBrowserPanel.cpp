#include "ContentBrowserPanel.h"

#include "AssetManager/Asset/MeshColliderAsset.h"
#include "Core/Input/Input.h"
#include "Editor/EditorResources.h"
#include "Editor/EditorSettings.h"
#include "Editor/SelectionManager.h"
#include "ImGui/ImGuiUtils.h"
#include "ImGui/Themes.h"

namespace Iris {

	namespace Utils {

		std::filesystem::path GetUniquePathForCopy(const std::filesystem::path& path, const std::filesystem::path& itemName)
		{
			std::filesystem::path pathNameToLookFor = path / itemName;

			if (!FileSystem::Exists(pathNameToLookFor))
				return pathNameToLookFor;

			int counter = 0;
			auto checkFileName = [&counter, &path, &itemName](auto checkFileName) -> std::filesystem::path
			{
				std::string basePath = fmt::format("{}_({}){}", Utils::RemoveExtension(itemName.string()), std::to_string(++counter), itemName.extension().string());
				if (FileSystem::Exists(path / basePath))
					return checkFileName(checkFileName);
				else
					return path / basePath;
			};

			return checkFileName(checkFileName);
		}

	}

	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	/// ContentBrowserItem
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	ContentBrowserItem::ContentBrowserItem(ItemType type, AssetHandle id, const std::string_view name, const Ref<Texture2D>& icon)
		: m_Type(type), m_ID(id), m_FileName(name), m_Icon(icon)
	{
		m_DisplayName = m_FileName;
	}

	void ContentBrowserItem::OnRenderBegin()
	{
		ImGui::PushID(&m_ID);
		ImGui::BeginGroup();
	}

	CBItemActionResult ContentBrowserItem::OnRender(Ref<ContentBrowserPanel> contentBrowser)
	{
		// We render the item from back to front so we start with background then thumbnail and info panel then outline

		CBItemActionResult result;

		const float thumbnailSize = static_cast<float>(EditorSettings::Get().ContentBrowserThumbnailSize);

		GetDisplayNameFromFileName();

		constexpr float edgeOffset = 4.0f;

		const float textLineHeight = ImGui::GetTextLineHeightWithSpacing() * 2.0f + edgeOffset * 2.0f;
		const float infoPanelHeight = std::max(thumbnailSize * 0.5f, textLineHeight);

		const ImVec2 topLeft = ImGui::GetCursorScreenPos();
		const ImVec2 thumbBottomRight = { topLeft.x + thumbnailSize, topLeft.y + thumbnailSize };
		const ImVec2 infoTopLeft = { topLeft.x,	topLeft.y + thumbnailSize };
		const ImVec2 bottomRight = { topLeft.x + thumbnailSize, topLeft.y + thumbnailSize + infoPanelHeight };

		m_IsFocused = ImGui::IsWindowFocused();
		m_IsSelected = SelectionManager::IsSelected(SelectionContext::ContentBrowser, m_ID);

		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, { 0.0f, 0.0f });

		ImGui::InvisibleButton("##thumbnailButton", { thumbnailSize, thumbnailSize });

		// Fill background
		if (m_Type != ItemType::Directory)
		{
			ImDrawList* drawList = ImGui::GetWindowDrawList();

			drawList->AddRectFilled(topLeft, thumbBottomRight, Colors::Theme::BackgroundDark);
			drawList->AddRectFilled(infoTopLeft, bottomRight, Colors::Theme::GroupHeader, 6.0f, ImDrawFlags_RoundCornersBottom);
		}
		else if (ImGui::IsItemHovered() || m_IsSelected)
		{
			// Selected or hovered directory

			ImDrawList* drawList = ImGui::GetWindowDrawList();

			drawList->AddRectFilled(topLeft, bottomRight, Colors::Theme::GroupHeader, 6.0f);
		}

		// Thumbnail
		Ref<ThumbnailCache> thumbnailCache = contentBrowser->GetThumbnailCache();
		if (m_Type == ItemType::Asset && thumbnailCache->HasThumbnail(m_ID)/*AssetManager::GetAssetType(m_ID) == AssetType::Texture*/)
		{
 			ImVec2 uv0{ 0.0f, 1.0f }, uv1{ 1.0f, 0.0f };
			if (AssetManager::GetAssetType(m_ID) == AssetType::Texture)
			{
				uv0 = { 0.0f, 0.0f };
				uv1 = { 1.0f, 1.0f };
			}

			Ref<Texture2D> thumbnailImage = thumbnailCache->GetThumbnailImage(m_ID);

			float widthDiff = 0.0f, heightDiff = 0.0f;
			// TODO: For now
			if (thumbnailImage)
			{
				if (thumbnailImage->GetWidth() > thumbnailImage->GetHeight())
				{
					float verticalAspectRatio = static_cast<float>(thumbnailImage->GetHeight()) / static_cast<float>(thumbnailImage->GetWidth());
					heightDiff = (thumbnailSize - thumbnailSize * verticalAspectRatio);
				}
				else
				{
					float horizontalAspectRatio = static_cast<float>(thumbnailImage->GetWidth()) / static_cast<float>(thumbnailImage->GetHeight());
					widthDiff = (thumbnailSize - thumbnailSize * horizontalAspectRatio);
				}
			}

			ImRect thumbnailRect = UI::RectExpanded(UI::GetItemRect(), -widthDiff * 0.5f, -heightDiff * 0.5f);

			UI::DrawButtonImage(thumbnailImage ? thumbnailImage : m_Icon,
				IM_COL32(255, 255, 255, 225),
				IM_COL32(255, 255, 255, 255),
				IM_COL32(255, 255, 255, 255),
				UI::RectExpanded(thumbnailRect, -6.0f, -6.0f), uv0, uv1);
		}
		else
		{
			UI::DrawButtonImage(
				m_Icon,
				IM_COL32(255, 255, 255, 255),
				IM_COL32(255, 255, 255, 255),
				IM_COL32(255, 255, 255, 255),
				UI::RectExpanded(UI::GetItemRect(), -6.0f, -6.0f)
			);
		}

		// Info Panel

		auto renamingWidget = [&]()
		{
			ImGui::SetKeyboardFocusHere();
			ImGui::InputText("##rename", s_RenameBuffer, CB_MAX_INPUT_SEARCH_BUFFER_LENGTH);

			if (ImGui::IsItemDeactivatedAfterEdit() || Input::IsKeyDown(KeyCode::Enter))
			{
				Rename(s_RenameBuffer);
				m_IsRenaming = false;
				GetDisplayNameFromFileName();
				result.Set(ContentBrowserAction::Renamed, true);
			}
		};

		UI::ShiftCursor(edgeOffset, edgeOffset);
		if (m_Type == ItemType::Directory)
		{
			ImGui::BeginVertical(fmt::format("InfoPanel{}", m_DisplayName).c_str(), { thumbnailSize - edgeOffset * 2.0f, infoPanelHeight - edgeOffset });
			{
				// Centre align the directory name
				ImGui::BeginHorizontal(m_FileName.c_str(), { thumbnailSize - 2.0f, 0.0f });
				ImGui::Spring();

				ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + (thumbnailSize - edgeOffset * 3.0f));
				const float textWidth = glm::min(ImGui::CalcTextSize(m_DisplayName.c_str()).x, thumbnailSize);
				if (m_IsRenaming)
				{
					ImGui::SetNextItemWidth(thumbnailSize - edgeOffset * 3.0f);
					renamingWidget();
				}
				else
				{
					ImGui::SetNextItemWidth(textWidth);
					ImGui::Text(m_DisplayName.c_str());
				}
				ImGui::PopTextWrapPos();

				ImGui::Spring();
				ImGui::EndHorizontal();
			}
			ImGui::EndVertical();
		}
		else
		{
			ImGui::BeginVertical(fmt::format("InfoPanel{}", m_DisplayName).c_str(), { thumbnailSize - edgeOffset * 3.0f, infoPanelHeight - edgeOffset });
			{
				ImGui::BeginHorizontal("label", { 0.0f, 0.0f });
				ImGui::SuspendLayout();

				ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + (thumbnailSize - edgeOffset * 2.0f));
				if (m_IsRenaming)
				{
					ImGui::SetNextItemWidth(thumbnailSize - edgeOffset * 3.0f);
					renamingWidget();
				}
				else
				{
					ImGui::Text(m_DisplayName.c_str());
				}
				ImGui::PopTextWrapPos();

				ImGui::ResumeLayout();
				ImGui::EndHorizontal();
			
				ImGui::Spring();
				UI::ShiftCursorX(edgeOffset);

				ImGui::BeginHorizontal("assetType", { 0.0f, 0.0f });
				ImGui::Spring();

				const AssetMetaData& assetMetaData = Project::GetEditorAssetManager()->GetMetaData(m_ID);
				const std::string assetType = Utils::ToUpper(Utils::AssetTypeToString(assetMetaData.Type));
				{
					const ImGuiFontsLibrary& fontsLibrary = Application::Get().GetImGuiLayer()->GetFontsLibrary();
					UI::ImGuiScopedColor textColor(ImGuiCol_Text, Colors::Theme::TextDarker);
					UI::ImGuiScopedFont textFont(fontsLibrary.GetFont(DefaultFonts::Small));

					ImGui::TextUnformatted(assetType.c_str());
				}

				ImGui::Spring(-1.0f, edgeOffset);
				ImGui::EndHorizontal();
			}
			ImGui::EndVertical();
		}
		UI::ShiftCursor(-edgeOffset, -edgeOffset);

		ImGui::PopStyleVar(); // ImGuiStyleVar_ItemSpacing
		
		ImGui::EndGroup();

		// Outline
		if (m_IsSelected || ImGui::IsItemHovered())
		{
			ImRect itemRect = UI::GetItemRect();
			ImDrawList* drawList = ImGui::GetWindowDrawList();

			if (m_IsSelected)
			{
				const bool mouseDown = ImGui::IsMouseDown(ImGuiMouseButton_Left) && ImGui::IsItemHovered();
				ImColor colTransition = UI::ColorWithMultipliedValue(Colors::Theme::Selection, 0.8f);

				drawList->AddRect(
					itemRect.Min, itemRect.Max,
					mouseDown ? ImColor(colTransition) : ImColor(Colors::Theme::Selection),
					6.0f,
					m_Type == ItemType::Directory ? 0 : ImDrawFlags_RoundCornersBottom,
					1.0f
				);
			}
			else // Hovered
			{
				if (m_Type != ItemType::Directory)
					drawList->AddRect(
						itemRect.Min, itemRect.Max,
						Colors::Theme::Muted,
						6.0f,
						ImDrawFlags_RoundCornersBottom,
						1.0f
					);
			}
		}

		UpdateDrop(result);

		bool dragging = ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID);
		if (dragging)
		{
			m_IsDragging = true;

			if(!SelectionManager::IsSelected(SelectionContext::ContentBrowser, m_ID))
				result.Set(ContentBrowserAction::ClearSelections, true);

			const std::vector<UUID>& currentSelections = SelectionManager::GetSelections(SelectionContext::ContentBrowser);
			const Utils::ItemList& currentItems = ContentBrowserPanel::Get().GetCurrentItems();

			if (!currentSelections.empty())
			{
				for (UUID selectedItemHandle : currentSelections)
				{
					std::size_t index = currentItems.Find(selectedItemHandle);
					if (index == Utils::ItemList::s_InvalidItem)
						continue;

					const Ref<ContentBrowserItem>& item = currentItems[index];
					UI::Image(item->GetIcon(), { 20.0f, 20.0f });
					ImGui::SameLine();
					ImGui::TextUnformatted(item->GetFileName().c_str());
				}

				ImGui::SetDragDropPayload("asset_payload", currentSelections.data(), sizeof(AssetHandle) * currentSelections.size());
			}

			result.Set(ContentBrowserAction::Selected, true);
			
			ImGui::EndDragDropSource();
		}

		m_IsDragging = dragging;

		// NOTE: This exists since we are using Mouse + Key combos for the state check so we cant put this in the OnKeyPressed/OnMouseButtonPressed stuff
		UpdateInput(result);

		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, { 4.0f, 4.0f });
		if (ImGui::BeginPopupContextItem("CBItemContextMenu"))
		{
			result.Set(ContentBrowserAction::Selected, true);
			
			int sectionIndex = 0;
			if (UI::BeginSection("General", sectionIndex, false))
			{
				if (m_Type == ItemType::Asset && ImGui::MenuItem("Reload"))
				{
					result.Set(ContentBrowserAction::Reload, true);
				}

				if (SelectionManager::GetSelectionCount(SelectionContext::ContentBrowser) == 1 && ImGui::MenuItem("Rename"))
				{
					result.Set(ContentBrowserAction::StartRenaming, true);
				}

				if (ImGui::MenuItem("Copy", "Ctrl + C"))
				{
					result.Set(ContentBrowserAction::Copy, true);
				}

				bool shouldDisable = ContentBrowserPanel::Get().GetCurrentCopiedItems().Find(m_ID) == Utils::SelectionStack::s_InvalidItem;
				if (ImGui::MenuItem("Delete", nullptr, nullptr, shouldDisable))
				{
					result.Set(ContentBrowserAction::OpenDeleteDialogue, true);
				}

				UI::EndSection();
			}

			if (UI::BeginSection("Actions", sectionIndex, false))
			{
				if (ImGui::MenuItem("Show In Explorer"))
				{
					result.Set(ContentBrowserAction::ShowInExplorer, true);
				}

				if (ImGui::MenuItem("Open Externally"))
				{
					result.Set(ContentBrowserAction::OpenExternal, true);
				}

				UI::EndSection();
			}

			ImGui::EndPopup();
		}
		ImGui::PopStyleVar();

		return result;
	}

	void ContentBrowserItem::OnRenderEnd()
	{
		ImGui::PopID();
		ImGui::NextColumn();
	}

	void ContentBrowserItem::StartRenaming()
	{
		if (m_IsRenaming)
			return;

		memset(s_RenameBuffer, 0, CB_MAX_INPUT_SEARCH_BUFFER_LENGTH);
		memcpy(s_RenameBuffer, m_FileName.c_str(), m_FileName.size());
		m_IsRenaming = true;
	}

	void ContentBrowserItem::StopRenaming()
	{
		m_IsRenaming = false;
		GetDisplayNameFromFileName();
		memset(s_RenameBuffer, 0, CB_MAX_INPUT_SEARCH_BUFFER_LENGTH);
	}

	void ContentBrowserItem::Rename(const std::string& newName)
	{
		OnRenamed(newName);
	}

	void ContentBrowserItem::GetDisplayNameFromFileName()
	{
		const float thumbnailSize = static_cast<float>(EditorSettings::Get().ContentBrowserThumbnailSize);

		int maxCharacters = static_cast<int>(0.00152587f * (thumbnailSize * thumbnailSize));

		if (m_FileName.size() > maxCharacters)
			m_DisplayName = fmt::format("{}...", m_FileName.substr(0, maxCharacters));
		else
			m_DisplayName = m_FileName;
	}

	void ContentBrowserItem::UpdateInput(CBItemActionResult& result)
	{
		if (!m_IsRenaming)
		{
			if (Input::IsKeyDown(KeyCode::F2) && m_IsSelected && m_IsFocused)
				StartRenaming();
		}

		if (ImGui::IsItemHovered())
		{
			result.Set(ContentBrowserAction::Hovered, true);

			if (!m_IsRenaming && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && !Input::IsKeyDown(KeyCode::LeftControl))
			{
				result.Set(ContentBrowserAction::Activated, true);
			}
			else
			{
				bool skipBecauseDragging = m_IsDragging && m_IsSelected;

				if (!skipBecauseDragging && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
				{
					if (m_JustSelected)
						m_JustSelected = false;

					if (m_IsSelected && Input::IsKeyDown(KeyCode::LeftControl) && !m_JustSelected)
					{
						result.Set(ContentBrowserAction::Deselected, true);
					}

					if (!m_IsSelected)
					{
						result.Set(ContentBrowserAction::Selected, true);
						m_JustSelected = true;
					}

					if (!Input::IsKeyDown(KeyCode::LeftControl) && !Input::IsKeyDown(KeyCode::LeftShift) && m_JustSelected)
					{
						result.Set(ContentBrowserAction::ClearSelections, true);
					}

					if (Input::IsKeyDown(KeyCode::LeftShift))
					{
						result.Set(ContentBrowserAction::SelectToHere, true);
					}
				}
			}
		}
	}

	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	/// ContentBrowserDirectory
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	ContentBrowserDirectory::ContentBrowserDirectory(const Ref<DirectoryInfo>& directoryInfo)
		: ContentBrowserItem(ItemType::Directory, directoryInfo->Handle, directoryInfo->FilePath.filename().string(), EditorResources::DirectoryIcon), m_DirectoryInfo(directoryInfo)
	{
	}

	void ContentBrowserDirectory::Delete()
	{
		bool deleted = FileSystem::DeleteFile(Project::GetActive()->GetAssetDirectory() / m_DirectoryInfo->FilePath);
		if (!deleted)
		{
			IR_CORE_ERROR_TAG("Editor", "Failed to delete folder: {0}!", m_DirectoryInfo->FilePath);
			return;
		}

		for (UUID asset : m_DirectoryInfo->Assets)
			Project::GetEditorAssetManager()->OnAssetDeleted(asset);
	}

	bool ContentBrowserDirectory::Move(const std::filesystem::path& destination)
	{
		if (!FileSystem::MoveFile(Project::GetActive()->GetAssetDirectory() / m_DirectoryInfo->FilePath, Project::GetActive()->GetAssetDirectory() / destination))
		{
			IR_CORE_ERROR_TAG("Editor", "Could not move directory {} to {} since it already exists!", Project::GetActive()->GetAssetDirectory() / m_DirectoryInfo->FilePath, Project::GetActive()->GetAssetDirectory() / destination);
			
			return false;
		}

		return true;
	}

	void ContentBrowserDirectory::OnRenamed(const std::string& newName)
	{
		std::filesystem::path target = Project::GetActive()->GetAssetDirectory() / m_DirectoryInfo->FilePath;
		std::filesystem::path destination = Project::GetActive()->GetAssetDirectory() / m_DirectoryInfo->FilePath.parent_path() / newName;

		if (Utils::ToLower(newName) == Utils::ToLower(target.filename().string()))
		{
			auto temp = Project::GetActive()->GetAssetDirectory() / m_DirectoryInfo->FilePath.parent_path() / "TempDir";
			FileSystem::Rename(target, temp);
			target = temp;
		}

		if (!FileSystem::Rename(target, destination))
		{
			IR_CORE_ERROR_TAG("Editor", "Couldn't rename {} to {}!", m_DirectoryInfo->FilePath.filename().string(), newName);
		}
	}

	void ContentBrowserDirectory::UpdateDrop(CBItemActionResult& result)
	{
		if (SelectionManager::IsSelected(SelectionContext::ContentBrowser, GetID()))
			return;

		if (ImGui::BeginDragDropTarget())
		{
			const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("asset_payload");

			if (payload)
			{
				Utils::ItemList& currentItems = ContentBrowserPanel::Get().GetCurrentItems();
				uint32_t count = payload->DataSize / sizeof(AssetHandle);

				for (uint32_t i = 0; i < count; i++)
				{
					AssetHandle assetHandle = *((reinterpret_cast<AssetHandle*>(payload->Data)) + i);
					std::size_t index = currentItems.Find(assetHandle);
					if (index != Utils::ItemList::s_InvalidItem)
					{
						if (currentItems[index]->Move(m_DirectoryInfo->FilePath))
						{
							result.Set(ContentBrowserAction::Refresh, true);
							currentItems.Erase(assetHandle);
						}
					}
				}
			}

			ImGui::EndDragDropTarget();
		}
	}

	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	/// ContentBrowserAsset
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	ContentBrowserAsset::ContentBrowserAsset(const AssetMetaData& metaData, const Ref<Texture2D>& icon)
		: ContentBrowserItem(ItemType::Asset, metaData.Handle, metaData.FilePath.filename().string(), icon), m_AssetInfo(metaData)
	{
	}

	void ContentBrowserAsset::Delete()
	{
		std::filesystem::path filepath = Project::GetEditorAssetManager()->GetFileSystemPath(m_AssetInfo);
		bool deleted = FileSystem::DeleteFile(filepath);
		if (!deleted)
		{
			IR_CORE_ERROR_TAG("Editor", "Couldn't delete asset: {0}!", m_AssetInfo.FilePath);
			return;
		}

		Ref<DirectoryInfo> currentDirectory = ContentBrowserPanel::Get().GetDirectory(m_AssetInfo.FilePath.parent_path());
		currentDirectory->Assets.erase(std::remove(currentDirectory->Assets.begin(), currentDirectory->Assets.end(), m_AssetInfo.Handle), currentDirectory->Assets.end());

		Project::GetEditorAssetManager()->OnAssetDeleted(m_AssetInfo.Handle);
	}

	bool ContentBrowserAsset::Move(const std::filesystem::path& destination)
	{
		std::filesystem::path filepath = Project::GetEditorAssetManager()->GetFileSystemPath(m_AssetInfo);
		if (!FileSystem::MoveFile(filepath, Project::GetActive()->GetAssetDirectory() / destination))
		{
			IR_CORE_ERROR_TAG("Editor", "Couldn't move asset {0} to {1}", m_AssetInfo.FilePath, destination);
			
			return false;
		}

		Project::GetEditorAssetManager()->OnAssetRenamed(m_AssetInfo.Handle, destination / filepath.filename());
		
		return true;
	}

	void ContentBrowserAsset::OnRenamed(const std::string& newName)
	{
		std::filesystem::path filePath = Project::GetEditorAssetManager()->GetFileSystemPath(m_AssetInfo);
		const std::string extension = filePath.extension().string();

		std::filesystem::path newFilePath = fmt::format("{}\\{}{}", filePath.parent_path().string(), newName, extension);

		std::string targetName = fmt::format("{}{}", newName, extension);
		if (Utils::ToLower(targetName) == Utils::ToLower(filePath.filename().string()))
		{
			FileSystem::RenameFilename(filePath, "temp-rename");
			filePath = fmt::format("{}\\temp-rename{}", filePath.parent_path().string(), extension);
		}

		if (FileSystem::RenameFilename(filePath, newName))
		{
			// Update AssetManager with the new name
			Project::GetEditorAssetManager()->OnAssetRenamed(m_AssetInfo.Handle, newFilePath);
		}
		else
		{
			IR_CORE_ERROR_TAG("Editor", "Couldn't rename {} to {}!", filePath.filename().string(), newName);
		}
	}

	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	/// Utils::SelectionStack
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	namespace Utils {

		void SelectionStack::CopyFrom(const Utils::ItemList& items)
		{
			// TODO: Here we want to do deep copies of the items actually that way we can avoid crashes if user copies then deletes then item before pasting!
			const std::vector<UUID>& selectedItems = SelectionManager::GetSelections(SelectionContext::ContentBrowser);

			for (UUID handle : selectedItems)
			{
				std::size_t index = items.Find(handle);
				if (index != Utils::ItemList::s_InvalidItem)
				{
					m_SelectedItems.push_back(items[index]);
				}
			}
		}

	}

	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	/// ContentBrowserPanel
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	ContentBrowserPanel::ContentBrowserPanel()
		: m_Project(nullptr)
	{
		s_Instance = this;
	
		memset(m_SearchBuffer, 0, CB_MAX_INPUT_SEARCH_BUFFER_LENGTH);
	}

	void ContentBrowserPanel::OnImGuiRender(bool& isOpen)
	{
		GenerateThumbnails();

		if (ImGui::Begin("Content Browser", &isOpen, ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoScrollbar))
		{
			m_IsContentBrowserHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);
			m_IsContentBrowserFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

			UI::ImGuiScopedStyle itemspacing(ImGuiStyleVar_ItemSpacing, { 8.0f, 8.0f });
			UI::ImGuiScopedStyle framePadding(ImGuiStyleVar_FramePadding, { 4.0f, 4.0f });
			UI::ImGuiScopedStyle cellPadding(ImGuiStyleVar_CellPadding, { 10.0f, 2.0f });

			UI::PushID();

			{
				// TODO: Hierarchy Side Outliner? If yes, we need to setup a table and columns for it otherwise that is not needed for just the Browser
			}
			
			// Content Browser
			{
				constexpr float topBarHeight = 26.0f;
				constexpr float bottomBarHeight = 32.0f;

				ImGui::BeginChild("##directory_content_browser", { ImGui::GetContentRegionAvail().x, ImGui::GetWindowHeight() - topBarHeight - bottomBarHeight });
				{
					{
						ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
						RenderTopBar(topBarHeight);
						ImGui::PopStyleVar();
					}

					ImGui::Separator();

					ImGui::BeginChild("Scrolling");
					{
						ImGui::PushStyleColor(ImGuiCol_Button, { 0.0f, 0.0f, 0.0f, 0.0f });
						ImGui::PushStyleColor(ImGuiCol_ButtonHovered, { 0.3f, 0.3f, 0.3f, 0.35f });

						ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, { 4.0f, 4.0f });
						if (ImGui::BeginPopupContextWindow(nullptr, ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
						{
							int sectionIndex = 0;

							if (UI::BeginSection("General", sectionIndex, false, 200.0f))
							{
								if (ImGui::BeginMenu("New"))
								{
									if (ImGui::MenuItem("Folder", "Ctrl + Shift + N"))
									{
										std::filesystem::path filePath = FileSystem::GetUniqueFileName(Project::GetAssetDirectory() / m_CurrentDirectory->FilePath / "New Folder");

										if (FileSystem::CreateDirectory(filePath))
										{
											Refresh();
											const Ref<DirectoryInfo>& dirInfo = GetDirectory(m_CurrentDirectory->FilePath / filePath.filename());
											std::size_t index = m_CurrentItems.Find(dirInfo->Handle);
											if (index != Utils::ItemList::s_InvalidItem)
											{
												SelectionManager::DeselectAll(SelectionContext::ContentBrowser);
												SelectionManager::Select(SelectionContext::ContentBrowser, dirInfo->Handle);
												m_CurrentItems[index]->StartRenaming();
											}
										}
									}

									if (ImGui::MenuItem("Scene"))
									{
										CreateAsset<Scene>("New Scene.Iscene");
									}

									if (ImGui::MenuItem("Material"))
									{
										CreateAsset<MaterialAsset>("New Material.Imaterial");
									}

									if (ImGui::BeginMenu("Physics"))
									{
										if (ImGui::MenuItem("Mesh Collider"))
										{
											CreateAsset<MeshColliderAsset>("New Mesh Collider.Imc");
										}

										ImGui::EndMenu();
									}

									ImGui::EndMenu();
								}

								if (ImGui::MenuItem("Refresh"))
									Refresh();

								if (ImGui::MenuItem("Copy", "Ctrl + C", nullptr, SelectionManager::GetSelectionCount(SelectionContext::ContentBrowser) > 0))
									m_CopiedAssets.CopyFrom(m_CurrentItems);

								if (ImGui::MenuItem("Paste", "Ctrl + V", nullptr, m_CopiedAssets.SelectionCount() > 0))
									PasteCopiedAssets();

								UI::EndSection();
							}

							if (UI::BeginSection("Actions", sectionIndex, false, 200.0f))
							{
								if (ImGui::MenuItem("Import"))
								{
									std::filesystem::path filepath = FileSystem::OpenFileDialog();
									if (!filepath.empty())
									{
										FileSystem::CopyFile(filepath, Project::GetAssetDirectory() / m_CurrentDirectory->FilePath);
										Refresh();
									}
								}

								if (ImGui::MenuItem("Show in Explorer"))
									FileSystem::OpenDirectoryInExplorer(Project::GetAssetDirectory() / m_CurrentDirectory->FilePath);

								UI::EndSection();
							}

							ImGui::EndPopup();
						}
						ImGui::PopStyleVar();

						constexpr float paddingForOutline = 2.0f;

						const float scrollBarOffset = 20.0f + ImGui::GetStyle().ScrollbarSize;
						const float panelWidth = ImGui::GetContentRegionAvail().x - scrollBarOffset;
						const float cellSize = EditorSettings::Get().ContentBrowserThumbnailSize + paddingForOutline + paddingForOutline;
						int columnCount = static_cast<int>(panelWidth / cellSize);

						if (columnCount < 1)
							columnCount = 1; // just in case we get rounded to 0

						{
							constexpr float rowSpacing = 12.0f;
							
							UI::ImGuiScopedStyle spacing(ImGuiStyleVar_ItemSpacing, { paddingForOutline, rowSpacing });
							ImGui::Columns(columnCount, nullptr, false);

							UI::ImGuiScopedStyle border(ImGuiStyleVar_FrameBorderSize, 0.0f);
							UI::ImGuiScopedStyle padding(ImGuiStyleVar_FramePadding, { 0.0f, 0.0f });
							RenderItems();
						}

						if (ImGui::IsWindowFocused() && !ImGui::IsMouseDragging(ImGuiMouseButton_Left))
							UpdateInput();

						ImGui::PopStyleColor(2);

						RenderDeleteDialogue();
					}
					ImGui::EndChild();
				}
				ImGui::EndChild();

				// TODO: Drag drop for prefabs from scene hierarchy to content browser

				RenderBottomBar(bottomBarHeight);
			}

			UI::PopID();
		}

		ImGui::End();
	}

	void ContentBrowserPanel::OnEvent(Events::Event& e)
	{
		Events::EventDispatcher dispatcher(e);
		dispatcher.Dispatch<Events::KeyPressedEvent>([this](Events::KeyPressedEvent& event) { return OnKeyPressed(event); });
		dispatcher.Dispatch<Events::MouseButtonPressedEvent>([this](Events::MouseButtonPressedEvent& event) { return OnMouseButtonPressed(event); });
		dispatcher.Dispatch<Events::AssetCreatedNotificationEvent>([this](Events::AssetCreatedNotificationEvent& event) { return OnAssetCreatedNotification(event); });
	}

	void ContentBrowserPanel::OnProjectChanged(const Ref<Project>& project)
	{
		m_Directories.clear();

		m_CurrentItems.Clear();

		m_BaseDirectory = nullptr;
		m_CurrentDirectory = nullptr;
		m_PreviousDirectory = nullptr;
		m_NextDirectory = nullptr;

		SelectionManager::DeselectAll();

		m_Project = project;

		m_ThumbnailCache = ThumbnailCache::Create();
		m_ThumbnailGenerator = ThumbnailGenerator::Create();

		AssetHandle baseDirectoryHandle = ProcessDirectory(project->GetAssetDirectory().string(), nullptr);
		m_BaseDirectory = m_Directories[baseDirectoryHandle];
		ChangeDirectory(m_BaseDirectory);
	
		memset(m_SearchBuffer, 0, CB_MAX_INPUT_SEARCH_BUFFER_LENGTH);
	}

	Ref<DirectoryInfo> ContentBrowserPanel::GetDirectory(const std::filesystem::path& filePath) const
	{
		if (filePath.empty() || filePath.string() == ".")
			return m_BaseDirectory;

		for (const auto& [handle, dir] : m_Directories)
		{
			if (dir->FilePath == filePath)
				return dir;
		}

		return nullptr;
	}

	AssetHandle ContentBrowserPanel::ProcessDirectory(const std::filesystem::path& directoryPath, const Ref<DirectoryInfo>& parent)
	{
		// Check if already exists
		const Ref<DirectoryInfo>& cachedDirectory = GetDirectory(directoryPath);
		if (cachedDirectory)
			return cachedDirectory->Handle;

		// Otherwise we create a new entry
		Ref<DirectoryInfo> newDirectory = DirectoryInfo::Create();
		newDirectory->Handle = AssetHandle();
		newDirectory->Parent = parent;

		if (directoryPath == m_Project->GetAssetDirectory())
			// We clear it, we could also set it to the asset directory path if we want.
			newDirectory->FilePath.clear();
		else
			newDirectory->FilePath = std::filesystem::relative(directoryPath, m_Project->GetAssetDirectory());

		for (const auto& entry : std::filesystem::directory_iterator{ directoryPath })
		{
			// Process all subdirectories recursively
			if (entry.is_directory())
			{
				AssetHandle subDirHandle = ProcessDirectory(entry.path(), newDirectory);
				newDirectory->SubDirectories[subDirHandle] = m_Directories[subDirHandle];
				continue;
			}

			// NOTE: This so that we can load any changes done by the user externally when we refresh the browser
			// If the entry is an asset instead of a directory
			AssetMetaData metaData = Project::GetEditorAssetManager()->GetMetaData(std::filesystem::relative(entry.path(), m_Project->GetAssetDirectory()));
			if (!metaData.IsValid())
			{
				// Try to get the asset type from the file extension
				AssetType assetType = Project::GetEditorAssetManager()->GetAssetTypeFromPath(entry.path());
				if (assetType == AssetType::None)
					continue;

				metaData.Handle = Project::GetEditorAssetManager()->ImportAsset(entry.path());
			}

			// Failed to import the asset
			if (!metaData.IsValid())
			{
				IR_CORE_ERROR_TAG("Editor", "Failed to import asset with filepath: {}", entry.path());
				
				continue;
			}

			newDirectory->Assets.push_back(metaData.Handle);
		}

		m_Directories[newDirectory->Handle] = newDirectory;
		return newDirectory->Handle;
	}

	void ContentBrowserPanel::ChangeDirectory(const Ref<DirectoryInfo>& directory)
	{
		if (!directory)
			return;

		m_UpdateNavigationPath = true;

		m_CurrentItems.Clear();

		if (strlen(m_SearchBuffer) == 0)
		{
			for (const auto& [subDirHandle, subDir] : directory->SubDirectories)
				m_CurrentItems.GetItems().push_back(ContentBrowserDirectory::Create(subDir));

			for (AssetHandle assetHandle : directory->Assets)
			{
				const AssetMetaData& assetMetaData = Project::GetEditorAssetManager()->GetMetaData(assetHandle);
				if (!assetMetaData.IsValid())
					continue;

				m_CurrentItems.GetItems().push_back(ContentBrowserAsset::Create(assetMetaData, EditorResources::FileIcon));
			}
		}
		else
		{
			m_CurrentItems = Search(directory);
		}
		
		SortItemList();

		m_PreviousDirectory = directory;
		m_CurrentDirectory = directory;

		ClearSelections();
		GenerateThumbnails();
	}

	void ContentBrowserPanel::OnBrowseBack()
	{
		m_NextDirectory = m_CurrentDirectory;
		m_PreviousDirectory = m_CurrentDirectory->Parent;
		ChangeDirectory(m_PreviousDirectory);
	}

	void ContentBrowserPanel::OnBrowseForward()
	{
		ChangeDirectory(m_NextDirectory);
	}

	void ContentBrowserPanel::Refresh()
	{
		m_CurrentItems.Clear();
		m_Directories.clear();

		Ref<DirectoryInfo> currentDirectory = m_CurrentDirectory;
		AssetHandle baseDirectoryHandle = ProcessDirectory(m_Project->GetAssetDirectory(), nullptr);
		m_BaseDirectory = m_Directories[baseDirectoryHandle];
		m_CurrentDirectory = GetDirectory(currentDirectory->FilePath);

		if (!m_CurrentDirectory)
			m_CurrentDirectory = m_BaseDirectory; // Our current directory was removed

		ChangeDirectory(m_CurrentDirectory);
	}

	void ContentBrowserPanel::PasteCopiedAssets()
	{
		if (m_CopiedAssets.SelectionCount() == 0)
			return;

		for (const Ref<ContentBrowserItem>& copiedAsset : m_CopiedAssets)
		{
			const std::filesystem::path assetDirectory = Project::GetAssetDirectory();

			if (copiedAsset->GetItemType() == ContentBrowserItem::ItemType::Asset)
			{
				std::filesystem::path originalFilePath = assetDirectory / copiedAsset.As<ContentBrowserAsset>()->GetAssetInfo().FilePath;

				std::filesystem::path destPath = assetDirectory / m_CurrentDirectory->FilePath;

				std::filesystem::path filepath = Utils::GetUniquePathForCopy(destPath, originalFilePath.filename());
				IR_VERIFY(!FileSystem::Exists(filepath));
				
				FileSystem::Copy(originalFilePath, filepath);
			}
			else
			{
				std::filesystem::path originalFilePath = assetDirectory / copiedAsset.As<ContentBrowserDirectory>()->GetDirectoryInfo()->FilePath;
				
				std::filesystem::path destPath = assetDirectory / m_CurrentDirectory->FilePath;

				std::filesystem::path filepath = Utils::GetUniquePathForCopy(destPath, originalFilePath.filename());
				IR_VERIFY(!FileSystem::Exists(filepath));
				
				FileSystem::Copy(originalFilePath, filepath);
			}
		}

		Refresh();

		SelectionManager::DeselectAll();
		m_CopiedAssets.Clear();
	}

	void ContentBrowserPanel::ClearSelections()
	{
		std::vector<AssetHandle> selectedItems = SelectionManager::GetSelections(SelectionContext::ContentBrowser);
		for (AssetHandle itemHandle : selectedItems)
		{
			SelectionManager::Deselect(SelectionContext::ContentBrowser, itemHandle);
			if (size_t index = m_CurrentItems.Find(itemHandle); index != Utils::ItemList::s_InvalidItem)
			{
				if (m_CurrentItems[index]->IsRenaming())
					m_CurrentItems[index]->StopRenaming();
			}
		}
	}

	void ContentBrowserPanel::SortItemList()
	{
		std::sort(m_CurrentItems.begin(), m_CurrentItems.end(), [](const Ref<ContentBrowserItem>& item1, const Ref<ContentBrowserItem>& item2)
		{
			if (item1->GetItemType() == item2->GetItemType())
				return Utils::ToLower(item1->GetFileName()) < Utils::ToLower(item2->GetFileName());

			return static_cast<uint16_t>(item1->GetItemType()) < static_cast<uint16_t>(item2->GetItemType());
		});
	}

	Utils::ItemList ContentBrowserPanel::Search(const Ref<DirectoryInfo>& directoryInfo)
	{
		Utils::ItemList result;

		std::string queryLowerCase = Utils::ToLower(m_SearchBuffer);

		for (const auto& [handle, subDir] : directoryInfo->SubDirectories)
		{
			std::string subDirName = subDir->FilePath.filename().string();
			if (subDirName.find(queryLowerCase) != std::string::npos)
				result.GetItems().push_back(ContentBrowserDirectory::Create(subDir));

			// Query string doesnt change in m_SearchBuffer
			Utils::ItemList subDirResult = Search(subDir);
			if (subDirResult.GetItems().size())
				result.GetItems().insert(result.end(), subDirResult.begin(), subDirResult.end());
		}

		for (const AssetHandle& assetHandle : directoryInfo->Assets)
		{
			const AssetMetaData& assetMetaData = Project::GetEditorAssetManager()->GetMetaData(assetHandle);
			std::string fileName = Utils::ToLower(assetMetaData.FilePath.filename().string());

			if (fileName.find(queryLowerCase) != std::string::npos)
				result.GetItems().push_back(ContentBrowserAsset::Create(assetMetaData, EditorResources::FileIcon));
		}

		return result;
	}

	void ContentBrowserPanel::UpdateInput()
	{
		// NOTE: This exists since we are using Mouse + Key combos for the state check so we cant put this in the OnKeyPressed/OnMouseButtonPressed stuff
		if (!m_IsContentBrowserHovered)
			return;

		if ((!m_IsAnyItemHovered && Input::IsMouseButtonDown(MouseButton::Left)) || Input::IsKeyDown(KeyCode::Escape))
			ClearSelections();
	}

	void ContentBrowserPanel::UpdateBreadCrumbDragDropArea(const Ref<DirectoryInfo>& targetDirectory)
	{
		// We can not drag and drop on the same directory that we are currently in
		if (targetDirectory->Handle != m_CurrentDirectory->Handle && ImGui::BeginDragDropTarget())
		{
			const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("asset_payload");

			if (payload)
			{
				uint32_t count = payload->DataSize / sizeof(AssetHandle);

				for (uint32_t i = 0; i < count; i++)
				{
					AssetHandle assetHandle = *((reinterpret_cast<AssetHandle*>(payload->Data)) + i);
					std::size_t index = m_CurrentItems.Find(assetHandle);
					if (index != Utils::ItemList::s_InvalidItem)
					{
						m_CurrentItems[index]->Move(targetDirectory->FilePath);
						m_CurrentItems.Erase(assetHandle);

						Refresh();
					}
				}
			}

			ImGui::EndDragDropTarget();
		}
	}

	static bool s_ActivateSearchWidget = false;
	static bool s_OpenDeleteDialog = false;

	bool ContentBrowserPanel::OnKeyPressed(Events::KeyPressedEvent& e)
	{
		if (!m_IsContentBrowserFocused)
			return false;

		if (Input::IsKeyDown(KeyCode::LeftControl))
		{
			switch (e.GetKeyCode())
			{
				case KeyCode::C:
				{
					m_CopiedAssets.CopyFrom(m_CurrentItems);

					return true;
				}
				case KeyCode::V:
				{
					PasteCopiedAssets();

					return true;
				}
				case KeyCode::F:
				{
					s_ActivateSearchWidget = true;

					return true;
				}
			}

			if (Input::IsKeyDown(KeyCode::LeftShift))
			{
				switch (e.GetKeyCode())
				{
					case KeyCode::N:
					{
						std::filesystem::path filePath = FileSystem::GetUniqueFileName(Project::GetAssetDirectory() / m_CurrentDirectory->FilePath / "New Folder");

						if (FileSystem::CreateDirectory(filePath))
						{
							Refresh();
							const Ref<DirectoryInfo>& dirInfo = GetDirectory(m_CurrentDirectory->FilePath / filePath.filename());
							std::size_t index = m_CurrentItems.Find(dirInfo->Handle);
							if (index != Utils::ItemList::s_InvalidItem)
							{
								SelectionManager::DeselectAll(SelectionContext::ContentBrowser);
								SelectionManager::Select(SelectionContext::ContentBrowser, dirInfo->Handle);
								m_CurrentItems[index]->StartRenaming();
							}
						}

						return true;
					}
				}
			}
		}

		if (Input::IsKeyDown(KeyCode::LeftAlt) || Input::IsKeyDown(KeyCode::RightAlt))
		{
			switch (e.GetKeyCode())
			{
				case KeyCode::Left:
				{
					OnBrowseBack();

					return true;
				}
				case KeyCode::Right:
				{
					OnBrowseForward();

					return true;
				}
			}
		}

		if (e.GetKeyCode() == KeyCode::Delete && SelectionManager::GetSelectionCount(SelectionContext::ContentBrowser) > 0)
		{
			for (const Ref<ContentBrowserItem>& item : m_CurrentItems)
			{
				if (item->IsRenaming())
					return false;
			}

			s_OpenDeleteDialog = true;
			return true;
		}

		if (Input::IsKeyDown(KeyCode::F5))
		{
			Refresh();

			return true;
		}

		return false;
	}

	bool ContentBrowserPanel::OnMouseButtonPressed(Events::MouseButtonPressedEvent& e)
	{
		if (!m_IsContentBrowserFocused)
			return false;

		return false;
	}

	bool ContentBrowserPanel::OnAssetCreatedNotification(Events::AssetCreatedNotificationEvent& e)
	{
		if (AssetManager::IsAssetHandleValid(e.GetAssetHandle()))
		{
			Refresh();

			return true;
		}

		IR_CORE_ERROR_TAG("Editor", "Asset that was just added is not valid! AssetHandle: {}", e.GetAssetHandle());

		return false;
	}

	void ContentBrowserPanel::RenderTopBar(float height)
	{
		ImGui::BeginChild("##top_bar", { 0.0f, height });
		ImGui::BeginHorizontal("##top_bar", ImGui::GetWindowSize());
		{
			constexpr float edgeOffset = 4.0f;

			// Nav Buttons
			{
				UI::ImGuiScopedStyle spacing(ImGuiStyleVar_ItemSpacing, { 2.0f, 0.0f });

				auto CBTopBarButton = [height](const char* labelId, const Ref<Texture2D>& icon, const char* hint = "")
				{
					UI::ImGuiScopedColor buttonCol(ImGuiCol_Button, Colors::Theme::BackgroundDark);
					UI::ImGuiScopedColor buttonHovCol(ImGuiCol_ButtonHovered, Colors::Theme::BackgroundDark);
					UI::ImGuiScopedColor buttonActiveCol(ImGuiCol_ButtonActive, ImU32(UI::ColorWithMultipliedValue(Colors::Theme::BackgroundDark, 0.8f)));

					constexpr float iconPadding = 3.0f;
					const float iconSize = std::min(24.0f, height);
					const bool clicked = ImGui::Button(labelId, ImVec2(iconSize, iconSize));
					if (ImGui::IsItemHovered())
					{
						UI::ImGuiScopedStyle tooltipPadding(ImGuiStyleVar_WindowPadding, { 5.0f, 5.0f });
						UI::ImGuiScopedColor textCol(ImGuiCol_Text, Colors::Theme::TextBrighter);
						ImGui::SetTooltip(hint);
					}

					UI::DrawButtonImage(icon, Colors::Theme::TextDarker,
						UI::ColorWithMultipliedValue(Colors::Theme::TextDarker, 1.2f),
						UI::ColorWithMultipliedValue(Colors::Theme::TextDarker, 0.8f),
						UI::RectExpanded(UI::GetItemRect(), -iconPadding, -iconPadding)
					);

					return clicked;
				};

				if (CBTopBarButton("##back", EditorResources::BrowseBackIcon, "Previous Directory"))
				{
					OnBrowseBack();
				}

				if (CBTopBarButton("##forward", EditorResources::BrowseForwardIcon, "Next Directory"))
				{
					OnBrowseForward();
				}

				if (CBTopBarButton("##clearThumbnailCache", EditorResources::ClearIcon, "Clear thumbnail cache"))
				{
					m_ThumbnailCache->Clear();
				}

				ImGui::Spring(-1.0f, edgeOffset * 2.0f);

				if (CBTopBarButton("##refresh", EditorResources::RotateIcon, "Refresh"))
				{
					Refresh();
				}

				ImGui::Spring(-1.0f, edgeOffset * 2.0f);
			}

			// Search bar
			{
				UI::ShiftCursorY(2.0f);

				if (s_ActivateSearchWidget)
				{
					ImGui::SetKeyboardFocusHere();
					s_ActivateSearchWidget = false;
				}

				// Dunno why 300.0f but seems a good width for a search bar
				ImGui::SetNextItemWidth(300.0f);
				if (UI::SearchWidget<CB_MAX_INPUT_SEARCH_BUFFER_LENGTH>(m_SearchBuffer))
				{
					if (strlen(m_SearchBuffer) == 0)
					{
						ChangeDirectory(m_CurrentDirectory);
					}
					else
					{
						m_CurrentItems = Search(m_CurrentDirectory);
						SortItemList();
					}
				}

				UI::ShiftCursorY(-2.0f);
			}

			// Breadcrumbs
			{
				if (m_UpdateNavigationPath)
				{
					m_BreadCrumbData.clear();

					Ref<DirectoryInfo> currentDir = m_CurrentDirectory;
					while (currentDir && currentDir->Parent != nullptr)
					{
						m_BreadCrumbData.push_back(currentDir);
						currentDir = currentDir->Parent;
					}

					std::reverse(m_BreadCrumbData.begin(), m_BreadCrumbData.end());
					m_UpdateNavigationPath = false;
				}

				const ImGuiFontsLibrary& fontsLibrary = Application::Get().GetImGuiLayer()->GetFontsLibrary();
				UI::ImGuiScopedFont boldFont(fontsLibrary.GetFont(DefaultFonts::Bold));
				UI::ImGuiScopedColor textColour(ImGuiCol_Text, Colors::Theme::TextDarker);

				const std::string& assetsDirectoryName = m_Project->GetConfig().AssetDirectory;
				ImVec2 textSize = ImGui::CalcTextSize(assetsDirectoryName.c_str());

				const float textPadding = ImGui::GetStyle().FramePadding.y;

				if (ImGui::Selectable(assetsDirectoryName.c_str(), false, 0, { textSize.x, textSize.y + textPadding }))
				{
					SelectionManager::DeselectAll(SelectionContext::ContentBrowser);
					ChangeDirectory(m_BaseDirectory);
				}
				
				UpdateBreadCrumbDragDropArea(m_BaseDirectory);

				for (const Ref<DirectoryInfo>& directory : m_BreadCrumbData)
				{
					ImGui::TextUnformatted("/");

					const std::string directoryName = directory->FilePath.filename().string();
					textSize = ImGui::CalcTextSize(directoryName.c_str());

					if (ImGui::Selectable(directoryName.c_str(), false, 0, { textSize.x, textSize.y + textPadding }))
					{
						SelectionManager::DeselectAll(SelectionContext::ContentBrowser);
						ChangeDirectory(directory);
					}

					UpdateBreadCrumbDragDropArea(directory);
				}
			}

			// Options
			{
				ImGui::Spring();

				const bool clicked = ImGui::InvisibleButton("##options", { ImGui::GetFrameHeight(), ImGui::GetFrameHeight() });
				UI::SetToolTip("Settings");

				constexpr float desiredIconSize = 15.0f;
				const float spaceAvail = std::min(ImGui::GetItemRectSize().x, ImGui::GetItemRectSize().y);
				const float padding = std::max((spaceAvail - desiredIconSize) / 2.0f, 0.0f);

				constexpr uint8_t value = static_cast<uint8_t>(ImColor(Colors::Theme::TextBrighter).Value.x * 255.0f);
				UI::DrawButtonImage(
					EditorResources::GearIcon,
					IM_COL32(value, value, value, 200),
					IM_COL32(value, value, value, 255),
					IM_COL32(value, value, value, 150),
					UI::RectExpanded(UI::GetItemRect(), -padding, -padding)
				);

				if (clicked)
				{
					ImGui::OpenPopup("ContentBrowserSettings");
				}

				if (ImGui::BeginPopup("ContentBrowserSettings"))
				{
					EditorSettings& settings = EditorSettings::Get();

					int sectionIndex = 0;

					if (UI::BeginSection("Settings", sectionIndex))
					{
						bool saveSettings = UI::SectionSlider("Thumbnail Size", settings.ContentBrowserThumbnailSize, 96, 512, "Thumbnail Size");

						if (saveSettings)
							EditorSettingsSerializer::Serialize();

						UI::EndSection();
					}

					ImGui::EndPopup();
				}
			}
		}
		ImGui::EndHorizontal();
		ImGui::EndChild();
	}

	void ContentBrowserPanel::RenderItems()
	{
		m_IsAnyItemHovered = false;

		for (Ref<ContentBrowserItem>& item : m_CurrentItems)
		{
			item->OnRenderBegin();

			CBItemActionResult result = item->OnRender(this);

			item->OnRenderEnd();

			if (result.IsSet(ContentBrowserAction::Refresh))
			{
				Refresh();

				break;
			}

			if (result.IsSet(ContentBrowserAction::ClearSelections))
				ClearSelections();

			if (result.IsSet(ContentBrowserAction::Selected))
				SelectionManager::Select(SelectionContext::ContentBrowser, item->GetID());

			if (result.IsSet(ContentBrowserAction::Deselected))
				SelectionManager::Deselect(SelectionContext::ContentBrowser, item->GetID());

			if (result.IsSet(ContentBrowserAction::Hovered))
				m_IsAnyItemHovered = true;

			if (result.IsSet(ContentBrowserAction::StartRenaming))
				item->StartRenaming();

			if (result.IsSet(ContentBrowserAction::Renamed))
			{
				SelectionManager::DeselectAll(SelectionContext::ContentBrowser);
				Refresh();
				SortItemList();

				// Since we cant keep going with the items we'll have to restart since we have to resort the items after renaming
				break;
			}

			if (result.IsSet(ContentBrowserAction::OpenDeleteDialogue) && !item->IsRenaming())
				s_OpenDeleteDialog = true;

			if (result.IsSet(ContentBrowserAction::SelectToHere) && SelectionManager::GetSelectionCount(SelectionContext::ContentBrowser) == 2)
			{
				std::size_t firstIndex = m_CurrentItems.Find(SelectionManager::GetSelection(SelectionContext::ContentBrowser, 0));
				std::size_t lastIndex = m_CurrentItems.Find(item->GetID());

				if (firstIndex > lastIndex)
				{
					std::size_t temp = lastIndex;
					lastIndex = firstIndex;
					firstIndex = temp;
				}

				for (std::size_t i = firstIndex; i < lastIndex; i++)
					SelectionManager::Select(SelectionContext::ContentBrowser, m_CurrentItems[i]->GetID());
			}

			if (result.IsSet(ContentBrowserAction::ShowInExplorer))
			{
				// Directories do not exist in the AssetManager so we have to build their filePaths from existing info unlike assets
				if (item->GetItemType() == ContentBrowserItem::ItemType::Directory)
					FileSystem::ShowFileInExplorer(m_Project->GetAssetDirectory() / m_CurrentDirectory->FilePath / item->GetFileName());
				else
					FileSystem::ShowFileInExplorer(Project::GetEditorAssetManager()->GetFileSystemPath(item->GetID()));
			}

			if (result.IsSet(ContentBrowserAction::OpenExternal))
			{
				// Directories do not exist in the AssetManager so we have to build their filePaths from existing info unlike assets
				if (item->GetItemType() == ContentBrowserItem::ItemType::Directory)
					FileSystem::OpenDirectoryInExplorer(m_Project->GetAssetDirectory() / m_CurrentDirectory->FilePath / item->GetFileName());
				else
					FileSystem::OpenExternally(Project::GetEditorAssetManager()->GetFileSystemPath(item->GetID()));
			}

			if (result.IsSet(ContentBrowserAction::Reload))
			{
				if ((item->GetItemType() == ContentBrowserItem::ItemType::Asset) && !AssetManager::ReloadData(item->GetID()))
					IR_CORE_ERROR_TAG("AssetManager", "Failed to reload asset with (ID: name): ({}, {})", item->GetID(), item->GetFileName());
			}

			if (result.IsSet(ContentBrowserAction::Copy))
				m_CopiedAssets.Select(item);

			if (result.IsSet(ContentBrowserAction::Activated))
			{
				if (item->GetItemType() == ContentBrowserItem::ItemType::Directory)
				{
					SelectionManager::DeselectAll(SelectionContext::ContentBrowser);
					ChangeDirectory(item.As<ContentBrowserDirectory>()->GetDirectoryInfo());

					break;
				}
				else
				{
					Ref<ContentBrowserAsset> assetItem = item.As<ContentBrowserAsset>();
					const AssetMetaData& assetMetadata = assetItem->GetAssetInfo();

					if (m_ItemActivationCallbacks.contains(assetMetadata.Type))
					{
						m_ItemActivationCallbacks.at(assetMetadata.Type)(assetMetadata);
					}
					else
					{
						AssetEditorPanel::OpenEditor(AssetManager::GetAsset<Asset>(assetMetadata.Handle));
					}
				}
			}
		}

		if (s_OpenDeleteDialog)
		{
			ImGui::OpenPopup("Delete");
			s_OpenDeleteDialog = false;
		}
	}

	void ContentBrowserPanel::RenderBottomBar(float height)
	{
		UI::ImGuiScopedStyle childBorder(ImGuiStyleVar_ChildBorderSize, 0.0f);
		UI::ImGuiScopedStyle frameBorder(ImGuiStyleVar_FrameBorderSize, 0.0f);
		UI::ImGuiScopedStyle itemSpacing(ImGuiStyleVar_ItemSpacing, { 0.0f, 0.0f });
		UI::ImGuiScopedStyle framePadding(ImGuiStyleVar_FramePadding, { 0.0f, 0.0f });

		ImGui::BeginChild("##bottom_bar", { 0.0f, height });
		ImGui::BeginHorizontal("##bottom_bar");
		{
			std::size_t itemsInDir = m_CurrentItems.GetItems().size();

			std::size_t selectionCount = SelectionManager::GetSelectionCount(SelectionContext::ContentBrowser);
			if (selectionCount == 1)
			{
				// Render filepath
				AssetHandle firstSelectionHandle = SelectionManager::GetSelection(SelectionContext::ContentBrowser, 0);
				const AssetMetaData& assetMetaData = Project::GetEditorAssetManager()->GetMetaData(firstSelectionHandle);

				std::string filePath;
				if (assetMetaData.IsValid())
				{
					filePath = fmt::format("{} Items | Assets/{}", itemsInDir, assetMetaData.FilePath.string());
				}
				else if (m_Directories.contains(firstSelectionHandle))
				{
					filePath = fmt::format("{} Items | Assets/{}", itemsInDir, m_Directories.at(firstSelectionHandle)->FilePath.string());
				}

				std::replace(filePath.begin(), filePath.end(), '\\', '/');
				ImGui::TextUnformatted(filePath.c_str());
			}
			else
			{
				// Render how many items selected
				ImGui::TextUnformatted(fmt::format("{} Items | {} items selected", itemsInDir, selectionCount).c_str());
			}
		}
		ImGui::EndHorizontal();
		ImGui::EndChild();
	}

	void ContentBrowserPanel::RenderDeleteDialogue()
	{
		if (ImGui::BeginPopupModal("Delete", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove))
		{
			if (SelectionManager::GetSelectionCount(SelectionContext::ContentBrowser) == 0)
				ImGui::CloseCurrentPopup();

			ImGui::Text("Are you sure you want to delete %d items?", SelectionManager::GetSelectionCount(SelectionContext::ContentBrowser));

			constexpr float buttonWidth = 60.0f;
			const float contentRegionWidth = ImGui::GetContentRegionAvail().x;

			UI::ShiftCursorX(((contentRegionWidth - (buttonWidth * 2.0f)) / 2.0f) - ImGui::GetStyle().ItemSpacing.x);
			if (ImGui::Button("Yes", { buttonWidth, 0.0f }) || Input::IsKeyDown(KeyCode::Enter))
			{
				const std::vector<UUID>& selectedItems = SelectionManager::GetSelections(SelectionContext::ContentBrowser);
				for (AssetHandle handle : selectedItems)
				{
					std::size_t index = m_CurrentItems.Find(handle);
					if (index == Utils::ItemList::s_InvalidItem)
						continue;

					m_CurrentItems[index]->Delete();
					m_CurrentItems.Erase(handle);
				}

				for (AssetHandle handle : selectedItems)
				{
					if (m_Directories.find(handle) != m_Directories.end())
						RemoveDirectory(m_Directories.at(handle));
				}

				SelectionManager::DeselectAll(SelectionContext::ContentBrowser);
				Refresh();

				ImGui::CloseCurrentPopup();
			}

			ImGui::SameLine();

			ImGui::SetItemDefaultFocus();
			if (ImGui::Button("No", { buttonWidth, 0.0f }))
				ImGui::CloseCurrentPopup();

			ImGui::EndPopup();
		}
	}

	void ContentBrowserPanel::RemoveDirectory(Ref<DirectoryInfo>& directory, bool removeFromParent)
	{
		if (directory->Parent && removeFromParent)
		{
			auto& childList = directory->Parent->SubDirectories;
			childList.erase(childList.find(directory->Handle));
		}

		for (auto& [handle, subDir] : directory->SubDirectories)
			RemoveDirectory(subDir, false);

		directory->SubDirectories.clear();
		directory->Assets.clear();
		
		m_Directories.erase(m_Directories.find(directory->Handle));
	}

	void ContentBrowserPanel::GenerateThumbnails()
	{
		struct Thumbnail
		{
			Ref<ContentBrowserItem> Item;
			AssetHandle Handle;
		};

		std::vector<Thumbnail> thumbnails;

		for (const Ref<ContentBrowserItem>& item : m_CurrentItems)
		{
			if (item->GetItemType() == ContentBrowserItem::ItemType::Asset)
			{
				AssetHandle handle = item->GetID();
				if (!AssetManager::IsAssetHandleValid(handle))
					continue;

				bool isThumbnailSupported = m_ThumbnailGenerator->IsThumbnailSupported(AssetManager::GetAssetType(handle));
				if (!isThumbnailSupported)
					continue;

				// TODO: What do we do here? Should we check timestamps or wait till file watcher thingy is done??
				//if (!m_ThumbnailCache->IsThumbnailCurrent(handle))
				if(!m_ThumbnailCache->HasThumbnail(handle))
				{
					// Queue thumbnail to be generated
					thumbnails.emplace_back(Thumbnail{ item, handle });
					break; // One thumbnail per frame
				}
			}
		}

		for (auto& thumbnail : thumbnails)
		{
			// TODO:
			//uint64_t timestamp = m_ThumbnailCache->GetAssetTimestamp(thumbnail.Handle);
			uint64_t timestamp = 0;
			Ref<Texture2D> thumbnailImage = m_ThumbnailGenerator->GenerateThumbnail(thumbnail.Handle);
			// we set thumbnail image even if it's nullptr (e.g. invalid asset)
			// that way we at least store the timestamp and won't attempt to generate the thumbnail
			// again until the asset timestamp changes
			m_ThumbnailCache->SetThumbnailImage(thumbnail.Handle, thumbnailImage, timestamp);
		}
	}

}