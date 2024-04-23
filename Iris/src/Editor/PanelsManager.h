#pragma once

#include "Core/Hash.h"
#include "EditorPanel.h"
#include "Project/Project.h"

namespace Iris {

	struct PanelSpecification
	{
		const char* ID = "";
		const char* Name = "";
		Ref<EditorPanel> Panel = nullptr;
		bool IsOpen = false;
	};

	enum class PanelCategory
	{
		View = 0,
		Edit = 1,
		_SIZE
	};

	class PanelsManager
	{
	public:
		PanelsManager() = default;
		~PanelsManager();

		[[nodiscard]] static Scope<PanelsManager> Create();

		void OnImGuiRender();
		void OnEvent(Events::Event& e);
		void SetSceneContext(const Ref<Scene> scene);
		void OnProjectChanged(Ref<Project> project);

		void Serialize();
		void Deserialize();

		PanelSpecification* GetPanelSpec(uint32_t id);
		const PanelSpecification* GetPanelSpec(uint32_t id) const;

		template<typename TPanel>
		Ref<TPanel> AddPanel(PanelCategory category, const PanelSpecification& spec)
		{
			static_assert(std::is_base_of<EditorPanel, TPanel>::value, "PanalsManager::AddPanel requires that TPanel inherits from EditorPanel!");

			std::unordered_map<uint32_t, PanelSpecification>& panelMap = m_Panels[static_cast<std::size_t>(category)];

			uint32_t panelIDHash = Hash::GenerateFNVHash(spec.ID);
			if (panelMap.contains(panelIDHash))
			{
				IR_CORE_WARN_TAG("PanelsManager", "Trying to add a panel that has already been added! (ID: {})", spec.ID);
				return nullptr;
			}

			panelMap[panelIDHash] = spec;
			return spec.Panel;
		}

		template<typename TPanel, typename... Args>
		Ref<TPanel> AddPanel(PanelCategory category, const char* strID, bool isOpenByDefault, Args&&... args)
		{
			return AddPanel<TPanel>(category, PanelSpecification{
				.ID = strID,
				.Name = strID,
				.Panel = TPanel::Create(std::forward<Args>(args)...),
				.IsOpen = isOpenByDefault
			});
		}

		template<typename TPanel, typename... Args>
		Ref<TPanel> AddPanel(PanelCategory category, const char* strID, const char* displayName, bool isOpenByDefault, Args&&... args)
		{
			return AddPanel<TPanel>(category, PanelSpecification{
				.ID = strID,
				.Name = displayName,
				.Panel = TPanel::Create(std::forward<Args>(args)...),
				.IsOpen = isOpenByDefault
			});
		}

		void RemovePanel(const char* strID);

		template<typename TPanel>
		Ref<TPanel> GetPanel(const char* strID)
		{
			static_assert(std::is_base_of<EditorPanel, TPanel>::value, "PanalsManager::AddPanel requires that TPanel inherits from EditorPanel!");

			uint32_t panelID = Hash::GenerateFNVHash(strID);

			for (auto& panelMap : m_Panels)
			{
				if (!panelMap.contains(panelID))
					continue;

				return panelMap.at(panelID).Panel;
			}

			IR_CORE_ERROR_TAG("PanelsManager", "Could not find panel with ID: {}", strID);
			return nullptr;
		}

		std::unordered_map<uint32_t, PanelSpecification>& GetPanels(PanelCategory category) { return m_Panels[static_cast<std::size_t>(category)]; }
		const std::unordered_map<uint32_t, PanelSpecification>& GetPanels(PanelCategory category) const { return m_Panels[static_cast<std::size_t>(category)]; }

	private:
		std::array<std::unordered_map<uint32_t, PanelSpecification>, static_cast<std::size_t>(PanelCategory::_SIZE)> m_Panels;

	};

}