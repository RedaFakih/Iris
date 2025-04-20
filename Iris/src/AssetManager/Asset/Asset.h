#pragma once

#include "AssetTypes.h"
#include "Core/UUID.h"

namespace Iris {

	using AssetHandle = UUID;

	class Asset : public RefCountedObject
	{
	public:
		virtual ~Asset() {}

		static AssetType GetStaticType() { return AssetType::None; }
		virtual AssetType GetAssetType() const { return AssetType::None; }

		// When a dependency for the current asset is updated (e.g. Texture for a mesh updated -> Mesh should be updated)
		virtual void OnDependencyUpdated(AssetHandle handle) { (void)handle; }

		virtual bool operator==(const Asset& other) const
		{
			return Handle == other.Handle;
		}

		virtual bool operator!=(const Asset& other) const
		{
			return !(*this == other);
		}

	private:
		// If you want to find out whether assets are valid or missing, use AssetManager::IsAssetValid(handle), IsAssetMissing(handle)
		// This cleans up and removes inconsistencies from rest of the code.
		// You simply go AssetManager::GetAsset<Whatever>(handle), and so long as you get a non-null pointer back, you're good to go.
		// No IsValid(), IsFlagSet(AssetFlag::Missing) etc. etc. all throughout the code.
		friend class EditorAssetManager;
		friend class RuntimeAssetManager;
		friend class AssimpMeshImporter;
		friend struct MeshSourceSerializer;
		friend struct MeshColliderSerializer;
		friend struct StaticMeshSerializer;
		friend struct TextureSerializer;
		friend struct MaterialAssetSerializer;

		bool IsValid() const { return ((Flags & static_cast<uint8_t>(AssetFlag::Missing))| (Flags & static_cast<uint8_t>(AssetFlag::Invalid))) == 0; }

		bool IsFlagSet(AssetFlag flag) const { return static_cast<uint8_t>(flag) & Flags; }
		void SetFlag(AssetFlag flag, bool value = true)
		{
			if (value)
				Flags |= static_cast<uint8_t>(flag);
			else
				Flags &= ~static_cast<uint8_t>(flag);
		}

	public:
		AssetHandle Handle = 0;
		uint8_t Flags = static_cast<uint8_t>(AssetFlag::None);
	};

	template<typename T>
	struct AsyncAssetResult
	{
		Ref<T> Asset = nullptr;
		bool IsReady = false;

		AsyncAssetResult() = default;

		AsyncAssetResult(Ref<T> asset, bool isReady)
			: Asset(asset), IsReady(isReady) {}

		template<typename T2>
		AsyncAssetResult(const AsyncAssetResult<T2>& other)
			: Asset(other.Asset.As<T>()), IsReady(other.IsReady) {}

		// Return the asset, we do not care if it is a placeholder
		operator Ref<T>() const { return Asset; }
	};

}