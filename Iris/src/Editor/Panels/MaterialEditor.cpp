#include "IrisPCH.h"
#include "MaterialEditor.h"

#include "ImGui/ImGuiUtils.h"
#include "Renderer/Renderer.h"
#include "Renderer/Texture.h"
#include "Renderer/UniformBufferSet.h"

#include <imgui/imgui.h>

namespace Iris {

	MaterialEditor::MaterialEditor()
		: AssetEditor("Material Editor")
	{
	}

	void MaterialEditor::SetAsset(const Ref<Asset>& asset)
	{
		m_MaterialAsset = asset.As<MaterialAsset>();
		std::string title = m_MaterialAsset->GetMaterial()->GetName().size() ? m_MaterialAsset->GetMaterial()->GetName() : Project::GetEditorAssetManager()->GetMetaData(asset->Handle).FilePath.stem().string();
		SetTitle(title);
	}

	void MaterialEditor::OnOpen()
	{
		if (!m_MaterialAsset)
			SetOpen(false);
	}

	void MaterialEditor::OnClose()
	{
		m_MaterialAsset = nullptr;
	}

	void MaterialEditor::Render()
	{
		bool needsSerialize = false;

		Ref<Material> material = m_MaterialAsset->GetMaterial();
		Ref<Shader> shader = material->GetShader();

		// TODO: Get transparent shader and compare with shader to know if transparent or not... but for now:
		bool transparent = m_MaterialAsset->IsTransparent();
		
		UI::BeginPropertyGrid();

		UI::PushID();
		if (UI::Property("Transparent", transparent))
		{
			// TODO: Once we have a tranparent shader
			//if (transparent)
			//	m_MaterialAsset->SetMaterial(Material::Create(transparentShader));
			//else
				m_MaterialAsset->SetMaterial(Material::Create(Renderer::GetShadersLibrary()->Get("IrisPBRStatic")));

			m_MaterialAsset->m_Transparent = transparent;
			m_MaterialAsset->SetDefaults();

			material = m_MaterialAsset->GetMaterial();
			shader = material->GetShader();
		}

		bool doubleSided = m_MaterialAsset->IsDoubleSided();
		if (UI::Property("Double Sided", doubleSided))
		{
			m_MaterialAsset->SetDoubleSided(doubleSided);
			needsSerialize = true;
		}

		UI::PropertyStringReadOnly("Shader", material->GetShader()->GetName().data());
		UI::PopID();

		UI::EndPropertyGrid();

		auto checkAndSetTexture = [&material, &needsSerialize](Ref<Texture2D>& texture2D) {
			auto data = ImGui::AcceptDragDropPayload("asset_payload");
			if (data && data->DataSize / sizeof(AssetHandle) == 1)
			{
				AssetHandle assetHandle = *(((AssetHandle*)data->Data));
				Ref<Asset> asset = AssetManager::GetAsset<Asset>(assetHandle);
				if (asset && asset->GetAssetType() == AssetType::Texture)
				{
					texture2D = asset.As<Texture2D>();
					needsSerialize = true;
				}
				return true;
			}
			return false;
		};

		// Albedo
		if (ImGui::CollapsingHeader("Albedo", nullptr, ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, { 10.0f, 10.0f });

			glm::vec3& albedoColor = m_MaterialAsset->GetAlbedoColor();
			Ref<Texture2D> albedoMap = m_MaterialAsset->GetAlbedoMap();
			bool hasAlbedoMap = albedoMap ? !albedoMap.EqualsObject(Renderer::GetWhiteTexture()) && albedoMap->Loaded() : false;
			Ref<Texture2D> albedoUITexture = hasAlbedoMap ? albedoMap : EditorResources::CheckerboardTexture;

			ImVec2 textureCursorPos = ImGui::GetCursorPos();
			UI::Image(albedoUITexture, { 77.0f, 77.0f });
			if (ImGui::BeginDragDropTarget())
			{
				if (checkAndSetTexture(albedoMap))
					material->Set("u_AlbedoTexture", albedoMap);

				ImGui::EndDragDropTarget();
			}

			ImGui::PopStyleVar();

			if (ImGui::IsItemHovered())
			{
				if (hasAlbedoMap)
				{
					ImGui::BeginTooltip();
					ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
					std::string filepath = albedoMap->GetAssetPath();
					ImGui::TextUnformatted(filepath.c_str());
					ImGui::PopTextWrapPos();
					UI::Image(albedoUITexture, ImVec2(384, 384));
					ImGui::EndTooltip();
				}
			}

			ImVec2 nextRowCursorPos = ImGui::GetCursorPos();
			ImGui::SameLine();
			ImVec2 properCursorPos = ImGui::GetCursorPos();

			ImGui::SetCursorPos(textureCursorPos);
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
			if (hasAlbedoMap && ImGui::Button("X##AlbedoMap", ImVec2(18, 18)))
			{
				m_MaterialAsset->ClearAlbedoMap();
				needsSerialize = true;
			}
			ImGui::PopStyleVar();
			ImGui::SetCursorPos(properCursorPos);

			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 0.0f, 0.0f });
			ImVec2 childSize = ImGui::GetContentRegionAvail();
			childSize.y = 77.0f;
			ImGui::BeginChild("##MaterialeditorChildID", childSize);
			UI::BeginPropertyGrid(2, 80.0f);

			UI::PropertyColor3("Color", albedoColor);
			if (ImGui::IsItemDeactivated())
				needsSerialize = true;

			float& emission = m_MaterialAsset->GetEmission();
			UI::Property("Emission", emission, 0.1f, 0.0f, 20.0f);
			if (ImGui::IsItemDeactivated())
				needsSerialize = true;

			UI::EndPropertyGrid();
			ImGui::EndChild();
			ImGui::PopStyleVar();

			ImGui::SetCursorPos(nextRowCursorPos);

			if (transparent)
			{
				float& transparency = m_MaterialAsset->GetTransparency();

				UI::BeginPropertyGrid();
				UI::Property("Transparency", transparency, 0.01f, 0.0f, 1.0f);
				if (ImGui::IsItemDeactivated())
					needsSerialize = true;
				UI::EndPropertyGrid();
			}
		}

		if (needsSerialize)
			AssetImporter::Serialize(m_MaterialAsset);
	}

}