#pragma once

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>

namespace Iris {

	class JoltLayerInterface : public JPH::BroadPhaseLayerInterface
	{
	public:
		JoltLayerInterface();
		virtual ~JoltLayerInterface() = default;

		inline virtual uint32_t GetNumBroadPhaseLayers() const override { return static_cast<uint32_t>(m_BroadPhaseLayers.size()); }
		virtual JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer inLayer) const override;

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
		/// Get the user readable name of a broadphase layer (debugging purposes)
		virtual const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer inLayer) const override;
#endif // JPH_EXTERNAL_PROFILE || JPH_PROFILE_ENABLED

	private:
		std::vector <JPH::BroadPhaseLayer> m_BroadPhaseLayers;

	};

}