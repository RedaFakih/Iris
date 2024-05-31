#include "IrisPCH.h"
#include "SceneHierarchyPanel.h"

#include "AssetManager/AssetManager.h"
#include "Core/Application.h"
#include "Core/Events/KeyEvents.h"
#include "Core/Input/Input.h"
#include "ImGui/CustomTreeNode.h"
#include "ImGui/FontAwesome.h"
#include "ImGui/ImGuiUtils.h"
#include "Project/Project.h"

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

namespace Iris {

	static bool s_ActivateSearchWidget = false;
	SelectionContext SceneHierarchyPanel::s_ActiveSelectionContext = SelectionContext::Scene;

	Ref<SceneHierarchyPanel> SceneHierarchyPanel::Create()
	{
		return CreateRef<SceneHierarchyPanel>();
	}

	Ref<SceneHierarchyPanel> SceneHierarchyPanel::Create(const Ref<Scene> scene, SelectionContext selectionContext, bool isWindow)
	{
		return CreateRef<SceneHierarchyPanel>(scene, selectionContext, isWindow);
	}

	SceneHierarchyPanel::SceneHierarchyPanel(const Ref<Scene> scene, SelectionContext selectionContext, bool isWindow)
		: m_Context(scene), m_SelectionContext(selectionContext), m_IsWindow(isWindow)
	{
		if (m_Context)
			m_Context->SetEntityDestroyedCallback([this](Entity entity) { OnExternalEntityDestroyed(entity); });

		m_ComponentCopyScene = Scene::CreateEmpty();
		m_ComponentCopyEntity = m_ComponentCopyScene->CreateEntity();
	}

	void SceneHierarchyPanel::SetSceneContext(const Ref<Scene>& scene)
	{
		m_Context = scene;
		if (m_Context)
			m_Context->SetEntityDestroyedCallback([this](Entity entity) { OnExternalEntityDestroyed(entity); });
	}

	void SceneHierarchyPanel::OnImGuiRender(bool& isOpen)
	{
		if (m_IsWindow)
		{
			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 0.0f, 0.0f });
			ImGui::Begin("Scene Hierarchy", &isOpen, ImGuiWindowFlags_NoCollapse);

			m_IsWindowHovered = ImGui::IsWindowHovered();
			m_IsWindowFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
		}

		s_ActiveSelectionContext = m_SelectionContext;

		ImRect windowRect = { ImGui::GetWindowContentRegionMin(), ImGui::GetWindowContentRegionMax() };

		{
			constexpr float edgeOffset = 4.0f;
			UI::ShiftCursor(edgeOffset * 3.0f, edgeOffset * 2.0f);

			if (s_ActivateSearchWidget)
			{
				ImGui::SetKeyboardFocusHere();
				s_ActivateSearchWidget = false;
			}

			ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - edgeOffset * 3.0f);
			static std::string searchString;
			UI::SearchWidget(searchString);

			ImGui::Spacing();
			ImGui::Spacing();

			// Entity list...
			UI::ImGuiScopedStyle cellPadding(ImGuiStyleVar_CellPadding, { 4.0f, 0.0f });

			// Alternating row colors...
			const ImU32 colRowAlternating = UI::ColorWithMultipliedValue(Colors::Theme::BackgroundDark, 1.3f);
			UI::ImGuiScopedColor tableBGAlternating(ImGuiCol_TableRowBgAlt, colRowAlternating);

			// Table
			{
				// Scrollable table uses child window internally
				UI::ImGuiScopedColor tableBg(ImGuiCol_ChildBg, Colors::Theme::BackgroundDark);

				ImGuiTableFlags tableFlags = ImGuiTableFlags_NoPadInnerX
					| ImGuiTableFlags_Resizable
					| ImGuiTableFlags_Reorderable
					| ImGuiTableFlags_ScrollY
			        | ImGuiTableFlags_RowBg
				 // | ImGuiTableFlags_Sortable
				    ;
				
				constexpr int numColumns = 3;
				if (ImGui::BeginTable("##SceneHierarchy-Table", numColumns, tableFlags, ImGui::GetContentRegionAvail()))
				{
					ImGui::TableSetupColumn("Label");
					ImGui::TableSetupColumn("Type");
					ImGui::TableSetupColumn("Visibility");

					// Headers...
					{
						const ImU32 colActive = UI::ColorWithMultipliedValue(Colors::Theme::Selection, 1.2f);
						ImGui::PushStyleColor(ImGuiCol_HeaderHovered, colActive);
						ImGui::PushStyleColor(ImGuiCol_HeaderActive, colActive);

						ImGui::TableSetupScrollFreeze(ImGui::TableGetColumnCount(), 1);

						ImGui::TableNextRow(ImGuiTableRowFlags_Headers, 22.0f);
						for (int column = 0; column < ImGui::TableGetColumnCount(); column++)
						{
							ImGui::TableSetColumnIndex(column);

							const char* columnName = ImGui::TableGetColumnName(column);
							UI::ImGuiScopedID columnID(column);

							UI::ShiftCursor(edgeOffset * 3.0f, edgeOffset * 2.0f);
							ImGui::TableHeader(columnName, 0, Colors::Theme::Background);
							UI::ShiftCursor(-edgeOffset * 3.0f, -edgeOffset * 2.0f);
						}
						ImGui::SetCursorPosX(ImGui::GetCurrentTable()->OuterRect.Min.x);
						UI::UnderLine(true, 0.0f, 5.0f);

						ImGui::PopStyleColor(2);
					}

					// List...
					{
						UI::ImGuiScopedColor entitySelection1(ImGuiCol_Header, IM_COL32_DISABLE);
						UI::ImGuiScopedColor entitySelection2(ImGuiCol_HeaderHovered, IM_COL32_DISABLE);
						UI::ImGuiScopedColor entitySelection3(ImGuiCol_HeaderActive, IM_COL32_DISABLE);

						for (auto entity : m_Context->GetAllEntitiesWith<IDComponent, RelationshipComponent>())
						{
							Entity e{ entity, m_Context.Raw() };
							if (e.GetParentUUID() == 0)
								DrawEntityNode({ entity, m_Context.Raw() }, searchString);
						}
					}

					if (!ImGui::IsAnyItemHovered())
						if (ImGui::IsMouseReleased(ImGuiMouseButton_Right) && ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByPopup))
							m_OpenEntityCreateMenuPopup = true;

					if (m_OpenEntityCreateMenuPopup)
					{
						if (!ImGui::IsAnyItemHovered())
							ImGui::OpenPopup("draw_entity_create_menu_popup");
					}

					ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 8.0f, 6.0f });
					if (ImGui::BeginPopup("draw_entity_create_menu_popup"))
					{
						DrawEntityCreateMenu({});
						ImGui::EndPopup();

						m_OpenEntityCreateMenuPopup = false;
					}
					ImGui::PopStyleVar();

					ImGui::EndTable();
				}
			}

			m_WindowBounds = ImGui::GetCurrentWindow()->Rect();
		}

		if (ImGui::BeginDragDropTargetCustom(windowRect, ImGui::GetCurrentWindow()->ID))
		{
			const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("scene_entity_hierarchy", ImGuiDragDropFlags_AcceptNoDrawDefaultRect);

			if (payload)
			{
				std::size_t count = payload->DataSize / sizeof(UUID);

				for (std::size_t i = 0; i < count; i++)
				{
					UUID entityID = *(((UUID*)payload->Data) + i);
					Entity entity = m_Context->GetEntityWithUUID(entityID);
					m_Context->UnparentEntity(entity);
				}
			}

			ImGui::EndDragDropTarget();
		}

		{
			UI::ImGuiScopedStyle windowPadding(ImGuiStyleVar_WindowPadding, { 0.0f, 0.0f });
			ImGui::Begin("Properties", nullptr, ImGuiWindowFlags_NoCollapse);
			m_IsHierarchyOrPropertiesFocused = m_IsWindowFocused || ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
			DrawComponents(SelectionManager::GetSelections(s_ActiveSelectionContext));
			ImGui::End();
		}

		if (m_IsWindow)
		{
			ImGui::End();
			ImGui::PopStyleVar();
		}

		// Go through entity deletion queue
		for (uint32_t i = 0; i < static_cast<uint32_t>(m_EntityDeletionQueue.size()); i++)
			m_Context->DestroyEntity(m_Context->GetEntityWithUUID(m_EntityDeletionQueue[i]));
		m_EntityDeletionQueue.clear();
	}

	void SceneHierarchyPanel::OnEvent(Events::Event& e)
	{
		Events::EventDispatcher dispatcher(e);

		dispatcher.Dispatch<Events::MouseButtonReleasedEvent>([&](Events::MouseButtonReleasedEvent& e)
		{
			switch (e.GetMouseButton())
			{
				case MouseButton::Right:
				{
					if (m_CurrentlyRenderingOnlyViewport)
						break;

					if (m_IsWindowHovered)
						break;

					if (Input::IsKeyDown(KeyCode::LeftShift))
						m_OpenEntityCreateMenuPopup = true;

					return true;
				}
			}

			return false;
		});

		dispatcher.Dispatch<Events::RenderViewportOnlyEvent>([&](Events::RenderViewportOnlyEvent& e)
		{
			m_CurrentlyRenderingOnlyViewport = e.GetViewportOnlyFlag();

			return false;
		});

		dispatcher.Dispatch<Events::KeyPressedEvent>([&](Events::KeyPressedEvent& e)
		{
			switch (e.GetKeyCode())
			{
				case KeyCode::A:
				{
					if (m_CurrentlyRenderingOnlyViewport)
						break;

					if (!Input::IsKeyDown(KeyCode::LeftShift))
						break;

					m_OpenEntityCreateMenuPopup = true;

					return true;
				}
			}

			return false;
		});

		if (!m_IsWindowFocused)
			return;

		dispatcher.Dispatch<Events::MouseButtonReleasedEvent>([&](Events::MouseButtonReleasedEvent& event)
		{
			if (ImGui::IsMouseHoveringRect(m_WindowBounds.Min, m_WindowBounds.Max, false) && !ImGui::IsAnyItemHovered())
			{
				m_FirstSelectedRow = -1;
				m_LastSelectedRow = -1;
				SelectionManager::DeselectAll();
				return true;
			}

			return false;
		});

		dispatcher.Dispatch<Events::KeyPressedEvent>([&](Events::KeyPressedEvent& e)
		{
			if (!m_IsWindowFocused)
				return false;

			switch (e.GetKeyCode())
			{
				case KeyCode::F:
				{
					if (Input::IsKeyDown(KeyCode::LeftControl))
					{
						s_ActivateSearchWidget = true;
						return true;
					}

					break;
				}
				case KeyCode::Escape:
				{
					m_FirstSelectedRow = -1;
					m_LastSelectedRow = -1;
					break;
				}
			}

			return false;
		});
	}

	// Switch to be its own imgui panel that we pass in the position of where to create it that way we could create it anywhere inside the window like in blender with Shift+A
	void SceneHierarchyPanel::DrawEntityCreateMenu(Entity parent)
	{
		auto beginSection = [](const char* name, int& sectionIndex, bool columns2 = true, float column1Width = 0.0f, float column2Width = 0.0f)
		{
			constexpr float popupWidth = 310.0f;

			ImGuiFontsLibrary& fontsLib = Application::Get().GetImGuiLayer()->GetFontsLibrary();

			if (sectionIndex > 0)
				UI::ShiftCursorY(5.5f);

			fontsLib.PushFont("RobotoBold");

			float halfHeight = ImGui::CalcTextSize(name).y / 2.0f;
			ImVec2 p1 = { 0.0f, halfHeight };
			ImVec2 p2 = { 14.0f, halfHeight };
			UI::UnderLine(Colors::Theme::TextDarker, false, p1, p2, 3.0f);
			UI::ShiftCursorX(17.0f);

			ImGui::TextUnformatted(name);

			fontsLib.PopFont();

			ImVec2 cursorPos = ImGui::GetCursorPos();
			ImGui::SameLine();

			UI::UnderLine(false, 3.0f, halfHeight, 3.0f, Colors::Theme::TextDarker);
			ImGui::SetCursorPos(cursorPos);

			bool result = ImGui::BeginTable("##section_table", columns2 ? 2 : 1, ImGuiTableFlags_SizingStretchSame);
			if (result)
			{
				ImGui::TableSetupColumn("Labels", ImGuiTableColumnFlags_WidthFixed, column1Width == 0.0f ? popupWidth * 0.5f : column1Width);
				if (columns2)
					ImGui::TableSetupColumn("Widgets", ImGuiTableColumnFlags_WidthFixed, column2Width == 0.0f ? popupWidth * 0.5f : column2Width);
			}

			sectionIndex++;
			return result;
		};

		auto endSection = []()
		{
			ImGui::EndTable();
		};

		int index = 0;
		if (beginSection(parent ? "Add Child Entity" : "Add Entity", index, false, parent ? 270.0f : 0.0f))
		{
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);

			Entity newEntity;

			if (ImGui::MenuItem("Empty Entity"))
			{
				newEntity = m_Context->CreateEntity("Empty Entity");
			}

			if (ImGui::BeginMenu("Camera"))
			{
				if (ImGui::MenuItem("From View"))
				{
					newEntity = m_Context->CreateEntity("Camera");
					newEntity.AddComponent<CameraComponent>();

					for (auto& func : m_EntityContextMenuPlugins)
						func(newEntity);
				}

				if (ImGui::MenuItem("At World Origin"))
				{
					newEntity = m_Context->CreateEntity("Camera");
					newEntity.AddComponent<CameraComponent>();
				}

				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("2D"))
			{
				// TODO: Add lines and circles

				if (ImGui::MenuItem("Sprite"))
				{
					newEntity = m_Context->CreateEntity("Sprite");
					newEntity.AddComponent<SpriteRendererComponent>();
				}

				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("3D"))
			{
				auto create3DEntity = [this](const char* entityName, const char* targetAssetName, const char* sourceAssetName)
				{
					Entity entity = m_Context->CreateEntity(entityName);

					std::filesystem::path sourcePath = "Meshes/Default/Source";
					std::filesystem::path targetPath = "Meshes/Default";

					// Check if we have loaded the mesh before and serialized the Iris mesh file
					AssetHandle mesh = Project::GetEditorAssetManager()->GetAssetHandleFromFilePath(targetPath / targetAssetName);
					if (mesh != 0)
					{
						// Load mesh from Iris mesh file
						entity.AddComponent<StaticMeshComponent>(mesh);
					}
					else
					{
						// Load mesh from source file
						if (FileSystem::Exists(Project::GetAssetDirectory() / sourcePath / sourceAssetName))
						{
							AssetHandle handle = Project::GetEditorAssetManager()->GetAssetHandleFromFilePath(sourcePath / sourceAssetName);
							// Load the primitive 3D objects synchronously since they are not large and we need to create the .Ismesh files out of them after they are loaded
							if (AssetManager::GetAsset<StaticMesh>(handle))
							{
								Ref<StaticMesh> staticMesh = Project::GetEditorAssetManager()->CreateNewAsset<StaticMesh>(targetAssetName, (Project::GetAssetDirectory() / targetPath).string(), handle);
								entity.AddComponent<StaticMeshComponent>(staticMesh->Handle);
							}
						}
					}

					return entity;
				};

				if (ImGui::MenuItem("Cube"))
				{
					newEntity = create3DEntity("Cube", "Cube.Ismesh", "Cube.gltf");
				}

				if (ImGui::MenuItem("Sphere"))
				{
					newEntity = create3DEntity("Sphere", "Sphere.Ismesh", "Sphere.gltf");
				}

				if (ImGui::MenuItem("Plane"))
				{
					newEntity = create3DEntity("Plane", "Plane.Ismesh", "Plane.gltf");
				}

				if (ImGui::MenuItem("Cone"))
				{
					newEntity = create3DEntity("Cone", "Cone.Ismesh", "Cone.gltf");
				}

				if (ImGui::MenuItem("Cylinder"))
				{
					newEntity = create3DEntity("Cylinder", "Cylinder.Ismesh", "Cylinder.gltf");
				}

				if (ImGui::MenuItem("Torus"))
				{
					newEntity = create3DEntity("Torus", "Torus.Ismesh", "Torus.gltf");
				}

				if (ImGui::MenuItem("Capsule"))
				{
					newEntity = create3DEntity("Capsule", "Capsule.Ismesh", "Capsule.gltf");
				}

				ImGui::EndMenu();
			}

			if (newEntity)
			{
				if (parent)
				{
					m_Context->ParentEntity(newEntity, parent);
					newEntity.Transform().Translation = glm::vec3(0.0f);
				}

				SelectionManager::DeselectAll();
				SelectionManager::Select(s_ActiveSelectionContext, newEntity.GetUUID());
			}

			endSection();
		}
	}

	bool SceneHierarchyPanel::TagSearchRecursive(Entity entity, std::string_view searchFilter, uint32_t maxSearchDepth, uint32_t currentDepth)
	{
		if (searchFilter.empty())
			return false;

		for (auto child : entity.Children())
		{
			Entity e = m_Context->GetEntityWithUUID(child);
			if (e.HasComponent<TagComponent>())
			{
				if (UI::IsMatchingSearch(e.GetComponent<TagComponent>().Tag, searchFilter))
					return true;
			}

			bool found = TagSearchRecursive(e, searchFilter, maxSearchDepth, currentDepth + 1);
			if (found)
				return true;
		}
		return false;
	}

	void SceneHierarchyPanel::DrawEntityNode(Entity entity, const std::string& searchFilter)
	{
		const char* name = "Unnamed Entity";
		if (entity.HasComponent<TagComponent>())
			name = entity.GetComponent<TagComponent>().Tag.c_str();

		constexpr uint32_t maxSearchDepth = 10;
		bool hasChildMatchingSearch = TagSearchRecursive(entity, searchFilter, maxSearchDepth);

		if (!UI::IsMatchingSearch(name, searchFilter) && !hasChildMatchingSearch)
			return;

		constexpr float edgeOffset = 4.0f;
		constexpr float rowHeight = 21.0f;

		// ImGui Item height tweaks
		auto* window = ImGui::GetCurrentWindow();
		window->DC.CurrLineSize.y = rowHeight;

		ImGui::TableNextRow(0, rowHeight);

		// Label column
		ImGui::TableNextColumn();

		window->DC.CurrLineTextBaseOffset = 3.0f;

		const ImVec2 rowAreaMin = ImGui::TableGetCellBgRect(ImGui::GetCurrentTable(), 0).Min;
		const ImVec2 rowAreaMax = { ImGui::TableGetCellBgRect(ImGui::GetCurrentTable(), ImGui::TableGetColumnCount() - 1).Max.x - 20, rowAreaMin.y + rowHeight };

		const bool isSelected = SelectionManager::IsSelected(s_ActiveSelectionContext, entity.GetUUID());

		ImGuiTreeNodeFlags flags = (isSelected ? ImGuiTreeNodeFlags_Selected : 0) | ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_AllowItemOverlap;
		flags |= ImGuiTreeNodeFlags_SpanAvailWidth;

		if (hasChildMatchingSearch)
			flags |= ImGuiTreeNodeFlags_DefaultOpen;

		if (entity.Children().empty())
			flags |= ImGuiTreeNodeFlags_Leaf;

		const std::string strID = fmt::format("{0}{1}", name, (uint64_t)entity.GetUUID());

		ImGui::PushClipRect(rowAreaMin, rowAreaMax, false);
		bool isRowHovered, held;
		bool isRowClicked = ImGui::ButtonBehavior(
			ImRect(rowAreaMin, rowAreaMax),
			ImGui::GetID(strID.c_str()),
			&isRowHovered,
			&held,
			ImGuiButtonFlags_AllowItemOverlap | ImGuiButtonFlags_PressedOnClickRelease | ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight
		);

		bool wasRowRightClicked = ImGui::IsMouseReleased(ImGuiMouseButton_Right);

		ImGui::SetItemAllowOverlap();

		ImGui::PopClipRect();

		const bool isWindowFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

		// Row coloring...

		auto isAnyDescendantSelected = [&](Entity entity, auto isAnyDescendantSelected) -> bool
		{
			if (SelectionManager::IsSelected(s_ActiveSelectionContext, entity.GetUUID()))
				return true;

			if (!entity.Children().empty())
			{
				for (auto& childEntityID : entity.Children())
				{
					Entity childEntity = m_Context->GetEntityWithUUID(childEntityID);
					if (isAnyDescendantSelected(childEntity, isAnyDescendantSelected))
						return true;
				}
			}

			return false;
		};

		auto fillRowWithColor = [](const ImColor& color)
		{
			for (int column = 0; column < ImGui::TableGetColumnCount(); column++)
				ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, color, column);
		};

		if (isSelected)
		{
			if (isWindowFocused || UI::NavigatedTo())
				fillRowWithColor(Colors::Theme::Selection);
			else
			{
				const ImColor col = UI::ColorWithMultipliedValue(Colors::Theme::Selection, 0.9f);
				fillRowWithColor(col);
			}
		}
		else if (isRowHovered)
		{
			fillRowWithColor(Colors::Theme::GroupHeader);
		}
		else if (isAnyDescendantSelected(entity, isAnyDescendantSelected))
		{
			fillRowWithColor(Colors::Theme::SelectionMuted);
		}

		// Text coloring...
		if (isSelected)
			ImGui::PushStyleColor(ImGuiCol_Text, Colors::Theme::BackgroundDark);

		bool isMeshValid = true;
		// NOTE: We always highlight unset meshes
		{
			if (entity.HasComponent<StaticMeshComponent>())
				isMeshValid = AssetManager::IsAssetValid(entity.GetComponent<StaticMeshComponent>().StaticMesh, true);

			if (!isMeshValid)
				ImGui::PushStyleColor(ImGuiCol_Text, Colors::Theme::MeshNotSet);
		}

		// Tree Node...
		ImGuiContext& g = *GImGui;
		auto& style = ImGui::GetStyle();
		const ImVec2 label_size = ImGui::CalcTextSize(strID.c_str(), nullptr, false);
		const ImVec2 padding = ((flags & ImGuiTreeNodeFlags_FramePadding)) ? style.FramePadding : ImVec2(style.FramePadding.x, ImMin(window->DC.CurrLineTextBaseOffset, style.FramePadding.y));
		const float text_offset_x = g.FontSize + padding.x * 2;           // Collapser arrow width + Spacing
		const float text_offset_y = ImMax(padding.y, window->DC.CurrLineTextBaseOffset);                    // Latch before ItemSize changes it
		const float text_width = g.FontSize + (label_size.x > 0.0f ? label_size.x + padding.x * 2 : 0.0f);  // Include collapser
		ImVec2 text_pos(window->DC.CursorPos.x + text_offset_x, window->DC.CursorPos.y + text_offset_y);
		const float arrow_hit_x1 = (text_pos.x - text_offset_x) - style.TouchExtraPadding.x;
		const float arrow_hit_x2 = (text_pos.x - text_offset_x) + (g.FontSize + padding.x * 2.0f) + style.TouchExtraPadding.x;
		const bool is_mouse_x_over_arrow = (g.IO.MousePos.x >= arrow_hit_x1 && g.IO.MousePos.x < arrow_hit_x2);

		bool previousState = ImGui::TreeNodeBehaviorIsOpen(ImGui::GetID(strID.c_str()));

		if (is_mouse_x_over_arrow && isRowClicked)
			ImGui::SetNextItemOpen(!previousState);

		if (!isSelected && isAnyDescendantSelected(entity, isAnyDescendantSelected))
			ImGui::SetNextItemOpen(true);

		const bool opened = ImGui::TreeNodeWithIcon(nullptr, ImGui::GetID(strID.c_str()), flags, name, nullptr);

		int32_t rowIndex = ImGui::TableGetRowIndex();
		if (rowIndex >= m_FirstSelectedRow && rowIndex <= m_LastSelectedRow && !SelectionManager::IsSelected(entity.GetUUID()) && m_ShiftSelectionRunning)
		{
			SelectionManager::Select(s_ActiveSelectionContext, entity.GetUUID());

			if (SelectionManager::GetSelectionCount(s_ActiveSelectionContext) == (m_LastSelectedRow - m_FirstSelectedRow) + 1)
			{
				m_ShiftSelectionRunning = false;
			}
		}

		const std::string rightClickPopupID = fmt::format("{0}-ContextMenu", strID);
		
		bool entityDeleted = false;
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 8.0f, 6.0f });
		if (ImGui::BeginPopupContextItem(rightClickPopupID.c_str()))
		{
			{
				UI::ImGuiScopedColor colText(ImGuiCol_Text, Colors::Theme::Text);
				UI::ImGuiScopedColor header(ImGuiCol_Header, Colors::Theme::GroupHeader);
				UI::ImGuiScopedColor headerHovered(ImGuiCol_HeaderHovered, Colors::Theme::GroupHeader);
				UI::ImGuiScopedColor headerActive(ImGuiCol_HeaderActive, Colors::Theme::GroupHeader);

				if (!isSelected)
				{
					if (!Input::IsKeyDown(KeyCode::LeftControl))
						SelectionManager::DeselectAll();

					SelectionManager::Select(s_ActiveSelectionContext, entity.GetUUID());
				}

				if (entity.GetParent())
				{
					if (ImGui::MenuItem("Unparent"))
						m_Context->UnparentEntity(entity, true);
				}

				DrawEntityCreateMenu(entity);

				if (ImGui::MenuItem("Delete"))
					entityDeleted = true;

				if (!m_EntityContextMenuPlugins.empty())
				{
					UI::UnderLine();
					UI::ShiftCursorY(2.0f);

					if (ImGui::MenuItem("Set Transform to Editor Camera Transform"))
					{
						for (auto& func : m_EntityContextMenuPlugins)
						{
							func(entity);
						}
					}
				}
			}

			ImGui::EndPopup();
		}
		ImGui::PopStyleVar();

		if (isRowClicked)
		{
			if (wasRowRightClicked)
			{
				ImGui::OpenPopup(rightClickPopupID.c_str());
			}
			else
			{
				bool ctrlDown = Input::IsKeyDown(KeyCode::LeftControl) || Input::IsKeyDown(KeyCode::RightControl);
				bool shiftDown = Input::IsKeyDown(KeyCode::LeftShift) || Input::IsKeyDown(KeyCode::RightShift);

				if (shiftDown && SelectionManager::GetSelectionCount(s_ActiveSelectionContext) > 0)
				{
					SelectionManager::DeselectAll();

					if (rowIndex < m_FirstSelectedRow)
					{
						m_LastSelectedRow = m_FirstSelectedRow;
						m_FirstSelectedRow = rowIndex;
					}
					else
					{
						m_LastSelectedRow = rowIndex;
					}

					m_ShiftSelectionRunning = true;
				}
				else if (!ctrlDown || shiftDown)
				{
					SelectionManager::DeselectAll();
					SelectionManager::Select(s_ActiveSelectionContext, entity.GetUUID());
					m_FirstSelectedRow = rowIndex;
					m_LastSelectedRow = -1;
				}
				else
				{
					if (isSelected)
						SelectionManager::Deselect(s_ActiveSelectionContext, entity.GetUUID());
					else
						SelectionManager::Select(s_ActiveSelectionContext, entity.GetUUID());
				}
			}

			ImGui::FocusWindow(ImGui::GetCurrentWindow());
		}

		if (isSelected)
			ImGui::PopStyleColor();
		if (!isMeshValid)
			ImGui::PopStyleColor();

		// Drag Drop
		if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
		{
			const auto& selectedEntities = SelectionManager::GetSelections(s_ActiveSelectionContext);
			UUID entityID = entity.GetUUID();

			if (!SelectionManager::IsSelected(s_ActiveSelectionContext, entityID))
			{
				const char* name = entity.Name().c_str();
				ImGui::TextUnformatted(name);
				ImGui::SetDragDropPayload("scene_entity_hierarchy", &entityID, 1 * sizeof(UUID));
			}
			else
			{
				for (const auto& selectedEntity : selectedEntities)
				{
					Entity e = m_Context->GetEntityWithUUID(selectedEntity);
					const char* name = e.Name().c_str();
					ImGui::TextUnformatted(name);
				}

				ImGui::SetDragDropPayload("scene_entity_hierarchy", selectedEntities.data(), selectedEntities.size() * sizeof(UUID));
			}

			ImGui::EndDragDropSource();
		}

		if (ImGui::BeginDragDropTarget())
		{
			const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("scene_entity_hierarchy", ImGuiDragDropFlags_AcceptNoDrawDefaultRect);

			if (payload)
			{
				size_t count = payload->DataSize / sizeof(UUID);

				for (size_t i = 0; i < count; i++)
				{
					UUID droppedEntityID = *(((UUID*)payload->Data) + i);
					Entity droppedEntity = m_Context->GetEntityWithUUID(droppedEntityID);
					m_Context->ParentEntity(droppedEntity, entity);
				}
			}

			ImGui::EndDragDropTarget();
		}

		// TODO: Type Column fill with data about the type of the entity (Text? Icon?)
		ImGui::TableNextColumn();

		ImGui::TableNextColumn();

		m_CurrentlySelectedMeshViewIcon = EditorResources::EyeIcon;
		if (entity.HasComponent<StaticMeshComponent>())
		{
			ImVec2 avaiLRegion = ImGui::GetContentRegionAvail();
			StaticMeshComponent& smc = entity.GetComponent<StaticMeshComponent>();

			if (ImGui::InvisibleButton(UI::GenerateID(), { avaiLRegion.x, rowHeight }))
				smc.Visible = !smc.Visible;

			m_CurrentlySelectedMeshViewIcon = smc.Visible ? EditorResources::EyeIcon : EditorResources::ClosedEyeIcon;

			ImRect buttonRect = UI::GetItemRect();

			ImRect iconRect;
			iconRect.Min = { buttonRect.Min.x + (buttonRect.Max.x - buttonRect.Min.x) / 2.0f - m_CurrentlySelectedMeshViewIcon->GetWidth() / 2.0f, 
							 buttonRect.Min.y + (buttonRect.Max.y - buttonRect.Min.y) / 2.0f - m_CurrentlySelectedMeshViewIcon->GetHeight() / 2.0f };
			iconRect.Max = { buttonRect.Min.x + (buttonRect.Max.x - buttonRect.Min.x) / 2.0f + m_CurrentlySelectedMeshViewIcon->GetWidth() / 2.0f, 
							 buttonRect.Min.y + (buttonRect.Max.y - buttonRect.Min.y) / 2.0f + m_CurrentlySelectedMeshViewIcon->GetHeight() / 2.0f };
			UI::DrawButtonImage(m_CurrentlySelectedMeshViewIcon, Colors::Theme::TextBrighter, Colors::Theme::TextBrighter, Colors::Theme::TextBrighter, iconRect);
		}

		// Draw Children
		if (opened)
		{
			for (auto child : entity.Children())
				DrawEntityNode(m_Context->GetEntityWithUUID(child), "");

			ImGui::TreePop();
		}

		// Defer deletion until after node UI
		if (entityDeleted)
		{
			auto selectedEntities = SelectionManager::GetSelections(s_ActiveSelectionContext);
			for (auto entityID : selectedEntities)
				m_EntityDeletionQueue.push_back(entityID);
		}
	}

	namespace Utils {

		static bool DrawVec3Control(const std::string_view label, glm::vec3& value, bool& manuallyEdited, bool fullWidth = false, float resetValue = 0.0f, float speed = 1.0f, float columnWidth = 100.0f, UI::VectorAxis renderMultiSelectAxes = UI::VectorAxis::None)
		{
			bool modified = false;

			UI::PushID();

			ImGui::TableSetColumnIndex(0);
			UI::ShiftCursor(17.0f, 7.0f);

			ImGui::Text(label.data());
			if (fullWidth)
				UI::ShiftCursorY(-2.0f);
			UI::UnderLine(fullWidth, 0.0f, 2.0f);

			ImGui::TableSetColumnIndex(1);
			UI::ShiftCursor(7.0f, 0.0f);

			modified = UI::EditVec3(label, ImVec2{ImGui::GetContentRegionAvail().x - 8.0f, ImGui::GetFrameHeightWithSpacing() + 8.0f}, resetValue, manuallyEdited, value, renderMultiSelectAxes, speed);

			UI::PopID();

			return modified;
		}

		template<typename TComponent, typename... TIncompatibleComponents>
		void DrawSimpleAddComponentButton(SceneHierarchyPanel* panel, const std::string& name, Ref<Texture2D> icon = nullptr)
		{
			bool canAddComponent = false;

			for (const auto& entityID : SelectionManager::GetSelections(SceneHierarchyPanel::GetActiveSelectionContext()))
			{
				Entity entity = panel->GetSceneContext()->GetEntityWithUUID(entityID);
				if (!entity.HasComponent<TComponent>())
				{
					canAddComponent = true;
					break;
				}
			}

			if (!canAddComponent)
				return;

			if (icon == nullptr)
				icon = EditorResources::AssetIcon;

			const float rowHeight = 25.0f;
			auto* window = ImGui::GetCurrentWindow();
			window->DC.CurrLineSize.y = rowHeight;
			ImGui::TableNextRow(0, rowHeight);
			ImGui::TableSetColumnIndex(0);

			window->DC.CurrLineTextBaseOffset = 3.0f;

			const ImVec2 rowAreaMin = ImGui::TableGetCellBgRect(ImGui::GetCurrentTable(), 0).Min;
			const ImVec2 rowAreaMax = { ImGui::TableGetCellBgRect(ImGui::GetCurrentTable(), ImGui::TableGetColumnCount() - 1).Max.x,
										rowAreaMin.y + rowHeight };

			ImGui::PushClipRect(rowAreaMin, rowAreaMax, false);
			bool isRowHovered, held, isRowClicked = false;
			isRowClicked = ImGui::ButtonBehavior(
				{ rowAreaMin, rowAreaMax },
				ImGui::GetID(name.c_str()),
				&isRowHovered,
				&held,
				ImGuiButtonFlags_AllowItemOverlap | ImGuiButtonFlags_PressedOnClick
			);

			ImGui::SetItemAllowOverlap();
			ImGui::PopClipRect();

			auto fillRowWithColour = [](const ImColor& colour)
			{
				for (int column = 0; column < ImGui::TableGetColumnCount(); column++)
					ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, colour, column);
			};

			if (isRowHovered)
				fillRowWithColour(Colors::Theme::Background);

			UI::ShiftCursor(1.5f, 1.5f);
			UI::Image(icon, { rowHeight - 3.0f, rowHeight - 3.0f });
			UI::ShiftCursor(-1.5f, -1.5f);
			ImGui::TableSetColumnIndex(1);
			ImGui::SetNextItemWidth(-1);
			ImGui::TextUnformatted(name.c_str());

			if (isRowClicked)
			{
				for (const auto& entityID : SelectionManager::GetSelections(SceneHierarchyPanel::GetActiveSelectionContext()))
				{
					Entity entity = panel->GetSceneContext()->GetEntityWithUUID(entityID);

					if (sizeof...(TIncompatibleComponents) > 0 && entity.HasComponent<TIncompatibleComponents...>())
						continue;

					if (!entity.HasComponent<TComponent>())
						entity.AddComponent<TComponent>();
				}

				ImGui::CloseCurrentPopup();
			}
		}

		template<typename T>
		void DrawMaterialTable(SceneHierarchyPanel* panel, const std::vector<UUID>& entities, Ref<MaterialTable> matTable, Ref<MaterialTable> localMatTable)
		{
			ImGuiTreeNodeFlags treeNodeFlags = ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_DefaultOpen;
			if (ImGui::TreeNodeEx("Materials", treeNodeFlags))
			{
				UI::BeginPropertyGrid();

				if (localMatTable->GetMaterialCount() != matTable->GetMaterialCount())
					localMatTable->SetMaterialCount(matTable->GetMaterialCount());

				for (uint32_t i = 0; i < localMatTable->GetMaterialCount(); i++)
				{
					if (i == matTable->GetMaterialCount())
						ImGui::Separator();

					bool hasLocalMaterial = localMatTable->HasMaterial(i);
					bool hasMeshMaterial = matTable->HasMaterial(i);

					std::string label = std::format("[Material {0}]", i);

					std::string id = std::format("{0}-{1}", label, i);
					ImGui::PushID(id.c_str());

					UI::PropertyAssetReferenceSettings settings;
					if (hasMeshMaterial && !hasLocalMaterial)
					{
						AssetHandle meshMaterialAssetHandle = matTable->GetMaterial(i);
						Ref<MaterialAsset> meshMaterialAsset = AssetManager::GetAsset<MaterialAsset>(meshMaterialAssetHandle);

						std::string meshMaterialName = meshMaterialAsset->GetMaterial()->GetName();
						if (meshMaterialName.empty())
							meshMaterialName = "Unnamed Material";

						ImGui::PushItemFlag(ImGuiItemFlags_MixedValue, entities.size() > 1 && panel->IsInconsistentPrimitive<AssetHandle, T>([i](const T& component)
						{
							Ref<MaterialTable> materialTable = nullptr;
							materialTable = AssetManager::GetAsset<StaticMesh>(component.StaticMesh)->GetMaterials();

							if (!materialTable || i >= materialTable->GetMaterialCount())
								return static_cast<AssetHandle>(0);

							return materialTable->GetMaterial(i);
						}));

						AssetHandle materialAssetHandle = meshMaterialAsset->Handle;
						UI::PropertyAssetReferenceTarget<MaterialAsset>(label.c_str(), meshMaterialName.c_str(), materialAssetHandle, [panel, &entities, i, localMatTable](Ref<MaterialAsset> materialAsset) mutable
						{
							Ref<Scene> context = panel->GetSceneContext();

							for (auto entityID : entities)
							{
								Entity entity = context->GetEntityWithUUID(entityID);
								auto& component = entity.GetComponent<T>();

								if (materialAsset == UUID(0))
									component.MaterialTable->ClearMaterial(i);
								else
									component.MaterialTable->SetMaterial(i, materialAsset->Handle);
							}
						}, "", settings);

						ImGui::PopItemFlag();
					}
					else
					{
						AssetHandle materialAssetHandle = 0;
						if (hasLocalMaterial)
						{
							materialAssetHandle = localMatTable->GetMaterial(i);
							settings.AdvanceToNextColumn = false;
							settings.WidthOffset = ImGui::GetStyle().ItemSpacing.x + 28.0f;
						}

						ImGui::PushItemFlag(ImGuiItemFlags_MixedValue, entities.size() > 1 && panel->IsInconsistentPrimitive<AssetHandle, T>([i, localMatTable](const T& component)
						{
							Ref<MaterialTable> materialTable = component.MaterialTable;

							if (!materialTable || i >= materialTable->GetMaterialCount())
								return static_cast<AssetHandle>(0);

							if (!materialTable->HasMaterial(i))
								return static_cast<AssetHandle>(0);

							return materialTable->GetMaterial(i);
						}));

						UI::PropertyAssetReferenceTarget<MaterialAsset>(label.c_str(), nullptr, materialAssetHandle, [panel, &entities, i, localMatTable](Ref<MaterialAsset> materialAsset) mutable
						{
							Ref<Scene> context = panel->GetSceneContext();

							for (auto entityID : entities)
							{
								Entity entity = context->GetEntityWithUUID(entityID);
								auto& component = entity.GetComponent<T>();

								if (materialAsset == UUID(0))
									component.MaterialTable->ClearMaterial(i);
								else
									component.MaterialTable->SetMaterial(i, materialAsset->Handle);
							}
						}, "", settings);

						ImGui::PopItemFlag();
					}

					if (hasLocalMaterial)
					{
						ImGui::SameLine();

						float prevItemHeight = ImGui::GetItemRectSize().y;
						if (ImGui::Button(UI::GenerateLabelID("X"), { prevItemHeight, prevItemHeight }))
						{
							Ref<Scene> context = panel->GetSceneContext();

							for (auto entityID : entities)
							{
								Entity entity = context->GetEntityWithUUID(entityID);
								auto& component = entity.GetComponent<T>();

								component.MaterialTable->ClearMaterial(i);
							}
						}

						ImGui::NextColumn();
					}

					ImGui::PopID();
				}

				UI::EndPropertyGrid();
				ImGui::TreePop();
			}
		}

	}

	void SceneHierarchyPanel::DrawComponents(const std::vector<UUID>& entities)
	{
		if (entities.size() == 0)
			return;

		ImGui::AlignTextToFramePadding();

		ImVec2 contentRegionAvailable = ImGui::GetContentRegionAvail();

		UI::ShiftCursor(4.0f, 4.0f);

		const bool isMultiSelect = entities.size() > 1;

		Entity firstEntity = m_Context->GetEntityWithUUID(entities[0]);

		// Draw Tag field
		{
			constexpr float iconOffset = 6.0f;
			UI::ShiftCursor(4.0f, iconOffset);
			UI::Image(EditorResources::PencilIcon, { (float)EditorResources::PencilIcon->GetWidth(), (float)EditorResources::PencilIcon->GetHeight() }, { 0.0f, 0.0f }, { 1.0f, 1.0f }, ImColor(128, 128, 128, 255).Value);

			ImGui::SameLine(0.0f, 4.0f);
			UI::ShiftCursorY(-iconOffset);

			const bool inconsistentTag = IsInconsistentString<TagComponent>([](const TagComponent& tagComponent) {return tagComponent.Tag; });
			const std::string& tag = (isMultiSelect && inconsistentTag) ? "----" : firstEntity.Name();

			char buffer[256];
			memset(buffer, 0, 256);
			buffer[0] = 0; // Makes it easy to check if empty later.
			memcpy(buffer, tag.c_str(), tag.length());
			ImGui::PushItemWidth(contentRegionAvailable.x - 92.0f);
			UI::ImGuiScopedStyle frameBorder(ImGuiStyleVar_FrameBorderSize, 0.0f);
			UI::ImGuiScopedColor frameColour(ImGuiCol_FrameBg, IM_COL32(0, 0, 0, 0));
			UI::ImGuiScopedFont boldFont(Application::Get().GetImGuiLayer()->GetFontsLibrary().GetFont("RobotoBold"));

			if (Input::IsKeyDown(KeyCode::F2) && (m_IsHierarchyOrPropertiesFocused || UI::IsWindowFocused("Viewport")) && !ImGui::IsAnyItemActive())
				ImGui::SetKeyboardFocusHere();

			if (ImGui::InputText("##Tag", buffer, 256))
			{
				for (auto entityID : entities)
				{
					Entity entity = m_Context->GetEntityWithUUID(entityID);
					if (buffer[0] == 0)
						memcpy(buffer, "Unnamed Entity", 16); // if the entity has no name, the name will be set to Unnamed Entity, this prevents invisible entities in SHP.

					entity.GetComponent<TagComponent>().Tag = buffer;
				}
			}

			UI::DrawItemActivityOutline(UI::OutlineFlags_NoOutlineInactive);

			ImGui::PopItemWidth();
		}
		
		ImGui::SameLine();
		UI::ShiftCursorX(-5.0f);

		float lineHeight = GImGui->Font->FontSize + GImGui->Style.FramePadding.y * 2.0f;

		const char* buttonText = " ADD       ";
		ImVec2 addTextSize = ImGui::CalcTextSize(buttonText);
		addTextSize.x += GImGui->Style.FramePadding.x * 2.0f;

		{
			UI::ImGuiScopedColor colorButton(ImGuiCol_Button, IM_COL32(70, 70, 70, 200));
			UI::ImGuiScopedColor colorButtonHovered(ImGuiCol_ButtonHovered, IM_COL32(70, 70, 70, 255));
			UI::ImGuiScopedColor colorButtonActive(ImGuiCol_ButtonActive, IM_COL32(70, 70, 70, 150));

			ImGui::SameLine(contentRegionAvailable.x - (addTextSize.x + GImGui->Style.FramePadding.x));
			UI::ShiftCursorY(-7.17f);
			if (ImGui::Button(buttonText, { addTextSize.x + 4.0f, lineHeight + 5.3f }))
				ImGui::OpenPopup("AddComponentPanel");

			constexpr float pad = 4.0f;
			const float iconWidth = ImGui::GetFrameHeight() - pad * 2.0f;
			ImVec2 iconPos = ImGui::GetItemRectMax();
			iconPos.x -= iconWidth + pad;
			iconPos.y -= iconWidth + pad;

			ImGui::SetCursorScreenPos(iconPos);
			UI::ShiftCursor(-5.0f, -2.5f);

			UI::Image(EditorResources::PlusIcon, { iconWidth, iconWidth }, { 0.0f, 0.0f }, { 1.0f, 1.0f }, ImColor(160, 160, 160, 255).Value);
		}

		float addComponenetPanelStartY = ImGui::GetCursorScreenPos().y;

		ImGui::Spacing();
		ImGui::Spacing();
		ImGui::Spacing();

		{
			UI::ImGuiScopedFont boldFont(Application::Get().GetImGuiLayer()->GetFontsLibrary().GetFont("RobotoBold"));
			UI::ImGuiScopedStyle itemSpacing(ImGuiStyleVar_ItemSpacing, { 0.0f, 0.0f });
			UI::ImGuiScopedStyle windowPadding(ImGuiStyleVar_WindowPadding, { 5.0f, 10.0f });
			UI::ImGuiScopedStyle windowRounding(ImGuiStyleVar_WindowRounding, 4.0f);
			UI::ImGuiScopedStyle cellPadding(ImGuiStyleVar_CellPadding, { 0.0f, 0.0f });

			constexpr float addComponentPanelWidth = 250.0f;
			ImVec2 windowPos = ImGui::GetWindowPos();
			const float maxHeight = ImGui::GetContentRegionAvail().y - 60.0f;

			ImGui::SetNextWindowPos({ windowPos.x + addComponentPanelWidth / 1.3f, addComponenetPanelStartY });
			ImGui::SetNextWindowSizeConstraints({ 50.0f, 50.0f }, { addComponentPanelWidth, maxHeight });

			if (ImGui::BeginPopup("AddComponentPanel", ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDocking))
			{
				// Setup table
				if (ImGui::BeginTable("##component_table", 2, ImGuiTableFlags_SizingStretchSame))
				{
					ImGui::TableSetupColumn("Icon", ImGuiTableColumnFlags_WidthFixed, addComponentPanelWidth * 0.15f);
					ImGui::TableSetupColumn("ComponentNames", ImGuiTableColumnFlags_WidthFixed, addComponentPanelWidth * 0.85f);

					Utils::DrawSimpleAddComponentButton<CameraComponent>(this, "Camera", EditorResources::CameraIcon);
					Utils::DrawSimpleAddComponentButton<SpriteRendererComponent>(this, "Sprite Renderer", EditorResources::SpriteIcon);
					Utils::DrawSimpleAddComponentButton<StaticMeshComponent>(this, "StaticMesh", EditorResources::StaticMeshIcon);

					ImGui::EndTable();
				}

				ImGui::EndPopup();
			}
		}

		DrawComponent<TransformComponent>("Transform", [&](TransformComponent& firstComponent, const std::vector<UUID>& entities, const bool isMultiEdit)
		{
			UI::ImGuiScopedStyle spacing(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 8.0f));
			UI::ImGuiScopedStyle padding(ImGuiStyleVar_FramePadding, ImVec2(4.0f, 4.0f));

			ImGui::BeginTable("transformComponent", 2, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_NoClip);
			ImGui::TableSetupColumn("label_column", 0, 100.0f);
			ImGui::TableSetupColumn("value_column", ImGuiTableColumnFlags_IndentEnable | ImGuiTableColumnFlags_NoClip, ImGui::GetContentRegionAvail().x - 100.0f);

			bool translationManuallyEdited = false;
			bool rotationManuallyEdited = false;
			bool scaleManuallyEdited = false;

			if (isMultiEdit)
			{
				UI::VectorAxis translationAxes = GetInconsistentVectorAxis<glm::vec3, TransformComponent>([](const TransformComponent& other) { return other.Translation; });
				UI::VectorAxis rotationAxes = GetInconsistentVectorAxis<glm::vec3, TransformComponent>([](const TransformComponent& other) { return other.GetRotationEuler(); });
				UI::VectorAxis scaleAxes = GetInconsistentVectorAxis<glm::vec3, TransformComponent>([](const TransformComponent& other) { return other.Scale; });

				glm::vec3 translation = firstComponent.Translation;
				glm::vec3 rotation = glm::degrees(firstComponent.GetRotationEuler());
				glm::vec3 scale = firstComponent.Scale;

				glm::vec3 oldTranslation = translation;
				glm::vec3 oldRotation = rotation;
				glm::vec3 oldScale = scale;

				ImGui::TableNextRow();
				bool changed = Utils::DrawVec3Control("Translation", translation, translationManuallyEdited, false, 0.0f, 0.1f, 100.0f, translationAxes);

				ImGui::TableNextRow();
				changed |= Utils::DrawVec3Control("Rotation", rotation, rotationManuallyEdited, false, 0.0f, 0.1f, 100.0f, rotationAxes);

				ImGui::TableNextRow();
				changed |= Utils::DrawVec3Control("Scale", scale, scaleManuallyEdited, true, 1.0f, 0.1f, 100.0f, scaleAxes);

				if (changed)
				{
					if (translationManuallyEdited || rotationManuallyEdited || scaleManuallyEdited)
					{
						translationAxes = GetInconsistentVectorAxis(translation, oldTranslation);
						rotationAxes = GetInconsistentVectorAxis(rotation, oldRotation);
						scaleAxes = GetInconsistentVectorAxis(scale, oldScale);

						for (auto& entityID : entities)
						{
							Entity entity = m_Context->GetEntityWithUUID(entityID);
							auto& component = entity.GetComponent<TransformComponent>();

							if ((translationAxes & UI::VectorAxis::X) != UI::VectorAxis::None)
								component.Translation.x = translation.x;
							if ((translationAxes & UI::VectorAxis::Y) != UI::VectorAxis::None)
								component.Translation.y = translation.y;
							if ((translationAxes & UI::VectorAxis::Z) != UI::VectorAxis::None)
								component.Translation.z = translation.z;

							glm::vec3 componentRotation = component.GetRotationEuler();
							if ((rotationAxes & UI::VectorAxis::X) != UI::VectorAxis::None)
								componentRotation.x = glm::radians(rotation.x);
							if ((rotationAxes & UI::VectorAxis::Y) != UI::VectorAxis::None)
								componentRotation.y = glm::radians(rotation.y);
							if ((rotationAxes & UI::VectorAxis::Z) != UI::VectorAxis::None)
								componentRotation.z = glm::radians(rotation.z);
							component.SetRotationEuler(componentRotation);

							if ((scaleAxes & UI::VectorAxis::X) != UI::VectorAxis::None)
								component.Scale.x = scale.x;
							if ((scaleAxes & UI::VectorAxis::Y) != UI::VectorAxis::None)
								component.Scale.y = scale.y;
							if ((scaleAxes & UI::VectorAxis::Z) != UI::VectorAxis::None)
								component.Scale.z = scale.z;
						}
					}
					else
					{
						glm::vec3 translationDiff = translation - oldTranslation;
						glm::vec3 rotationDiff = rotation - oldRotation;
						glm::vec3 scaleDiff = scale - oldScale;

						for (auto& entityID : entities)
						{
							Entity entity = m_Context->GetEntityWithUUID(entityID);
							auto& component = entity.GetComponent<TransformComponent>();

							component.Translation += translationDiff;
							glm::vec3 componentRotation = component.GetRotationEuler();
							componentRotation += glm::radians(rotationDiff);
							component.SetRotationEuler(componentRotation);
							component.Scale += scaleDiff;
						}
					}
				}
			}
			else
			{
				Entity entity = m_Context->GetEntityWithUUID(entities[0]);
				auto& component = entity.GetComponent<TransformComponent>();

				ImGui::TableNextRow();
				Utils::DrawVec3Control("Translation", component.Translation, translationManuallyEdited, false, 0.0f, 0.1f);

				ImGui::TableNextRow();
				glm::vec3 rotation = glm::degrees(component.GetRotationEuler());
				if (Utils::DrawVec3Control("Rotation", rotation, rotationManuallyEdited, false, 0.0f, 0.1f))
					component.SetRotationEuler(glm::radians(rotation));

				ImGui::TableNextRow();
				Utils::DrawVec3Control("Scale", component.Scale, scaleManuallyEdited, true, 1.0f, 0.1f);
			}

			ImGui::EndTable();

			UI::ShiftCursorY(7.0f);
		}, EditorResources::TransformIcon);

		DrawComponent<CameraComponent>("Camera", [&](CameraComponent& camComponent, const std::vector<UUID>& entities, const bool isMultiEdit)
		{
			UI::BeginPropertyGrid();

			// Projection type
			static const char* projectionTypes[2] = { "Perspective", "Orthographic" };
			int currentPorj = static_cast<int>(camComponent.Camera.GetProjectionType());

			ImGui::PushItemFlag(ImGuiItemFlags_MixedValue, isMultiEdit && IsInconsistentPrimitive<int, CameraComponent>([](const CameraComponent& other) { return static_cast<int>(other.Camera.GetProjectionType()); }));

			if (UI::PropertyDropdown("Projection", projectionTypes, 2, &currentPorj))
			{
				for (auto& entityID : entities)
				{
					Entity entity = m_Context->GetEntityWithUUID(entityID);
					entity.GetComponent<CameraComponent>().Camera.SetProjectionType(static_cast<SceneCamera::ProjectionType>(currentPorj));
				}
			}

			ImGui::PopItemFlag();

			// Perspective Params...
			if (camComponent.Camera.GetProjectionType() == SceneCamera::ProjectionType::Perspective)
			{
				// Vertical FOV
				// Near Clip
				// Far Clip

				float verticalFOV = camComponent.Camera.GetDegPerspectiveVerticalFOV();
				ImGui::PushItemFlag(ImGuiItemFlags_MixedValue, isMultiEdit && IsInconsistentPrimitive<float, CameraComponent>([](const CameraComponent& other) { return other.Camera.GetDegPerspectiveVerticalFOV(); }));

				if (UI::Property("Vertical FOV", verticalFOV))
				{
					for (auto& entityID : entities)
					{
						Entity entity = m_Context->GetEntityWithUUID(entityID);
						entity.GetComponent<CameraComponent>().Camera.SetDegPerspectiveVerticalFOV(verticalFOV);
					}
				}

				ImGui::PopItemFlag();

				float nearClip = camComponent.Camera.GetPerspectiveNearClip();
				ImGui::PushItemFlag(ImGuiItemFlags_MixedValue, isMultiEdit && IsInconsistentPrimitive<float, CameraComponent>([](const CameraComponent& other) { return other.Camera.GetPerspectiveNearClip(); }));

				if (UI::Property("Near Clip", nearClip))
				{
					for (auto& entityID : entities)
					{
						Entity entity = m_Context->GetEntityWithUUID(entityID);
						entity.GetComponent<CameraComponent>().Camera.SetPerspectiveNearClip(nearClip);
					}
				}

				ImGui::PopItemFlag();

				float farClip = camComponent.Camera.GetPerspectiveFarClip();
				ImGui::PushItemFlag(ImGuiItemFlags_MixedValue, isMultiEdit && IsInconsistentPrimitive<float, CameraComponent>([](const CameraComponent& other) { return other.Camera.GetPerspectiveFarClip(); }));

				if (UI::Property("Far Clip", farClip))
				{
					for (auto& entityID : entities)
					{
						Entity entity = m_Context->GetEntityWithUUID(entityID);
						entity.GetComponent<CameraComponent>().Camera.SetPerspectiveFarClip(farClip);
					}
				}

				ImGui::PopItemFlag();
			}
			// Orthographic Params...
			else if (camComponent.Camera.GetProjectionType() == SceneCamera::ProjectionType::Orthographic)
			{
				// Orthographic Size
				// Near
				// Far

				float orthoSize = camComponent.Camera.GetOrthographicSize();
				ImGui::PushItemFlag(ImGuiItemFlags_MixedValue, isMultiEdit && IsInconsistentPrimitive<float, CameraComponent>([](const CameraComponent& other) { return other.Camera.GetOrthographicSize(); }));

				if (UI::Property("Size", orthoSize))
				{
					for (auto& entityID : entities)
					{
						Entity entity = m_Context->GetEntityWithUUID(entityID);
						entity.GetComponent<CameraComponent>().Camera.SetOrthographicSize(orthoSize);
					}
				}

				ImGui::PopItemFlag();

				float nearClip = camComponent.Camera.GetOrthographicNearClip();
				ImGui::PushItemFlag(ImGuiItemFlags_MixedValue, isMultiEdit && IsInconsistentPrimitive<float, CameraComponent>([](const CameraComponent& other) { return other.Camera.GetOrthographicNearClip(); }));

				if (UI::Property("Near Clip", nearClip))
				{
					for (auto& entityID : entities)
					{
						Entity entity = m_Context->GetEntityWithUUID(entityID);
						entity.GetComponent<CameraComponent>().Camera.SetOrthographicNearClip(nearClip);
					}
				}

				ImGui::PopItemFlag();

				float farClip = camComponent.Camera.GetOrthographicFarClip();
				ImGui::PushItemFlag(ImGuiItemFlags_MixedValue, isMultiEdit && IsInconsistentPrimitive<float, CameraComponent>([](const CameraComponent& other) { return other.Camera.GetOrthographicFarClip(); }));

				if (UI::Property("Far Clip", farClip))
				{
					for (auto& entityID : entities)
					{
						Entity entity = m_Context->GetEntityWithUUID(entityID);
						entity.GetComponent<CameraComponent>().Camera.SetOrthographicFarClip(farClip);
					}
				}

				ImGui::PopItemFlag();
			}

			ImGui::PushItemFlag(ImGuiItemFlags_MixedValue, isMultiEdit && IsInconsistentPrimitive<bool, CameraComponent>([](const CameraComponent& other) { return other.Primary; }));

			if (UI::Property("Main Camera", camComponent.Primary))
			{
				// If we set the current camera as the main camera then we need to go through all the other cameras make them NOT primary since
				// we should only have one primary camera
				auto cameraEntities = m_Context->GetAllEntitiesWith<CameraComponent>();
				for (auto& camera : cameraEntities)
				{
					// Assumes the camera is the first in the selection
					Entity entity = m_Context->GetEntityWithUUID(entities[0]);
					if (entity.m_EntityHandle == camera)
						continue;

					CameraComponent& comp = cameraEntities.get<CameraComponent>(camera);
					comp.Primary = false;
				}

				// If we have multi select then we ignore that and only set the first selected as primary
				auto& entityID = entities[0];
				Entity entity = m_Context->GetEntityWithUUID(entityID);
				entity.GetComponent<CameraComponent>().Primary = camComponent.Primary;
			}

			ImGui::PopItemFlag();

			UI::EndPropertyGrid();
		}, EditorResources::CameraIcon);
	
		DrawComponent<SpriteRendererComponent>("SpriteRenderer", [&](SpriteRendererComponent& spriteComponent, const std::vector<UUID>& entities, const bool isMultiSelect)
		{
			UI::BeginPropertyGrid();

			ImGui::PushItemFlag(ImGuiItemFlags_MixedValue, isMultiSelect && IsInconsistentPrimitive<glm::vec4, SpriteRendererComponent>([](const SpriteRendererComponent& other) { return other.Color; }));

			if (UI::PropertyColor4("Color", spriteComponent.Color))
			{
				for (auto& entityID : entities)
				{
					Entity entity = m_Context->GetEntityWithUUID(entityID);
					entity.GetComponent<SpriteRendererComponent>().Color = spriteComponent.Color;
				}
			}

			ImGui::PopItemFlag();

			{
				UI::PropertyAssetReferenceSettings settings;
				bool textureSet = spriteComponent.Texture != 0;
				if (textureSet)
				{
					settings.AdvanceToNextColumn = false;
					settings.WidthOffset = ImGui::GetStyle().ItemSpacing.x + 28.0f;
				}

				const bool inconsistentTexture = IsInconsistentPrimitive<AssetHandle, SpriteRendererComponent>([](const SpriteRendererComponent& other) { return other.Texture; });
				ImGui::PushItemFlag(ImGuiItemFlags_MixedValue, isMultiSelect && inconsistentTexture);

				if (UI::PropertyAssetReference<Texture2D>("Texture", spriteComponent.Texture, "", settings))
				{
					for (auto& entityID : entities)
					{
						Entity entity = m_Context->GetEntityWithUUID(entityID);
						entity.GetComponent<SpriteRendererComponent>().Texture = spriteComponent.Texture;
					}
				}
				ImGui::PopItemFlag();

				if (textureSet)
				{
					ImGui::SameLine();
					float prevItemHeight = ImGui::GetItemRectSize().y;
					if (ImGui::Button("X", { prevItemHeight, prevItemHeight }))
					{
						for (auto& entityID : entities)
						{
							Entity entity = m_Context->GetEntityWithUUID(entityID);
							entity.GetComponent<SpriteRendererComponent>().Texture = 0;
						}
					}
					ImGui::NextColumn();
				}
			}

			ImGui::PushItemFlag(ImGuiItemFlags_MixedValue, isMultiSelect && IsInconsistentPrimitive<float, SpriteRendererComponent>([](const SpriteRendererComponent& other) { return other.TilingFactor; }));

			if (UI::Property("Tiling Factor", spriteComponent.TilingFactor))
			{
				for (auto& entityID : entities)
				{
					Entity entity = m_Context->GetEntityWithUUID(entityID);
					entity.GetComponent<SpriteRendererComponent>().TilingFactor = spriteComponent.TilingFactor;
				}
			}

			ImGui::PopItemFlag();

			ImGui::PushItemFlag(ImGuiItemFlags_MixedValue, isMultiSelect && IsInconsistentPrimitive<glm::vec2 , SpriteRendererComponent>([](const SpriteRendererComponent& other) { return other.UV0; }));

			if (UI::PropertyDrag("UV Start", spriteComponent.UV0))
			{
				for (auto& entityID : entities)
				{
					Entity entity = m_Context->GetEntityWithUUID(entityID);
					entity.GetComponent<SpriteRendererComponent>().UV0 = spriteComponent.UV0;
				}
			}

			ImGui::PopItemFlag();

			ImGui::PushItemFlag(ImGuiItemFlags_MixedValue, isMultiSelect && IsInconsistentPrimitive<glm::vec2, SpriteRendererComponent>([](const SpriteRendererComponent& other) { return other.UV1; }));

			if (UI::PropertyDrag("UV End", spriteComponent.UV1))
			{
				for (auto& entityID : entities)
				{
					Entity entity = m_Context->GetEntityWithUUID(entityID);
					entity.GetComponent<SpriteRendererComponent>().UV1 = spriteComponent.UV1;
				}
			}

			ImGui::PopItemFlag();

			UI::EndPropertyGrid();
		}, EditorResources::SpriteIcon);
	
		DrawComponent<StaticMeshComponent>("StaticMesh", [&](StaticMeshComponent& meshComp, const std::vector<UUID>& entities, const bool isMultiSelect)
		{
			AssetHandle meshHandle = meshComp.StaticMesh;
			Ref<StaticMesh> mesh = AssetManager::GetAssetAsync<StaticMesh>(meshHandle);
			Ref<MeshSource> meshSource = mesh ? AssetManager::GetAssetAsync<MeshSource>(mesh->GetMeshSource()).Asset : nullptr;

			UI::BeginPropertyGrid();

			{
				const bool inconsistentMesh = IsInconsistentPrimitive<AssetHandle, StaticMeshComponent>([](const StaticMeshComponent& other) { return other.StaticMesh; });
				ImGui::PushItemFlag(ImGuiItemFlags_MixedValue, isMultiSelect && inconsistentMesh);

				if (UI::PropertyAssetReference<StaticMesh>("Static Mesh", meshHandle))
				{
					mesh = AssetManager::GetAssetAsync<StaticMesh>(meshHandle);

					for (auto& entityID : entities)
					{
						Entity entity = m_Context->GetEntityWithUUID(entityID);
						entity.GetComponent<StaticMeshComponent>().StaticMesh = meshHandle;
					}
				}
				ImGui::PopItemFlag();
			}

			// TODO: For now this does nothing however when we have submesh selection it will have an effect
			if (meshSource)
			{
				uint32_t subMeshIndex = meshComp.SubMeshIndex;
				const bool inconsistentSubMeshIndex = IsInconsistentPrimitive<uint32_t, StaticMeshComponent>([](const StaticMeshComponent& other) { return other.SubMeshIndex; });
				ImGui::PushItemFlag(ImGuiItemFlags_MixedValue, isMultiSelect && inconsistentSubMeshIndex);
				if (UI::Property("Submesh Index", subMeshIndex, 1, 0, static_cast<uint32_t>(meshSource->GetSubMeshes().size()) - 1))
				{
					for (auto& entityID : entities)
					{
						Entity entity = m_Context->GetEntityWithUUID(entityID);
						auto& mc = entity.GetComponent<StaticMeshComponent>();
						mc.SubMeshIndex = subMeshIndex;
					}
				}
				ImGui::PopItemFlag();
			}

			ImGui::PushItemFlag(ImGuiItemFlags_MixedValue, isMultiSelect && IsInconsistentPrimitive<bool, StaticMeshComponent>([](const StaticMeshComponent& other) { return other.Visible; }));

			if (UI::Property("Visible", meshComp.Visible))
			{
				for (auto& entityID : entities)
				{
					Entity entity = m_Context->GetEntityWithUUID(entityID);
					auto& mc = entity.GetComponent<StaticMeshComponent>();
					mc.Visible = meshComp.Visible;
				}
			}

			ImGui::PopItemFlag();

			UI::EndPropertyGrid();

			if (mesh)
				Utils::DrawMaterialTable<StaticMeshComponent>(this, entities, mesh->GetMaterials(), meshComp.MaterialTable);
		}, EditorResources::StaticMeshIcon);
	}

	void SceneHierarchyPanel::OnExternalEntityDestroyed(Entity entity)
	{
		if (m_EntityDeletedCallback)
			m_EntityDeletedCallback(entity);
	}

}
