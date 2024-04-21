#pragma once

#include "Core/UUID.h"
#include "AssetTypes.h"

namespace Iris {

	using AssetHandle = UUID;

	class Asset : public RefCountedObject
	{
	public:
		virtual ~Asset() {}

		static AssetType GetStaticType() { return AssetType::None; }
		virtual AssetType GetAssetType() const { return AssetType::None; }

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

		bool IsValid() const { return ((Flags & static_cast<uint8_t>(AssetFlag::Missing)) | (Flags & static_cast<uint8_t>(AssetFlag::Invalid))) == 0; }

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
		uint8_t Flags = static_cast<uint8_t>(AssetFlag::Invalid);
	};

}