#include "IrisPCH.h"
#include "AssetRegistry.h"

namespace Iris {

#define IR_ASSETREGISTRY_LOG 0
#if IR_ASSETREGISTRY_LOG
#define ASSET_LOG(...) IR_CORE_TRACE_TAG("ASSET", __VA_ARGS__)
#else 
#define ASSET_LOG(...)
#endif

	static std::mutex s_AssetRegistryMutex;

	bool AssetRegistry::Contains(const AssetHandle handle) const
	{
		std::scoped_lock<std::mutex> lock(s_AssetRegistryMutex);

		ASSET_LOG("Contains handle {}", handle);
		return m_AssetRegistry.contains(handle);
	}

	std::size_t AssetRegistry::Remove(const AssetHandle handle)
	{
		std::scoped_lock<std::mutex> lock(s_AssetRegistryMutex);

		ASSET_LOG("Removing handle", handle);
		return m_AssetRegistry.erase(handle);
	}

	void AssetRegistry::Clear()
	{
		std::scoped_lock<std::mutex> lock(s_AssetRegistryMutex);

		ASSET_LOG("Clearing registry");
		m_AssetRegistry.clear();
	}

	AssetMetaData& AssetRegistry::Get(const AssetHandle handle)
	{
		std::scoped_lock<std::mutex> lock(s_AssetRegistryMutex);

		ASSET_LOG("Retrieving handle {}", handle);
		return m_AssetRegistry[handle];
	}

	const AssetMetaData& AssetRegistry::Get(const AssetHandle handle) const
	{
		std::scoped_lock<std::mutex> lock(s_AssetRegistryMutex);

		IR_ASSERT(m_AssetRegistry.contains(handle));
		ASSET_LOG("Retrieving const handle {}", handle);
		return m_AssetRegistry.at(handle);
	}

	AssetMetaData& AssetRegistry::operator[](const AssetHandle handle)
	{
		std::scoped_lock<std::mutex> lock(s_AssetRegistryMutex);

		ASSET_LOG("Retrieving handle {}", handle);
		return m_AssetRegistry[handle];
	}

	const AssetMetaData& AssetRegistry::operator[](const AssetHandle handle) const
	{
		std::scoped_lock<std::mutex> lock(s_AssetRegistryMutex);

		ASSET_LOG("Retrieving handle {}", handle);
		return m_AssetRegistry.at(handle);
	}

}