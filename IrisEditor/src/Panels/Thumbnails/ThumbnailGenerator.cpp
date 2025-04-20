#include "ThumbnailGenerator.h"

#include "AssetManager/AssetManager.h"

namespace Iris {

	namespace Utils {

		static Entity CreateDefaultCamera(Ref<Scene> scene, float cameraDistance = 1.5f)
		{
			Entity cameraEntity = scene->CreateEntity("Camera");
			CameraComponent& cc = cameraEntity.AddComponent<CameraComponent>();
			cc.Camera.SetPerspective(40.0f, 0.1f, 100.0f);
			cc.Primary = true;
			TransformComponent& cameraTransform = cameraEntity.GetComponent<TransformComponent>();
			cameraTransform.Translation = { 0.0f, 0.0f, cameraDistance };
			return cameraEntity;
		}

		static Entity CreateMeshCamera(Ref<Scene> scene, AABB meshBoundingBox, float cameraDistance = 1.5f)
		{
			Entity cameraEntity = scene->CreateEntity("Camera");
			CameraComponent& cc = cameraEntity.AddComponent<CameraComponent>();
			cc.Camera.SetPerspective(45.0f, 0.1f, 1000.0f);
			cc.Primary = true;
			TransformComponent& cameraTransform = cameraEntity.GetComponent<TransformComponent>();
			cameraTransform.Translation = { -5.0f, 5.0f, 5.0f };
			cameraTransform.SetRotationEuler({ glm::radians(-27.0f), glm::radians(40.0f), 0.0f });
			glm::vec3 cameraForward = cameraTransform.GetRotation() * glm::vec3{ 0.0f, 0.0f, -1.0f };

			glm::vec3 objectSizes = meshBoundingBox.Size();
			float objectSize = glm::max(glm::max(objectSizes.x, objectSizes.y), objectSizes.z);
			float cameraView = 2.0f * glm::tan(0.5f * cc.Camera.GetRadPerspectiveVerticalFOV());
			float distance = cameraDistance * objectSize / cameraView;
			distance += 0.5f * objectSize;
			glm::vec3 translation = meshBoundingBox.Center() - distance * cameraForward;
			cameraTransform.Translation = translation;

			return cameraEntity;
		}

		static Entity CreateInvalidText(Ref<Scene> scene)
		{
			Entity textEntity = scene->CreateEntity("Text");
			TransformComponent& transform = textEntity.GetComponent<TransformComponent>();
			transform.Translation = { -0.6f, -0.6f, 0.0f };
			transform.Scale = glm::vec3(0.2f);
			TextComponent& tc = textEntity.AddComponent<TextComponent>();
			tc.Color = { 0.8f, 0.1f, 0.1f, 1.0f };
			tc.TextString = "INVALID";
			return textEntity;
		}

	}

	class EnvironmentThumbnailGenerator : public AssetThumbnailGenerator
	{
	public:
		EnvironmentThumbnailGenerator()
		{
			// TODO: get default sphere?
			m_Sphere = 18398963716275400970;
		}
		~EnvironmentThumbnailGenerator() = default;

		[[nodiscard]] inline static Ref<EnvironmentThumbnailGenerator> Create()
		{
			return CreateRef<EnvironmentThumbnailGenerator>();
		}

		virtual void OnPrepare(AssetHandle assetHandle, Ref<Scene> scene, Ref<SceneRendererLite> sceneRenderer) override
		{
			m_DirectionalLight = scene->CreateEntity("DirectionalLight");
			m_DirectionalLight.GetComponent<TransformComponent>().SetRotationEuler(glm::vec3(glm::radians(-90.0f), 0.0f, glm::radians(180.0f)));
			DirectionalLightComponent& dlc = m_DirectionalLight.AddComponent<DirectionalLightComponent>();
			dlc.Intensity = 0.0f;

			m_Entity = scene->CreateEntity("Model");
			StaticMeshComponent& smc = m_Entity.AddComponent<StaticMeshComponent>(m_Sphere);
			Ref<MaterialAsset> material = MaterialAsset::Create();
			material->SetAlbedoColor(glm::vec3(1.0f));
			material->SetMetalness(1.0f);
			material->SetRoughness(0.0f);
			AssetHandle materialH = AssetManager::AddMemoryOnlyAsset(material);
			smc.MaterialTable->SetMaterial(0, materialH);

			m_SkyLight = scene->CreateEntity("Skylight");
			SkyLightComponent& slc = m_SkyLight.AddComponent<SkyLightComponent>();
			slc.SceneEnvironment = assetHandle;
			slc.Intensity = 0.7f;
			slc.Lod = 1.5f;

			m_CameraEntity = Utils::CreateDefaultCamera(scene, 0.9f);
			m_CameraEntity.GetComponent<CameraComponent>().Camera.SetDegPerspectiveVerticalFOV(80.0f);
		}

		virtual void OnFinish(AssetHandle assetHandle, Ref<Scene> scene, Ref<SceneRendererLite> sceneRenderer) override
		{
			scene->DestroyEntity(m_Entity);
			scene->DestroyEntity(m_DirectionalLight);
			scene->DestroyEntity(m_SkyLight);
			scene->DestroyEntity(m_CameraEntity);
		}
	private:
		AssetHandle m_Sphere = 0;

		Entity m_Entity;
		Entity m_DirectionalLight;
		Entity m_SkyLight;
		Entity m_CameraEntity;
	};

	class MaterialThumbnailGenerator : public AssetThumbnailGenerator
	{
	public:
		MaterialThumbnailGenerator()
		{
			// TODO: get default sphere?
			m_Sphere = 18398963716275400970;
		}
		~MaterialThumbnailGenerator() = default;

		[[nodiscard]] inline static Ref<MaterialThumbnailGenerator> Create()
		{
			return CreateRef<MaterialThumbnailGenerator>();
		}

		virtual void OnPrepare(AssetHandle assetHandle, Ref<Scene> scene, Ref<SceneRendererLite> sceneRenderer) override
		{
			m_DirectionalLight = scene->CreateEntity("DirectionalLight");
			m_DirectionalLight.GetComponent<TransformComponent>().SetRotationEuler(glm::vec3(glm::radians(-90.0f), 0.0f, glm::radians(180.0f)));
			DirectionalLightComponent& dlc = m_DirectionalLight.AddComponent<DirectionalLightComponent>();
			dlc.Intensity = 0.5f;

			m_Entity = scene->CreateEntity("Model");
			StaticMeshComponent& smc = m_Entity.AddComponent<StaticMeshComponent>();
			smc.StaticMesh = m_Sphere;
			smc.MaterialTable->SetMaterial(0, assetHandle);

			m_CameraEntity = Utils::CreateDefaultCamera(scene, 1.75f);
		}

		virtual void OnFinish(AssetHandle assetHandle, Ref<Scene> scene, Ref<SceneRendererLite> sceneRenderer) override
		{
			scene->DestroyEntity(m_Entity);
			scene->DestroyEntity(m_DirectionalLight);
			scene->DestroyEntity(m_CameraEntity);
		}
	private:
		AssetHandle m_Sphere = 0;

		Entity m_Entity;
		Entity m_DirectionalLight;
		Entity m_CameraEntity;
	};

	class StaticMeshThumbnailGenerator : public AssetThumbnailGenerator
	{
	public:
		StaticMeshThumbnailGenerator() = default;
		~StaticMeshThumbnailGenerator() = default;

		[[nodiscard]] inline static Ref<StaticMeshThumbnailGenerator> Create()
		{
			return CreateRef<StaticMeshThumbnailGenerator>();
		}

		virtual void OnPrepare(AssetHandle assetHandle, Ref<Scene> scene, Ref<SceneRendererLite> sceneRenderer) override
		{
			m_TextEntity = {};

			m_DirectionalLight = scene->CreateEntity("DirectionalLight");
			m_DirectionalLight.GetComponent<TransformComponent>().SetRotationEuler(glm::vec3(glm::radians(188.0f), glm::radians(68.0f), 0.0f));
			DirectionalLightComponent& dlc = m_DirectionalLight.AddComponent<DirectionalLightComponent>();
			dlc.Intensity = 0.5f;

			Ref<StaticMesh> staticMesh = AssetManager::GetAsset<StaticMesh>(assetHandle);
			if (staticMesh)
			{
				m_Entity = scene->InstantiateStaticMesh(staticMesh);

				Ref<MeshSource> meshSource = AssetManager::GetAsset<MeshSource>(staticMesh->GetMeshSource());
				if (meshSource)
				{
					AABB boundingBox = meshSource->GetBoundingBox();
					m_CameraEntity = Utils::CreateMeshCamera(scene, boundingBox);
				}
			}

			if (!m_CameraEntity)
			{
				m_CameraEntity = Utils::CreateDefaultCamera(scene, 1.75f);
				m_TextEntity = Utils::CreateInvalidText(scene);
			}
		}

		virtual void OnFinish(AssetHandle assetHandle, Ref<Scene> scene, Ref<SceneRendererLite> sceneRenderer) override
		{
			scene->DestroyEntity(m_Entity);
			scene->DestroyEntity(m_DirectionalLight);
			scene->DestroyEntity(m_CameraEntity);
			scene->DestroyEntity(m_TextEntity);
		}
	private:
		Entity m_Entity;
		Entity m_DirectionalLight;
		Entity m_CameraEntity;
		Entity m_TextEntity;
	};

	class MeshSourceThumbnailGenerator : public AssetThumbnailGenerator
	{
	public:
		MeshSourceThumbnailGenerator() = default;
		virtual ~MeshSourceThumbnailGenerator() = default;

		[[nodiscard]] inline static Ref<MeshSourceThumbnailGenerator> Create()
		{
			return CreateRef<MeshSourceThumbnailGenerator>();
		}

		virtual void OnPrepare(AssetHandle assetHandle, Ref<Scene> scene, Ref<SceneRendererLite> sceneRenderer) override
		{
			m_TextEntity = {};

			m_DirectionalLight = scene->CreateEntity("DirectionalLight");
			m_DirectionalLight.GetComponent<TransformComponent>().SetRotationEuler(glm::vec3(glm::radians(188.0f), glm::radians(68.0f), 0.0f));
			DirectionalLightComponent& dlc = m_DirectionalLight.AddComponent<DirectionalLightComponent>();
			dlc.Intensity = 0.5f;

			float cameraDistance = 1.5f;
			m_Entity = scene->CreateEntity("Model");

			Ref<MeshSource> meshSource = AssetManager::GetAsset<MeshSource>(assetHandle);
			if (meshSource)
			{
				// Look through asset dependencies to see if we already have a mesh or static mesh referencing this mesh source.
				std::unordered_set<AssetHandle> staticMeshHandles = AssetManager::GetAllAssetsWithType<StaticMesh>();
				std::unordered_set<AssetHandle> candidateAssetHandles;
				// TODO:
				//for (auto handle : staticMeshHandles)
				//{
				//	// not AssetManager::GetAsset<>(...) here because that forces load of the whole asset.
				//	auto dependencies = Project::GetEditorAssetManager()->GetDependencies(assetHandle);
				//	for (auto dependency : dependencies)
				//	{
				//		if (handle == dependency)
				//		{
				//			candidateAssetHandles.insert(handle);
				//		}
				//	}
				//}

				auto instantiateAsset = [this, &scene, &meshSource](AssetHandle handle)
				{
					const auto& metadata = Project::GetEditorAssetManager()->GetMetaData(handle);
					if (metadata.Type == AssetType::StaticMesh)
					{
						m_Entity = scene->InstantiateStaticMesh(AssetManager::GetAsset<StaticMesh>(handle));
						AABB boundingBox = meshSource->GetBoundingBox();
						m_CameraEntity = Utils::CreateMeshCamera(scene, boundingBox);
						return;
					}
				};

				// If any of the candidates are already loaded, use that one
				for (UUID candidateHandle : candidateAssetHandles)
				{
					if (Project::GetEditorAssetManager()->IsAssetLoaded(candidateHandle))
					{
						instantiateAsset(candidateHandle);
						return;
					}
				}

				// No assets that use meshsource are loaded, just load up the first one and instantiate that
				if (!candidateAssetHandles.empty())
				{
					instantiateAsset(*candidateAssetHandles.begin());
					return;
				}

				// No existing assets use meshsouce, create a new memory only static mesh
				Ref<StaticMesh> staticMesh = AssetManager::GetAsset<StaticMesh>(AssetManager::AddMemoryOnlyAsset(StaticMesh::Create(assetHandle, false)));
				if (staticMesh)
				{
					m_Entity = scene->InstantiateStaticMesh(staticMesh);
					AABB boundingBox = meshSource->GetBoundingBox();
					m_CameraEntity = Utils::CreateMeshCamera(scene, boundingBox);
					return;
				}
			}

			// TODO: error text
			m_CameraEntity = Utils::CreateDefaultCamera(scene, 1.75f);
			m_TextEntity = Utils::CreateInvalidText(scene);
		}

		virtual void OnFinish(AssetHandle assetHandle, Ref<Scene> scene, Ref<SceneRendererLite> sceneRenderer) override
		{
			scene->DestroyEntity(m_Entity);
			scene->DestroyEntity(m_DirectionalLight);
			scene->DestroyEntity(m_CameraEntity);
			scene->DestroyEntity(m_TextEntity);
		}
	private:
		Entity m_Entity;
		Entity m_DirectionalLight;
		Entity m_CameraEntity;
		Entity m_TextEntity;
	};

	ThumbnailGenerator::ThumbnailGenerator()
	{
		// Fill all different asset type generators
		m_AssetThumbnailGenerators[AssetType::EnvironmentMap] = EnvironmentThumbnailGenerator::Create();
		m_AssetThumbnailGenerators[AssetType::Material] = MaterialThumbnailGenerator::Create();
		m_AssetThumbnailGenerators[AssetType::StaticMesh] = StaticMeshThumbnailGenerator::Create();
		m_AssetThumbnailGenerators[AssetType::MeshSource] = MeshSourceThumbnailGenerator::Create();

		m_Scene = Scene::Create("ThumbnailGenerator", true);
		m_Scene->SetViewportSize(m_Width, m_Height);

		// SkyLight
		{
			Entity skyLight = m_Scene->CreateEntity("SkyLight");
			SkyLightComponent& slc = skyLight.AddComponent<SkyLightComponent>();

			// TODO: Handle the environment map better, cant have a filepath thrown in here
			auto [radiance, irradiance] = Renderer::CreateEnvironmentMap("Resources/EnvironmentMaps/pink_sunrise_1k.hdr");
			AssetHandle handle = AssetManager::AddMemoryOnlyAsset(Environment::Create(radiance, irradiance));
			slc.SceneEnvironment = handle;
			slc.Intensity = 0.8f;
			slc.Lod = AssetManager::GetAsset<Environment>(handle)->RadianceMap->GetMipLevelCount() - 3.0f;
		}

		// Render at 512x512 then display with the content browser item thumbnail size, if less/bigger then we downscale/upscale with Blit
		m_SceneRenderer = SceneRendererLite::Create(m_Scene, { .ViewportWidth = 512, .ViewportHeight = 512 });
		m_SceneRenderer->GetOptions().ShowGrid = false;
		m_SceneRenderer->GetOptions().BloomIntensity = 0.5f;
		m_SceneRenderer->GetOptions().BloomFilterThreshold = 1.25f;

		// TODO: Add camera for this?
		m_Scene->OnRenderRuntime(m_SceneRenderer);

		m_EnvironmentScene = Scene::Create("ThumbnailGenerator-EnvironmentScene");
		m_EnvironmentScene->SetViewportSize(m_Width, m_Height);
		m_EnvironmentScene->OnRenderRuntime(m_SceneRenderer);

		m_RenderCommandBuffer = RenderCommandBuffer::Create(1);
	}

	Ref<Texture2D> ThumbnailGenerator::GenerateThumbnail(AssetHandle handle)
	{
		if (!m_SceneRenderer->IsReady())
			return nullptr;

		if (!AssetManager::IsAssetHandleValid(handle) || !AssetManager::IsAssetValid(handle))
			return nullptr;

		// TODO: Maybe wont be needed since we will use a filewatcher
		// AssetManager::EnsureCurrent(handle);

		const AssetMetaData& metadata = Project::GetEditorAssetManager()->GetMetaData(handle);
		if (metadata.Type == AssetType::Texture)
		{
			Ref<Texture2D> texture = AssetManager::GetAsset<Texture2D>(handle);

			uint32_t textureWidth = texture->GetWidth();
			uint32_t textureHeight = texture->GetHeight();
			float verticalAspectRatio = 1.0f;
			float horizontalAspectRatio = 1.0f;
			if (textureWidth > textureHeight)
				verticalAspectRatio = static_cast<float>(textureHeight) / static_cast<float>(textureWidth);
			else
				horizontalAspectRatio = static_cast<float>(textureWidth) / static_cast<float>(textureHeight);

			TextureSpecification imageSpec = {
				.Width = static_cast<uint32_t>(m_Width * horizontalAspectRatio),
				.Height = static_cast<uint32_t>(m_Height * verticalAspectRatio),
				.Format = ImageFormat::RGBA,
				.Usage = ImageUsage::Texture,
				.GenerateMips = false
			};
			Ref<Texture2D> result = Texture2D::Create(imageSpec);

			m_RenderCommandBuffer->Begin();
			// Fragment shader since it will be displayed by ImGui
			Renderer::BlitImage(m_RenderCommandBuffer, texture, result, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT);
			m_RenderCommandBuffer->End();
			m_RenderCommandBuffer->Submit();

			return result;
		}

		if (m_AssetThumbnailGenerators.find(metadata.Type) == m_AssetThumbnailGenerators.end())
			return nullptr;

		Ref<Scene> scene = metadata.Type == AssetType::EnvironmentMap ? m_EnvironmentScene : m_Scene;

		m_AssetThumbnailGenerators.at(metadata.Type)->OnPrepare(handle, scene, m_SceneRenderer);

		scene->OnRenderRuntime(m_SceneRenderer);

		m_AssetThumbnailGenerators.at(metadata.Type)->OnFinish(handle, scene, m_SceneRenderer);

		if (!m_SceneRenderer->GetFinalPassImage()->GetVulkanImage())
		{
			IR_CORE_ERROR("Retruned Image is null!");
			return nullptr;
		}

		TextureSpecification imageSpec = {
			.Width = static_cast<uint32_t>(m_Width),
			.Height = static_cast<uint32_t>(m_Height),
			.Format = ImageFormat::RGBA,
			.Usage = ImageUsage::Texture,
			.GenerateMips = false
		};
		Ref<Texture2D> result = Texture2D::Create(imageSpec);

		m_RenderCommandBuffer->Begin();
		Renderer::BlitImage(m_RenderCommandBuffer, m_SceneRenderer->GetFinalPassImage(), result, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT);
		m_RenderCommandBuffer->End();
		m_RenderCommandBuffer->Submit();

		return result;
	}

}