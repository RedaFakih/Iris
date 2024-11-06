#include "IrisPCH.h"
#include "LayerInterface.h"

#include "PhysicsLayer.h"

namespace Iris {

	JoltLayerInterface::JoltLayerInterface()
	{
		uint32_t layerCount = PhysicsLayerManager::GetLayerCount();
		m_BroadPhaseLayers.resize(layerCount);

		for (const PhysicsLayer& layer : PhysicsLayerManager::GetLayers())
			m_BroadPhaseLayers[layer.LayerID] = JPH::BroadPhaseLayer(layer.LayerID);
	}

	JPH::BroadPhaseLayer JoltLayerInterface::GetBroadPhaseLayer(JPH::ObjectLayer inLayer) const
	{
		IR_VERIFY(inLayer < m_BroadPhaseLayers.size());
		return m_BroadPhaseLayers[inLayer];
	}

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
	/// Get the user readable name of a broadphase layer (debugging purposes)
	const char* JoltLayerInterface::GetBroadPhaseLayerName(JPH::BroadPhaseLayer inLayer) const
	{
		const auto& layer = PhysicsLayerManager::GetLayer(static_cast<uint32_t>(static_cast<uint8_t>(inLayer)));
		return layer.Name.c_str();
	}
#endif // JPH_EXTERNAL_PROFILE || JPH_PROFILE_ENABLED

}