#include "IrisPCH.h"
#include "SelectionManager.h"

#include "Renderer/StorageBufferSet.h"
#include "Renderer/Texture.h"
#include "Renderer/UniformBufferSet.h"

namespace Iris {

	void SelectionManager::Select(SelectionContext context, UUID selectionID)
	{
		auto& selectedContext = s_Contexts[context];
		if (std::find(selectedContext.begin(), selectedContext.end(), selectionID) != selectedContext.end())
			return;

		selectedContext.push_back(selectionID);
		// TODO: Dispatch selection changed event...? SelectionChangedEvent
	}

	bool SelectionManager::IsSelected(UUID selectionID)
	{
		for (const auto& [context, contextSelections] : s_Contexts)
		{
			if (std::find(contextSelections.begin(), contextSelections.end(), selectionID) != contextSelections.end())
			{
				return true;
			}
		}

		return false;
	}

	bool SelectionManager::IsSelected(SelectionContext context, UUID selectionID)
	{
		const auto& contextSelections = s_Contexts[context];
		return std::find(contextSelections.begin(), contextSelections.end(), selectionID) != contextSelections.end();
	}

	bool SelectionManager::IsEntityOrAncestorSelected(const Entity entity)
	{
		Entity e = entity;
		while (e)
		{
			if (IsSelected(e.GetUUID()))
			{
				return true;
			}
			e = e.GetParent();
		}
		return false;
	}

	bool SelectionManager::IsEntityOrAncestorSelected(SelectionContext context, const Entity entity)
	{
		Entity e = entity;
		while (e)
		{
			if (IsSelected(context, e.GetUUID()))
			{
				return true;
			}
			e = e.GetParent();
		}
		return false;
	}

	void SelectionManager::Deselect(UUID selectionID)
	{
		for (auto& [contextID, contextSelections] : s_Contexts)
		{
			auto it = std::find(contextSelections.begin(), contextSelections.end(), selectionID);
			if (it == contextSelections.end())
				continue;

			// TODO: Dispatch selection changed event...? SelectionChangedEvent
			contextSelections.erase(it);
			break;
		}
	}

	void SelectionManager::Deselect(SelectionContext context, UUID selectionID)
	{
		auto& contextSelections = s_Contexts[context];
		auto it = std::find(contextSelections.begin(), contextSelections.end(), selectionID);
		if (it == contextSelections.end())
			return;

		contextSelections.erase(it);
	}

	void SelectionManager::DeselectAll()
	{
		for (auto& [ctxID, contextSelections] : s_Contexts)
		{
			// TODO: Dispatch selection changed event...? SelectionChangedEvent
			contextSelections.clear();
		}
	}

	void SelectionManager::DeselectAll(SelectionContext context)
	{
		auto& contextSelections = s_Contexts[context];

		// TODO: Dispatch selection changed event...? SelectionChangedEvent

		contextSelections.clear();
	}

	UUID SelectionManager::GetSelection(SelectionContext context, std::size_t index)
	{
		auto& contextSelections = s_Contexts[context];
		IR_VERIFY(index >= 0 && index < contextSelections.size());
		return contextSelections[index];
	}

	std::size_t SelectionManager::GetSelectionCount(SelectionContext context)
	{
		return s_Contexts[context].size();
	}

}