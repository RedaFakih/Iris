#include "IrisPCH.h"
#include "MaterialEditor.h"

#include "ImGui/ImGuiUtils.h"
#include "Renderer/Renderer.h"
#include "Renderer/StorageBufferSet.h"
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
		if (UI::Property("Transparent", transparent, "Select between opaque or transparent object"))
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
		if (UI::Property("Double Sided", doubleSided, "Sets whether to show backsides or cull them"))
		{
			m_MaterialAsset->SetDoubleSided(doubleSided);
			needsSerialize = true;
		}

		UI::PropertyStringReadOnly("Shader", shader->GetName().data());
		UI::PopID();

		UI::EndPropertyGrid();

		auto checkAndSetTexture = [&needsSerialize](Ref<Texture2D>& texture2D) {
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
					m_MaterialAsset->SetAlbedoMap(albedoMap->Handle);

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
					UI::Image(albedoUITexture, { 384.0f, 384.0f });
					ImGui::EndTooltip();
				}
			}

			ImVec2 nextRowCursorPos = ImGui::GetCursorPos();
			ImGui::SameLine();
			ImVec2 properCursorPos = ImGui::GetCursorPos();

			ImGui::SetCursorPos(textureCursorPos);
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, { 0.0f, 0.0f });
			if (hasAlbedoMap && ImGui::Button("X##AlbedoMap", { 18.0f, 18.0f }))
			{
				m_MaterialAsset->ClearAlbedoMap();
				needsSerialize = true;
			}
			ImGui::PopStyleVar();
			ImGui::SetCursorPos(properCursorPos);

			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 0.0f, 0.0f });
			ImVec2 childSize = ImGui::GetContentRegionAvail();
			childSize.y = 77.0f;
			ImGui::BeginChild("##MaterialeditorAlbedoChildID", childSize);
			UI::BeginPropertyGrid(2, 105.0f);

			UI::PropertyColor3("Color", albedoColor);
			if (ImGui::IsItemDeactivated())
				needsSerialize = true;

			float& emission = m_MaterialAsset->GetEmission();
			UI::Property("Emission", emission, 0.1f, 0.0f, 20.0f, "Sets how emissive an object is");
			if (ImGui::IsItemDeactivated())
				needsSerialize = true;

			UI::EndPropertyGrid();
			ImGui::EndChild();
			ImGui::PopStyleVar();

			ImGui::SetCursorPos(nextRowCursorPos);
		}

		if (transparent)
		{
			float& transparency = m_MaterialAsset->GetTransparency();

			UI::BeginPropertyGrid();
			UI::Property("Transparency", transparency, 0.01f, 0.0f, 1.0f, "Set how transparent the object is");
			if (ImGui::IsItemDeactivated())
				needsSerialize = true;
			UI::EndPropertyGrid();
		}
		else
		{
			// Normal map
			if (ImGui::CollapsingHeader("Normal", nullptr, ImGuiTreeNodeFlags_DefaultOpen))
			{
				ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, { 10.0f, 10.0f });

				bool useNormalMap = m_MaterialAsset->IsUsingNormalMap();

				Ref<Texture2D> normalMap = m_MaterialAsset->GetNormalMap();
				bool hasNormalMap = normalMap ? !normalMap.EqualsObject(Renderer::GetWhiteTexture()) && normalMap->Loaded() : false;
				Ref<Texture2D> normalUITexture = hasNormalMap ? normalMap : EditorResources::CheckerboardTexture;

				ImVec2 textureCursorPos = ImGui::GetCursorPos();
				UI::Image(normalUITexture, { 77.0f, 77.0f });
				if (ImGui::BeginDragDropTarget())
				{
					if (checkAndSetTexture(normalMap))
					{
						m_MaterialAsset->SetNormalMap(normalMap->Handle);
						m_MaterialAsset->SetUseNormalMap(true);
					}

					ImGui::EndDragDropTarget();
				}

				ImGui::PopStyleVar();

				if (ImGui::IsItemHovered())
				{
					if (hasNormalMap)
					{
						ImGui::BeginTooltip();
						ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
						std::string filepath = normalMap->GetAssetPath();
						ImGui::TextUnformatted(filepath.c_str());
						ImGui::PopTextWrapPos();
						UI::Image(normalUITexture, { 384.0f, 384.0f });
						ImGui::EndTooltip();
					}
				}

				ImVec2 nextRowCursorPos = ImGui::GetCursorPos();
				ImGui::SameLine();
				ImVec2 properCursorPos = ImGui::GetCursorPos();

				ImGui::SetCursorPos(textureCursorPos);
				ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, { 0.0f, 0.0f });
				if (hasNormalMap && ImGui::Button("X##NormalMap", { 18.0f, 18.0f }))
				{
					m_MaterialAsset->ClearNormalMap();
					m_MaterialAsset->SetUseNormalMap(false);
					needsSerialize = true;
				}
				ImGui::PopStyleVar();
				ImGui::SetCursorPos(properCursorPos);

				ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 0.0f, 0.0f });
				ImVec2 childSize = ImGui::GetContentRegionAvail();
				childSize.y = 77.0f;
				ImGui::BeginChild("##MaterialeditorNormalChildID", childSize);
				UI::BeginPropertyGrid(2, 145.0f);

				if (UI::Property("Use normal map", useNormalMap, "Sets whether you want to use the normal map or not"))
					m_MaterialAsset->SetUseNormalMap(useNormalMap);
				if (ImGui::IsItemDeactivated())
					needsSerialize = true;

				UI::EndPropertyGrid();
				ImGui::EndChild();
				ImGui::PopStyleVar();

				ImGui::SetCursorPos(nextRowCursorPos);
			}

			// Roughness map
			if (ImGui::CollapsingHeader("Roughness", nullptr, ImGuiTreeNodeFlags_DefaultOpen))
			{
				ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, { 10.0f, 10.0f });

				float& rougnessValue = m_MaterialAsset->GetRoughness();

				Ref<Texture2D> roughnessMap = m_MaterialAsset->GetRoughnessMap();
				bool hasRoughnessMap = roughnessMap ? !roughnessMap.EqualsObject(Renderer::GetWhiteTexture()) && roughnessMap->Loaded() : false;
				Ref<Texture2D> roughnessUITexture = hasRoughnessMap ? roughnessMap : EditorResources::CheckerboardTexture;

				ImVec2 textureCursorPos = ImGui::GetCursorPos();
				UI::Image(roughnessUITexture, { 77.0f, 77.0f });
				if (ImGui::BeginDragDropTarget())
				{
					if (checkAndSetTexture(roughnessMap))
						m_MaterialAsset->SetRoughnessMap(roughnessMap->Handle);

					ImGui::EndDragDropTarget();
				}

				ImGui::PopStyleVar();

				if (ImGui::IsItemHovered())
				{
					if (hasRoughnessMap)
					{
						ImGui::BeginTooltip();
						ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
						std::string filepath = roughnessMap->GetAssetPath();
						ImGui::TextUnformatted(filepath.c_str());
						ImGui::PopTextWrapPos();
						UI::Image(roughnessUITexture, { 384.0f, 384.0f });
						ImGui::EndTooltip();
					}
				}

				ImVec2 nextRowCursorPos = ImGui::GetCursorPos();
				ImGui::SameLine();
				ImVec2 properCursorPos = ImGui::GetCursorPos();

				ImGui::SetCursorPos(textureCursorPos);
				ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, { 0.0f, 0.0f });
				if (hasRoughnessMap && ImGui::Button("X##RoughnessMap", { 18.0f, 18.0f }))
				{
					m_MaterialAsset->ClearRoughnessMap();
					needsSerialize = true;
				}
				ImGui::PopStyleVar();
				ImGui::SetCursorPos(properCursorPos);

				ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 0.0f, 0.0f });
				ImVec2 childSize = ImGui::GetContentRegionAvail();
				childSize.y = 77.0f;
				ImGui::BeginChild("##MaterialeditorRoughnessChildID", childSize);
				UI::BeginPropertyGrid(2, 130.0f);

				UI::Property("Roughness", rougnessValue, 0.01f, 0.0f, 1.0f, "Adjusts the roughness value");

				if (ImGui::IsItemDeactivated())
					needsSerialize = true;

				UI::EndPropertyGrid();
				ImGui::EndChild();
				ImGui::PopStyleVar();

				ImGui::SetCursorPos(nextRowCursorPos);
			}

			// Metalness map
			if (ImGui::CollapsingHeader("Metalness", nullptr, ImGuiTreeNodeFlags_DefaultOpen))
			{
				ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, { 10.0f, 10.0f });

				float& metalnessValue = m_MaterialAsset->GetMetalness();

				Ref<Texture2D> metalnessMap = m_MaterialAsset->GetMetalnessMap();
				bool hasMetalnessMap = metalnessMap ? !metalnessMap.EqualsObject(Renderer::GetWhiteTexture()) && metalnessMap->Loaded() : false;
				Ref<Texture2D> metalnessUITexture = hasMetalnessMap ? metalnessMap : EditorResources::CheckerboardTexture;

				ImVec2 textureCursorPos = ImGui::GetCursorPos();
				UI::Image(metalnessUITexture, { 77.0f, 77.0f });
				if (ImGui::BeginDragDropTarget())
				{
					if (checkAndSetTexture(metalnessMap))
						m_MaterialAsset->SetMetalnessMap(metalnessMap->Handle);

					ImGui::EndDragDropTarget();
				}

				ImGui::PopStyleVar();

				if (ImGui::IsItemHovered())
				{
					if (hasMetalnessMap)
					{
						ImGui::BeginTooltip();
						ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
						std::string filepath = metalnessMap->GetAssetPath();
						ImGui::TextUnformatted(filepath.c_str());
						ImGui::PopTextWrapPos();
						UI::Image(metalnessUITexture, { 384.0f, 384.0f });
						ImGui::EndTooltip();
					}
				}

				ImVec2 nextRowCursorPos = ImGui::GetCursorPos();
				ImGui::SameLine();
				ImVec2 properCursorPos = ImGui::GetCursorPos();

				ImGui::SetCursorPos(textureCursorPos);
				ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, { 0.0f, 0.0f });
				if (hasMetalnessMap && ImGui::Button("X##MetalnessMap", { 18.0f, 18.0f }))
				{
					m_MaterialAsset->ClearMetalnessMap();
					needsSerialize = true;
				}
				ImGui::PopStyleVar();
				ImGui::SetCursorPos(properCursorPos);

				ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 0.0f, 0.0f });
				ImVec2 childSize = ImGui::GetContentRegionAvail();
				childSize.y = 77.0f;
				ImGui::BeginChild("##MaterialeditorMetalnessChildID", childSize);
				UI::BeginPropertyGrid(2, 130.0f);

				UI::Property("Metalness", metalnessValue, 0.01f, 0.0f, 1.0f, "Adjusts the metalness value");

				if (ImGui::IsItemDeactivated())
					needsSerialize = true;

				UI::EndPropertyGrid();
				ImGui::EndChild();
				ImGui::PopStyleVar();

				ImGui::SetCursorPos(nextRowCursorPos);
			}
		}

		if (needsSerialize)
			AssetImporter::Serialize(m_MaterialAsset);
	}

}