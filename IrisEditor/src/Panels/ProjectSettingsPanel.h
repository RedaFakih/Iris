#pragma once

#include "AssetManager/Asset/Asset.h"
#include "Editor/EditorPanel.h"

namespace Iris {

	class Project;

	class ProjectSettingsPanel : public EditorPanel
	{
	public:
		ProjectSettingsPanel() = default;
		~ProjectSettingsPanel() = default;

		[[nodiscard]] static Ref<ProjectSettingsPanel> Create()
		{
			return CreateRef<ProjectSettingsPanel>();
		}

		virtual void OnImGuiRender(bool& isOpen) override;
		virtual void OnProjectChanged(const Ref<Project>& project) override;

	private:
		Ref<Project> m_Context = nullptr;
		AssetHandle m_StartScene;

	};

}