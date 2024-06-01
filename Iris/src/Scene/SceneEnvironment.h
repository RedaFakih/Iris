#pragma once

#include "AssetManager/Asset/Asset.h"
#include "Renderer/Renderer.h"

namespace Iris {

	class Environment : public Asset
	{
	public:
		Ref<TextureCube> RadianceMap;
		Ref<TextureCube> IrradianceMap;

		Environment() = default;
		Environment(const Ref<TextureCube>& radianceMap, const Ref<TextureCube>& irradianceMap)
			: RadianceMap(radianceMap), IrradianceMap(irradianceMap) {}

		[[nodiscard]] inline static Ref<Environment> Create(const Ref<TextureCube>& radianceMap, const Ref<TextureCube>& irradianceMap)
		{
			return CreateRef<Environment>(radianceMap, irradianceMap);
		}

		static AssetType GetStaticType() { return AssetType::EnvironmentMap; }
		virtual AssetType GetAssetType() const override { return GetStaticType(); }
	};

}