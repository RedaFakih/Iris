#include "IrisPCH.h"
#include "AssetManager.h"

#include "Renderer/Renderer.h"
#include "Renderer/Texture.h"
#include "Scene/SceneEnvironment.h"

namespace Iris {

	using GetDefualtPlaceholderResourceFunc = Ref<Asset>(*)();

	static std::unordered_map<AssetType, GetDefualtPlaceholderResourceFunc> s_AssetPlaceHolderTable = {
		{ AssetType::Texture, []() -> Ref<Asset> { return Renderer::GetWhiteTexture(); } },
		{ AssetType::EnvironmentMap, []() -> Ref<Asset> { return Renderer::GetEmptyEnvironment(); } }
	};

	Ref<Asset> AssetManager::GetPlaceHolderAsset(AssetType type)
	{
		if (s_AssetPlaceHolderTable.contains(type))
			return s_AssetPlaceHolderTable.at(type)();

		return nullptr;
	}

}