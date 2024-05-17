#pragma once

#include "Editor/EditorPanel.h"
#include "Editor/SelectionManager.h"
#include "ImGui/CustomTreeNode.h"
#include "ImGui/ImGuiUtils.h"
#include "Renderer/Texture.h"
#include "Renderer/UniformBufferSet.h"
#include "Scene/Scene.h"

#include <magic_enum.hpp>
using namespace magic_enum::bitwise_operators;

namespace Iris {

	#define IR_COLOR32(R,G,B,A)    (((ImU32)(A)<<24) | ((ImU32)(B)<<16) | ((ImU32)(G)<<8) | ((ImU32)(R)<<0))

	class SceneHierarchyPanel : public EditorPanel
	{
	public:
		SceneHierarchyPanel() = default;
		SceneHierarchyPanel(const Ref<Scene> scene, SelectionContext selectionContext, bool isWindow = true);
		
		[[nodiscard]] static Ref<SceneHierarchyPanel> Create();
		[[nodiscard]] static Ref<SceneHierarchyPanel> Create(const Ref<Scene> scene, SelectionContext selectionContext, bool isWindow = true);

		virtual void SetSceneContext(const Ref<Scene>& scene) override;
		virtual void OnImGuiRender(bool& isOpen) override;
		virtual void OnEvent(Events::Event& e) override;

		void SetEntityDeletedCallback(const std::function<void(Entity)>& callback) { m_EntityDeletedCallback = callback; }
		void AddEntityContextMenuPlugin(const std::function<void(Entity)>& plugin) { m_EntityContextMenuPlugins.push_back(plugin); }

		Ref<Scene> GetSceneContext() const { return m_Context; }

		static SelectionContext GetActiveSelectionContext() { return s_ActiveSelectionContext; }

	public:
		template<typename TVectorType, typename TComponent, typename GetOtherFunc>
		UI::VectorAxis GetInconsistentVectorAxis(GetOtherFunc func)
		{
			UI::VectorAxis axes = UI::VectorAxis::None;

			const auto& entities = SelectionManager::GetSelections(m_SelectionContext);

			if (entities.size() < 2)
				return axes;

			Entity firstEntity = m_Context->GetEntityWithUUID(entities[0]);
			const TVectorType& first = func(firstEntity.GetComponent<TComponent>());

			for (size_t i = 1; i < entities.size(); i++)
			{
				Entity entity = m_Context->GetEntityWithUUID(entities[i]);
				const TVectorType& otherVector = func(entity.GetComponent<TComponent>());

				if (glm::epsilonNotEqual(otherVector.x, first.x, FLT_EPSILON))
					axes |= UI::VectorAxis::X;

				if (glm::epsilonNotEqual(otherVector.y, first.y, FLT_EPSILON))
					axes |= UI::VectorAxis::Y;

				if constexpr (std::is_same_v<TVectorType, glm::vec3> || std::is_same_v<TVectorType, glm::vec4>)
				{
					if (glm::epsilonNotEqual(otherVector.z, first.z, FLT_EPSILON))
						axes |= UI::VectorAxis::Z;
				}

				if constexpr (std::is_same_v<TVectorType, glm::vec4>)
				{
					if (glm::epsilonNotEqual(otherVector.w, first.w, FLT_EPSILON))
						axes |= UI::VectorAxis::W;
				}
			}

			return axes;
		}

		template<typename TVectorType>
		UI::VectorAxis GetInconsistentVectorAxis(const TVectorType& first, const TVectorType& other)
		{
			UI::VectorAxis axes = UI::VectorAxis::None;

			if (glm::epsilonNotEqual(other.x, first.x, FLT_EPSILON))
				axes |= UI::VectorAxis::X;

			if (glm::epsilonNotEqual(other.y, first.y, FLT_EPSILON))
				axes |= UI::VectorAxis::Y;

			if constexpr (std::is_same_v<TVectorType, glm::vec3> || std::is_same_v<TVectorType, glm::vec4>)
			{
				if (glm::epsilonNotEqual(other.z, first.z, FLT_EPSILON))
					axes |= UI::VectorAxis::Z;
			}

			if constexpr (std::is_same_v<TVectorType, glm::vec4>)
			{
				if (glm::epsilonNotEqual(other.w, first.w, FLT_EPSILON))
					axes |= UI::VectorAxis::W;
			}

			return axes;
		}

		template<typename TPrimitive, typename TComponent, typename GetOtherFunc>
		bool IsInconsistentPrimitive(GetOtherFunc func)
		{
			const auto& entities = SelectionManager::GetSelections(m_SelectionContext);

			if (entities.size() < 2)
				return false;

			Entity firstEntity = m_Context->GetEntityWithUUID(entities[0]);
			const TPrimitive& first = func(firstEntity.GetComponent<TComponent>());

			for (size_t i = 1; i < entities.size(); i++)
			{
				Entity entity = m_Context->GetEntityWithUUID(entities[i]);

				if (!entity.HasComponent<TComponent>())
					continue;

				const auto& otherValue = func(entity.GetComponent<TComponent>());
				if (otherValue != first)
					return true;
			}

			return false;
		}

		template<typename TComponent, typename GetOtherFunc>
		bool IsInconsistentString(GetOtherFunc func)
		{
			return IsInconsistentPrimitive<std::string, TComponent, GetOtherFunc>(func);
		}

		template<typename TComponent, typename UIFunction>
		void DrawComponent(const std::string& name, UIFunction function, Ref<Texture2D> icon = nullptr)
		{
			bool shouldDraw = true;

			auto& entities = SelectionManager::GetSelections(s_ActiveSelectionContext);
			for (const auto& entityID : entities)
			{
				Entity entity = m_Context->GetEntityWithUUID(entityID);
				if (!entity.HasComponent<TComponent>())
				{
					shouldDraw = false;
					break;
				}
			}

			if (!shouldDraw || entities.size() == 0)
				return;

			if (icon == nullptr)
				icon = EditorResources::AssetIcon;

			//	This fixes an issue where the first "+" button would display the "Remove" buttons for ALL components on an Entity.
			//	This is due to ImGui::TreeNodeEx only pushing the id for it's children if it's actually open
			ImGui::PushID((void*)typeid(TComponent).hash_code());
			ImVec2 contentRegionAvailable = ImGui::GetContentRegionAvail();

			bool open = UI::TreeNodeWithIcon(name, icon, { 20.0f, 20.0f });
			bool right_clicked = ImGui::IsItemClicked(ImGuiMouseButton_Right);
			float lineHeight = ImGui::GetItemRectMax().y - ImGui::GetItemRectMin().y;
			float itemPadding = 4.0f;

			bool resetValues = false;
			bool removeComponent = false;

			ImGui::SameLine(contentRegionAvailable.x - lineHeight - 5.0f);
			UI::ShiftCursorY(lineHeight / 4.0f);
			if (ImGui::InvisibleButton("##options", ImVec2{ lineHeight, lineHeight }) || right_clicked)
			{
				ImGui::OpenPopup("ComponentSettings");
			}
			UI::DrawButtonImage(EditorResources::GearIcon, IR_COLOR32(160, 160, 160, 200),
				IR_COLOR32(160, 160, 160, 255),
				IR_COLOR32(160, 160, 160, 150),
				UI::RectExpanded(UI::GetItemRect(), 0.0f, 0.0f));

			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 6.0f, 6.0f });
			if (UI::BeginPopup("ComponentSettings"))
			{
				UI::ShiftCursorX(itemPadding);

				Entity entity = m_Context->GetEntityWithUUID(entities[0]);
				auto& component = entity.GetComponent<TComponent>();

				if (ImGui::MenuItem("Copy"))
					Scene::CopyComponentFromScene<TComponent>(m_ComponentCopyEntity, m_ComponentCopyScene, entity, m_Context);

				UI::ShiftCursorX(itemPadding);
				if (ImGui::MenuItem("Paste"))
				{
					for (auto entityID : SelectionManager::GetSelections(s_ActiveSelectionContext))
					{
						Entity selectedEntity = m_Context->GetEntityWithUUID(entityID);
						Scene::CopyComponentFromScene<TComponent>(selectedEntity, m_Context, m_ComponentCopyEntity, m_ComponentCopyScene);
					}
				}

				UI::ShiftCursorX(itemPadding);
				if (ImGui::MenuItem("Reset"))
					resetValues = true;

				UI::ShiftCursorX(itemPadding);
				if constexpr (!std::is_same<TComponent, TransformComponent>::value)
				{
					if (ImGui::MenuItem("Remove component"))
						removeComponent = true;
				}

				UI::EndPopup();
			}
			ImGui::PopStyleVar();

			if (open)
			{
				Entity entity = m_Context->GetEntityWithUUID(entities[0]);
				TComponent& firstComponent = entity.GetComponent<TComponent>();
				const bool isMultiEdit = entities.size() > 1;
				function(firstComponent, entities, isMultiEdit);
				ImGui::TreePop();
			}

			if (removeComponent)
			{
				for (auto& entityID : entities)
				{
					Entity entity = m_Context->GetEntityWithUUID(entityID);
					if (entity.HasComponent<TComponent>())
						entity.RemoveComponent<TComponent>();
				}
			}

			if (resetValues)
			{
				for (auto& entityID : entities)
				{
					Entity entity = m_Context->GetEntityWithUUID(entityID);
					if (entity.HasComponent<TComponent>())
					{
						entity.RemoveComponent<TComponent>();
						entity.AddComponent<TComponent>();
					}
				}
			}

			if (!open)
				UI::ShiftCursorY(-(ImGui::GetStyle().ItemSpacing.y + 1.0f));

			ImGui::PopID();
		}

	private:
		bool TagSearchRecursive(Entity entity, std::string_view searchFilter, uint32_t maxSearchDepth, uint32_t currentDepth = 1);

		void DrawEntityCreateMenu(Entity parent = {});
		void DrawEntityNode(Entity entity, const std::string& searchFilter = "");
		void DrawComponents(const std::vector<UUID>& entities);

		void OnExternalEntityDestroyed(Entity entity);

	private:
		Ref<Scene> m_Context;
		bool m_IsWindow;
		bool m_IsWindowFocused = false;
		bool m_IsWindowHovered = false;
		bool m_IsHierarchyOrPropertiesFocused = false;
		bool m_ShiftSelectionRunning = false;
		bool m_OpenEntityCreateMenuPopup = false;
		bool m_CurrentlyRenderingOnlyViewport = false;

		ImRect m_WindowBounds = {};

		std::function<void(Entity)> m_EntityDeletedCallback;
		std::vector<std::function<void(Entity)>> m_EntityContextMenuPlugins;

		std::vector<UUID> m_EntityDeletionQueue;

		SelectionContext m_SelectionContext;

		int32_t m_FirstSelectedRow = -1;
		int32_t m_LastSelectedRow = -1;

		Ref<Scene> m_ComponentCopyScene;
		Entity m_ComponentCopyEntity;

		static SelectionContext s_ActiveSelectionContext;

	};

}