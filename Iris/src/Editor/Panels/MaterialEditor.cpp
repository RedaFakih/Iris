#include "IrisPCH.h"
#include "MaterialEditor.h"

#include "Core/Input/Input.h"
#include "ImGui/ImGuiUtils.h"
#include "Renderer/Mesh/MeshFactory.h"
#include "Renderer/Renderer.h"

namespace Iris {

	/*
	 * When we set Roughness and Metalness MAPS the Roughness and Metalness values will be locked to 1.0f since they are multiplied with the sampled value from the textures
	 * This means that any modifications to the Roughness/Metalness values should be done in the authored texture
	 */

	MaterialEditor::MaterialEditor()
		: AssetEditor("Material Editor")
	{
		m_SceneRenderer = SceneRendererLite::Create(m_Scene, { .ViewportWidth = 512, .ViewportHeight = 512 });
	}

	void MaterialEditor::OnUpdate(TimeStep ts)
	{
		if (m_Scene)
		{
			m_Camera.SetActive(m_ViewportMouseOver || m_AllowViewportCameraEvents);
			m_Camera.OnUpdate(ts);
			m_Scene->OnUpdateEditor(ts);
			m_Scene->OnRenderEditor(m_SceneRenderer, m_Camera);
		}

		bool leftAltWithEitherLeftOrMiddleButtonOrJustRight = (Input::IsKeyDown(KeyCode::LeftAlt) && (Input::IsMouseButtonDown(MouseButton::Left) || (Input::IsMouseButtonDown(MouseButton::Middle)))) || Input::IsMouseButtonDown(MouseButton::Right);
		bool notStartCameraViewportAndViewportHoveredFocused = !m_StartedCameraClickInViewport && m_ViewportFocused && m_ViewportMouseOver;
		if (leftAltWithEitherLeftOrMiddleButtonOrJustRight && notStartCameraViewportAndViewportHoveredFocused)
			m_StartedCameraClickInViewport = true;

		bool NotRightAndNOTLeftAltANDLeftOrMiddle = !Input::IsMouseButtonDown(MouseButton::Right) && !(Input::IsKeyDown(KeyCode::LeftAlt) && (Input::IsMouseButtonDown(MouseButton::Left) || Input::IsMouseButtonDown(MouseButton::Middle)));
		if (NotRightAndNOTLeftAltANDLeftOrMiddle)
			m_StartedCameraClickInViewport = false;
	}

	void MaterialEditor::OnEvent(Events::Event& e)
	{
		if (!IsOpen())
		{
			return;
		}

		if (m_AllowViewportCameraEvents)
		{
			m_Camera.OnEvent(e);
		}

		Events::EventDispatcher dispatcher(e);

		if (m_ViewportMouseOver)
		{
			dispatcher.Dispatch<Events::KeyPressedEvent>([this](Events::KeyPressedEvent& event) { return OnKeyPressedEvent(event); });
		}
	}

	void MaterialEditor::SetAsset(const Ref<Asset>& asset)
	{
		m_MaterialAsset = asset.As<MaterialAsset>();

		if (!AssetManager::IsAssetHandleValid(m_DisplayMeshHandle))
		{
			// TODO: Handle this better also scale the sphere maybe?
			m_DisplayMeshHandle = 18398963716275400970;
		}

		// create the scene
		m_Scene = Scene::Create("MaterialAssetEditor", true);
		// TODO: Manage the SkyLight better we cant just have a UUID like that
		m_Scene->CreateEntity("SkyLight").AddComponent<SkyLightComponent>(AssetHandle{ 9111623399177370701 }, 0.8f, 2.0f, false);
		m_Scene->CreateEntity("Sphere").AddComponent<StaticMeshComponent>().StaticMesh = m_DisplayMeshHandle;
		AssetManager::GetAsset<StaticMesh>(m_DisplayMeshHandle)->GetMaterials()->SetMaterial(0, m_MaterialAsset->Handle);

		m_SceneRenderer->SetScene(m_Scene);

		std::string title = m_MaterialAsset->GetMaterial()->GetName().size() ? m_MaterialAsset->GetMaterial()->GetName() : Project::GetEditorAssetManager()->GetMetaData(asset->Handle).FilePath.stem().string();
		SetTitle(fmt::format("Material Editor: {}", title));
	}

	void MaterialEditor::OnOpen()
	{
		if (!m_MaterialAsset)
			SetOpen(false);
	}

	void MaterialEditor::OnClose()
	{
		m_MaterialAsset = nullptr;
		m_Scene = nullptr;
	}

	void MaterialEditor::Render()
	{
		ImGui::DockSpace(ImGui::GetID("MyDockspace"));

		UI::ImGuiScopedStyle stylePadding(ImGuiStyleVar_WindowPadding, { 0.0f, 0.0f });

		m_WindowClass.ClassId = ImGui::GetID(m_ID.c_str());
		m_WindowClass.DockingAllowUnclassed = false;

		DrawDisplayImagePane();
		DrawMaterialSettingsPane();
	}

	ImGuiWindowFlags MaterialEditor::GetWindowFlags()
	{
		return ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar;
	}

	void MaterialEditor::OnWindowStylePush()
	{
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 0.0f, 0.0f });
	}

	void MaterialEditor::OnWindowStylePop()
	{
		ImGui::PopStyleVar();
	}

	void MaterialEditor::DrawDisplayImagePane()
	{
		ImGui::SetNextWindowClass(&m_WindowClass);
		if (ImGui::Begin("Viewport##materialAsset", nullptr,
			ImGuiWindowFlags_NoTitleBar |
			ImGuiWindowFlags_NoScrollbar |
			ImGuiWindowFlags_NoCollapse |
			(Input::IsKeyDown(KeyCode::LeftAlt) ? ImGuiWindowFlags_NoMove : 0)))
		{
			m_ViewportMouseOver = ImGui::IsWindowHovered();
			m_ViewportFocused = ImGui::IsWindowFocused();

			ImRect viewportRect = UI::GetWindowRect();

			ImVec2 viewportSize = ImGui::GetContentRegionAvail();

			if ((viewportSize.x > 0.0f) && (viewportSize.y > 0.0f))
			{
				m_SceneRenderer->SetViewportSize(static_cast<uint32_t>(viewportSize.x), static_cast<uint32_t>(viewportSize.y));
				m_Scene->SetViewportSize(static_cast<uint32_t>(viewportSize.x), static_cast<uint32_t>(viewportSize.y));
				m_Camera.SetViewportSize(static_cast<uint32_t>(viewportSize.x), static_cast<uint32_t>(viewportSize.y));

				Ref<Texture2D> finalImage = m_SceneRenderer->GetFinalPassImage();
				UI::Image(finalImage ? finalImage : EditorResources::CheckerboardTexture, viewportSize, { 0, 1 }, { 1, 0 });

				m_AllowViewportCameraEvents = (ImGui::IsMouseHoveringRect(viewportRect.Min, viewportRect.Max) && m_ViewportFocused) || m_StartedCameraClickInViewport;
			}
		}

		ImGui::End();
	}

	void MaterialEditor::DrawMaterialSettingsPane()
	{
		bool needsSerialize = false;

		Ref<Material> material = m_MaterialAsset->GetMaterial();
		Ref<Shader> shader = material->GetShader();

		// TODO: Get transparent shader and compare with shader to know if transparent or not... but for now:
		bool transparent = m_MaterialAsset->IsTransparent();

		ImGui::SetNextWindowClass(&m_WindowClass);
		if (ImGui::Begin("Properties##MaterialAsset", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse))
		{
			UI::BeginPropertyGrid();

			UI::PushID();
			if (UI::Property("Transparent", transparent, "Select between opaque or transparent object"))
			{
				// TODO: Once we have a tranparent shader handle this, for now do nothing
				//if (transparent)
				//	m_MaterialAsset->SetMaterial(Material::Create(transparentShader));
				//else
				// m_MaterialAsset->SetMaterial(Material::Create(Renderer::GetShadersLibrary()->Get("IrisPBRStatic")));
				// 
				// m_MaterialAsset->m_Transparent = transparent;
				// m_MaterialAsset->SetDefaults();
				// 
				// material = m_MaterialAsset->GetMaterial();
				// shader = material->GetShader();
			}

			// TODO: Some of the settings should not be shown in case of a transparent materials. So once we have transparency we need to change this...

			bool doubleSided = m_MaterialAsset->IsDoubleSided();
			if (UI::Property("Double Sided", doubleSided, "Sets whether to show backsides or cull them"))
			{
				m_MaterialAsset->SetDoubleSided(doubleSided);
				needsSerialize = true;
			}

			bool shadowCasting = m_MaterialAsset->IsShadowCasting();
			if (UI::Property("Shadow Casting", shadowCasting, "Set whether the object will cast shadows or not"))
			{
				m_MaterialAsset->SetShadowCasting(shadowCasting);
				needsSerialize = true;
			}

			if (UI::Property("Tiling", m_MaterialAsset->GetTiling(), 0.1f, 0.01f, 100.0f, "Configure the tiling factor of the maps"))
				needsSerialize = true;

			if (UI::Property("EnvMapRotation", m_MaterialAsset->GetEnvMapRotation(), 0.1f, 0.0f, 360.0f, "Configure the Environment Map Rotation on the object"))
				needsSerialize = true;

			UI::PropertyStringReadOnly("Shader", shader->GetName().data(), false, "Shader currently used for the material");
			UI::PopID();

			UI::EndPropertyGrid();

			auto checkAndSetTexture = []() -> AssetHandle
			{
				const ImGuiPayload* data = ImGui::AcceptDragDropPayload("asset_payload");
				if (data && data->DataSize / sizeof(AssetHandle) == 1)
				{
					AssetHandle assetHandle = *reinterpret_cast<AssetHandle*>(data->Data);
					AssetType type = AssetManager::GetAssetType(assetHandle);
					if (type == AssetType::Texture)
						return assetHandle;
				}

				return static_cast<AssetHandle>(0);
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
					AssetHandle assetHandle = checkAndSetTexture();
					if (assetHandle)
					{
						Project::GetEditorAssetManager()->AddPostSyncTask([materialAsset = m_MaterialAsset, assetHandle]() mutable -> bool
						{
							AsyncAssetResult<Texture2D> result = AssetManager::GetAssetAsync<Texture2D>(assetHandle);
							if (result.IsReady)
							{
								materialAsset->SetAlbedoMap(result.Asset->Handle, true);
								AssetImporter::Serialize(materialAsset);

								return true;
							}

							return false;
						}, true);
					}

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
						AssetHandle assetHandle = checkAndSetTexture();
						if (assetHandle)
						{
							Project::GetEditorAssetManager()->AddPostSyncTask([materialAsset = m_MaterialAsset, assetHandle]() mutable -> bool
							{
								AsyncAssetResult<Texture2D> result = AssetManager::GetAssetAsync<Texture2D>(assetHandle);
								if (result.IsReady)
								{
									materialAsset->SetNormalMap(result.Asset->Handle, true);
									materialAsset->SetUseNormalMap(true);
									AssetImporter::Serialize(materialAsset);

									return true;
								}

								return false;
							}, true);
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
						AssetHandle assetHandle = checkAndSetTexture();
						if (assetHandle)
						{
							Project::GetEditorAssetManager()->AddPostSyncTask([materialAsset = m_MaterialAsset, assetHandle]() mutable -> bool
							{
								AsyncAssetResult<Texture2D> result = AssetManager::GetAssetAsync<Texture2D>(assetHandle);
								if (result.IsReady)
								{
									materialAsset->SetRoughnessMap(result.Asset->Handle, true);
									materialAsset->SetRoughness(1.0f);
									AssetImporter::Serialize(materialAsset);

									return true;
								}

								return false;
							}, true);
						}

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
						m_MaterialAsset->SetRoughness(0.4f); // Defualt back to 0.0f
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
						AssetHandle assetHandle = checkAndSetTexture();
						if (assetHandle)
						{
							Project::GetEditorAssetManager()->AddPostSyncTask([materialAsset = m_MaterialAsset, assetHandle]() mutable -> bool
							{
								AsyncAssetResult<Texture2D> result = AssetManager::GetAssetAsync<Texture2D>(assetHandle);
								if (result.IsReady)
								{
									materialAsset->SetMetalnessMap(result.Asset->Handle, true);
									materialAsset->SetMetalness(1.0f);
									AssetImporter::Serialize(materialAsset);

									return true;
								}

								return false;
							}, true);
						}

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
						m_MaterialAsset->SetMetalness(0.0f); // Default back to 0.0f
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
		}

		ImGui::End();

		if (needsSerialize)
			AssetImporter::Serialize(m_MaterialAsset);
	}

	bool MaterialEditor::OnKeyPressedEvent(Events::KeyPressedEvent& e)
	{
		if (Input::IsKeyDown(KeyCode::LeftControl) && !Input::IsMouseButtonDown(MouseButton::Right))
		{
			switch (e.GetKeyCode())
			{
				case KeyCode::G:
				{
					m_SceneRenderer->GetOptions().ShowGrid = !m_SceneRenderer->GetOptions().ShowGrid;
					e.Handled = true;
					
					break;
				}
			}

		}

		return false;
	}

}