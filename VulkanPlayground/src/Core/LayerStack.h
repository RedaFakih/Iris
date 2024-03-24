#pragma once

#include "Layer.h"

#include <vector>
#include <utility>

namespace vkPlayground {

	class LayerStack
	{
	public:
		LayerStack() = default;
		~LayerStack() = default;

		void PushLayer(Layer* layer)
		{
			m_Layers.emplace(m_Layers.begin() + m_LayerInsertIndex, layer);
			m_LayerInsertIndex++;
		}

		void PopLayer(Layer* layer)
		{
			auto it = std::find(m_Layers.begin(), m_Layers.end(), layer);
			if (it != m_Layers.end())
			{
				m_Layers.erase(it);
				m_LayerInsertIndex--;
			}
		}

		void PushOverlay(Layer* overlay)
		{
			m_Layers.emplace_back(overlay);
		}

		void PopOverlay(Layer* overlay)
		{
			auto it = std::find(m_Layers.begin(), m_Layers.end(), overlay);
			if (it != m_Layers.end())
				m_Layers.erase(it);
		}

		Layer* operator[](std::size_t index)
		{
			VKPG_ASSERT(index >= 0 && index < m_Layers.size());
			return m_Layers[index];
		}

		const Layer* operator[](std::size_t index) const
		{
			VKPG_ASSERT(index >= 0 && index < m_Layers.size());
			return m_Layers[index];
		}

		std::size_t GetSize() const { return m_Layers.size(); }

		std::vector<Layer*>::iterator begin() { return m_Layers.begin(); }
		std::vector<Layer*>::iterator end() { return m_Layers.end(); }

	private:
		std::vector<Layer*> m_Layers;
		uint32_t m_LayerInsertIndex = 0;
	};

}