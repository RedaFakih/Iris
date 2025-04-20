#pragma once

#include "AssetManager/Asset/AssetTypes.h"
#include "Renderer/Core/RenderCommandBuffer.h"
#include "Renderer/SceneRendererLite.h"
#include "Renderer/Texture.h"
#include "Scene/Scene.h"

#include <set>

namespace Iris {

	struct ThumbnailImage
	{
		Ref<Texture2D> Image;
		uint64_t LastWriteTime; // TODO: Would be better to just use a FileWatch 
	};

	class AssetThumbnailGenerator : public RefCountedObject
	{
	public:
		// Setup Scene and SceneRenderer as required
		virtual void OnPrepare(AssetHandle assetHandle, Ref<Scene> scene, Ref<SceneRendererLite> sceneRenderer) = 0;
		// Undo all Scene/SceneRenderer changes
		virtual void OnFinish(AssetHandle assetHandle, Ref<Scene> scene, Ref<SceneRendererLite> sceneRenderer) = 0;
	};

	class ThumbnailGenerator : public RefCountedObject
	{
	public:
		ThumbnailGenerator();
		~ThumbnailGenerator() = default;

		[[nodiscard]] inline static Ref<ThumbnailGenerator> Create()
		{
			return CreateRef<ThumbnailGenerator>();
		}

		Ref<Texture2D> GenerateThumbnail(AssetHandle handle);

		bool IsThumbnailSupported(AssetType type)
		{
			return s_SupportedThumbnailAssetTypes.contains(type);
		}

	private:
		Ref<Scene> m_Scene, m_EnvironmentScene;
		Ref<SceneRendererLite> m_SceneRenderer;
		Ref<RenderCommandBuffer> m_RenderCommandBuffer; // For blitting

		Entity m_CameraEntity;

		uint32_t m_Width = 256;
		uint32_t m_Height = 256;

		std::unordered_map<AssetType, Ref<AssetThumbnailGenerator>> m_AssetThumbnailGenerators;

	public:
		inline static std::set<AssetType> s_SupportedThumbnailAssetTypes = {
			AssetType::Texture,
			AssetType::Material,
			AssetType::MeshSource,
			AssetType::StaticMesh
			// AssetType::EnvironmentMap // TODO: This is not supported for now since my device does not have enough memory to load multiple environment maps
		};

	};

}