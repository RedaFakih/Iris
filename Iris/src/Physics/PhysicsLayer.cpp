#include "IrisPCH.h"
#include "PhysicsLayer.h"

namespace Iris {

	namespace Utils {

		template<typename T, typename ConditionFunction>
		inline bool RemoveIf(std::vector<T>& vector, ConditionFunction condition)
		{
			for (auto it = vector.begin(); it != vector.end(); it++)
			{
				if (condition(*it))
				{
					vector.erase(it);
					return true;
				}
			}

			return false;
		}

	}

	uint32_t PhysicsLayerManager::AddLayer(const std::string& name, bool setCollisions)
	{
		// Check if there exists a layer with that name first
		for (const PhysicsLayer& layer : s_Layers)
		{
			if (layer.Name == name)
				return layer.LayerID;
		}

		uint32_t layerId = GetNextLayerID();
		PhysicsLayer layer = { layerId, name, static_cast<int32_t>(BIT(layerId)), static_cast<int32_t>(BIT(layerId)) };
		s_Layers.insert(s_Layers.begin() + layerId, layer);
		s_LayerNames.insert(s_LayerNames.begin() + layerId, name);

		if (setCollisions)
		{
			for (const PhysicsLayer& layer2 : s_Layers)
			{
				SetLayerCollision(layer.LayerID, layer2.LayerID, true);
			}
		}

		return layer.LayerID;
	}

	void PhysicsLayerManager::RemoveLayer(uint32_t layerId)
	{
		PhysicsLayer& layer = GetLayer(layerId);

		for (PhysicsLayer& otherLayer : s_Layers)
		{
			if (otherLayer.LayerID == layerId)
				continue;

			if (otherLayer.CollidesWith & layer.BitValue)
				otherLayer.CollidesWith &= ~layer.BitValue;
		}

		Utils::RemoveIf(s_LayerNames, [&](const std::string& name) {return name == layer.Name; });
		Utils::RemoveIf(s_Layers, [&](const PhysicsLayer& layer) {return layer.LayerID == layerId; });
	}

	void PhysicsLayerManager::UpdateLayerName(uint32_t layerId, const std::string& newName)
	{
		// Check if layer with newName does not exist first
		for (const std::string& layerName : s_LayerNames)
			if (layerName == newName)
				return;
		
		PhysicsLayer& layer = GetLayer(layerId);
		Utils::RemoveIf(s_LayerNames, [&](const std::string& name) {return name == layer.Name; });
		s_LayerNames.insert(s_LayerNames.begin() + layerId, newName);
		layer.Name = newName;
	}

	void PhysicsLayerManager::SetLayerCollision(uint32_t layerId, uint32_t otherLayerId, bool shouldCollide)
	{
		if (ShouldCollide(layerId, otherLayerId) && shouldCollide)
			return;

		PhysicsLayer& layerInfo = GetLayer(layerId);
		PhysicsLayer& otherLayerInfo = GetLayer(otherLayerId);

		if (shouldCollide)
		{
			layerInfo.CollidesWith |= otherLayerInfo.BitValue;
			otherLayerInfo.CollidesWith |= layerInfo.BitValue;
		}
		else
		{
			layerInfo.CollidesWith &= ~otherLayerInfo.BitValue;
			otherLayerInfo.CollidesWith &= ~layerInfo.BitValue;
		}
	}

	std::vector<PhysicsLayer> PhysicsLayerManager::GetLayerCollisions(uint32_t layerId)
	{
		const PhysicsLayer& layerInfo = GetLayer(layerId);

		std::vector<PhysicsLayer> result;
		for (const PhysicsLayer& layer : s_Layers)
		{
			if (layer.LayerID == layerId)
				continue;

			if (layerInfo.CollidesWith & layer.BitValue)
				result.push_back(layer);
		}

		return result;
	}

	PhysicsLayer& PhysicsLayerManager::GetLayer(uint32_t layerId)
	{
		return layerId >= s_Layers.size() ? s_NullLayer : s_Layers[layerId];
	}

	PhysicsLayer& PhysicsLayerManager::GetLayer(const std::string& layerName)
	{
		for (PhysicsLayer& layer : s_Layers)
		{
			if (layer.Name == layerName)
				return layer;
		}

		return s_NullLayer;
	}

	bool PhysicsLayerManager::ShouldCollide(uint32_t layer1Id, uint32_t layer2Id)
	{
		return GetLayer(layer1Id).CollidesWith & GetLayer(layer2Id).BitValue;
	}

	bool PhysicsLayerManager::IsLayerValid(uint32_t layerId)
	{
		const PhysicsLayer& layer = GetLayer(layerId);
		return layer.LayerID != s_NullLayer.LayerID && layer.IsValid();
	}

	bool PhysicsLayerManager::IsLayerValid(const std::string& layerName)
	{
		const PhysicsLayer& layer = GetLayer(layerName);
		return layer.LayerID != s_NullLayer.LayerID && layer.IsValid();
	}

	void PhysicsLayerManager::ClearLayers()
	{
		s_Layers.clear();
		s_LayerNames.clear();
	}

	uint32_t PhysicsLayerManager::GetNextLayerID()
	{
		int32_t lastId = -1;
		
		for (const PhysicsLayer& layer : s_Layers)
		{
			if (lastId != -1 && static_cast<int32_t>(layer.LayerID) != lastId + 1)
				return static_cast<uint32_t>(lastId + 1);

			lastId = layer.LayerID;
		}

		return static_cast<uint32_t>(s_Layers.size());
	}

}