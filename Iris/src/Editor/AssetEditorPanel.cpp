#include "IrisPCH.h"
#include "AssetEditorPanel.h"

#include "Panels/MaterialEditor.h"
#include "Project/Project.h"
#include "Renderer/StorageBufferSet.h"
#include "Renderer/Texture.h"
#include "Renderer/UniformBufferSet.h"

#include <imgui/imgui.h>

namespace Iris {

	AssetEditor::AssetEditor(const char* id)
		: m_ID(id), m_MinSize(200, 400), m_MaxSize(2000, 2000)
	{
		SetTitle(id);
	}

	void AssetEditor::OnImGuiRender()
	{
		if (!m_IsOpen)
			return;

		bool wasOpen = m_IsOpen;

		OnWindowStylePush();

		ImGui::SetNextWindowSizeConstraints({ m_MinSize.x, m_MinSize.y }, { m_MaxSize.x, m_MaxSize.y });
		ImGui::Begin(m_TitleAndID.c_str(), &m_IsOpen, GetWindowFlags());
		OnWindowStylePop();
		if (m_IsOpen)
			Render();
		ImGui::End();

		if (wasOpen && !m_IsOpen)
			OnClose();
	}

	void AssetEditor::SetOpen(bool isOpen)
	{
		m_IsOpen = isOpen;
		if (!m_IsOpen)
			OnClose();
		else
			OnOpen();
	}

	void AssetEditor::SetMinSize(uint32_t width, uint32_t height)
	{
		if (width <= 0) 
			width = 200;
		if (height <= 0) 
			height = 400;

		m_MinSize = glm::vec2(static_cast<float>(width), static_cast<float>(height));
	}

	void AssetEditor::SetMaxSize(uint32_t width, uint32_t height)
	{
		if (width <= 0) 
			width = 2000;
		if (height <= 0)
			height = 2000;
		if (static_cast<float>(width) <= m_MinSize.x) 
			width = static_cast<uint32_t>(m_MinSize.x * 2.0f);
		if (static_cast<float>(height) <= m_MinSize.y) 
			height = static_cast<uint32_t>(m_MinSize.y * 2.0f);

		m_MaxSize = glm::vec2(static_cast<float>(width), static_cast<float>(height));
	}

	ImGuiWindowFlags AssetEditor::GetWindowFlags()
	{
		// Default window flag to use with all asset editors
		return ImGuiWindowFlags_NoCollapse;
	}

	void AssetEditorPanel::RegisterDefaultEditors()
	{
		RegisterEditor<MaterialEditor>(AssetType::Material);
	}

	void AssetEditorPanel::UnregisterAllEditors()
	{
		s_Editors.clear();
	}

	void AssetEditorPanel::OnUpdate(TimeStep ts)
	{
		for (auto& kv : s_Editors)
			kv.second->OnUpdate(ts);
	}

	void AssetEditorPanel::OnEvent(Events::Event& e)
	{
		for (auto& kv : s_Editors)
			kv.second->OnEvent(e);
	}

	void AssetEditorPanel::SetSceneContext(const Ref<Scene>& context)
	{
		s_SceneContext = context;

		for (auto& kv : s_Editors)
			kv.second->SetSceneContext(context);
	}

	void AssetEditorPanel::OnImGuiRender()
	{
		for (auto& kv : s_Editors)
			kv.second->OnImGuiRender();
	}

	void AssetEditorPanel::OpenEditor(const Ref<Asset>& asset)
	{
		if (asset == nullptr)
			return;

		if (!s_Editors.contains(asset->GetAssetType()))
			return;

		auto& editor = s_Editors[asset->GetAssetType()];
		if (editor->IsOpen())
			editor->SetOpen(false);

		const auto& metadata = Project::GetEditorAssetManager()->GetMetaData(asset->Handle);
		editor->SetTitle(metadata.FilePath.filename().string());
		editor->SetAsset(asset);
		editor->SetSceneContext(s_SceneContext);
		editor->SetOpen(true);
	}

}