#pragma once

namespace Iris {

	struct PhysicsLayer
	{
		uint32_t LayerID;
		std::string Name;
		int32_t BitValue;
		int32_t CollidesWith = 0;
		bool CollidesWithSelf = true;

		bool IsValid() const
		{
			return !Name.empty() && BitValue > 0;
		}
	};

	// Fully static since the physics layers are basically global information in the project and not scene specific
	class PhysicsLayerManager
	{
	public:
		static uint32_t AddLayer(const std::string& name, bool setCollisions = true);
		static void RemoveLayer(uint32_t layerId);

		static void UpdateLayerName(uint32_t layerId, const std::string& newName);

		static void SetLayerCollision(uint32_t layerId, uint32_t otherLayerId, bool shouldCollide);
		static std::vector<PhysicsLayer> GetLayerCollisions(uint32_t layerId);

		inline static const std::vector<PhysicsLayer>& GetLayers() { return s_Layers; }
		inline static const std::vector<std::string>& GetLayerNames() { return s_LayerNames; }

		static PhysicsLayer& GetLayer(uint32_t layerId);
		static PhysicsLayer& GetLayer(const std::string& layerName);
		inline static uint32_t GetLayerCount() { return static_cast<uint32_t>(s_Layers.size()); }

		static bool ShouldCollide(uint32_t layer1Id, uint32_t layer2Id);
		static bool IsLayerValid(uint32_t layerId);
		static bool IsLayerValid(const std::string& layerName);

		static void ClearLayers();

	private:
		static uint32_t GetNextLayerID();

	private:
		inline static std::vector<PhysicsLayer> s_Layers;
		// NOTE: We cache this since we need it editor UI which happens every frame
		inline static std::vector<std::string> s_LayerNames;

		inline static PhysicsLayer s_NullLayer = { static_cast<uint32_t>(-1), "NULL LAYER", -1, -1 };

	};

}