#pragma once

#include "Asset/AssetMetaData.h"

#include <unordered_map>

namespace Iris {

	class AssetRegistry
	{
	public:
		std::size_t Size() const { return m_AssetRegistry.size(); }
		bool Contains(const AssetHandle handle) const;
		std::size_t Remove(const AssetHandle handle);
		void Clear();

		auto begin() { return m_AssetRegistry.begin(); }
		auto end() { return m_AssetRegistry.end(); }
		auto begin() const { return m_AssetRegistry.cbegin(); }
		auto end() const { return m_AssetRegistry.cend(); }

		AssetMetaData& Get(const AssetHandle handle);
		const AssetMetaData& Get(const AssetHandle handle) const;
		AssetMetaData& operator[](const AssetHandle handle);
		const AssetMetaData& operator[](const AssetHandle handle) const;

	private:
		std::unordered_map<AssetHandle, AssetMetaData> m_AssetRegistry;

	};

}