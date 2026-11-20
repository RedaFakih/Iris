#include "IrisPCH.h"
#include "SelectionManager.h"

namespace Iris {

	void SelectionManager::Select(SelectionContext context, UUID selectionID)
	{
		auto& selectedContext = s_Contexts[static_cast<uint32_t>(context)];
		if (std::find(selectedContext.begin(), selectedContext.end(), selectionID) != selectedContext.end())
			return;

		selectedContext.push_back(selectionID);
		++s_SelectionVersion;
	}

	bool SelectionManager::IsSelected(UUID selectionID)
	{
		for (const auto& contextSelections : s_Contexts)
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
		const auto& contextSelections = s_Contexts[static_cast<uint32_t>(context)];
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
		for (auto& contextSelections : s_Contexts)
		{
			if (contextSelections.empty())
				continue;

			auto it = std::find(contextSelections.begin(), contextSelections.end(), selectionID);
			if (it == contextSelections.end())
				continue;

			contextSelections.erase(it);
			++s_SelectionVersion;
			break;
		}
	}

	void SelectionManager::Deselect(SelectionContext context, UUID selectionID)
	{
		auto& contextSelections = s_Contexts[static_cast<uint32_t>(context)];

		if (contextSelections.empty())
			return;

		auto it = std::find(contextSelections.begin(), contextSelections.end(), selectionID);
		if (it == contextSelections.end())
			return;

		contextSelections.erase(it);
		++s_SelectionVersion;
	}

	void SelectionManager::DeselectAll()
	{
		for (auto& contextSelections : s_Contexts)
		{
			if (!contextSelections.empty())
			{
				contextSelections.clear();

				++s_SelectionVersion;
			}
		}
	}

	void SelectionManager::DeselectAll(SelectionContext context)
	{
		auto& contextSelections = s_Contexts[static_cast<uint32_t>(context)];

		if (!contextSelections.empty())
		{
			contextSelections.clear();
			++s_SelectionVersion;
		}
	}

	UUID SelectionManager::GetSelection(SelectionContext context, std::size_t index)
	{
		auto& contextSelections = s_Contexts[static_cast<uint32_t>(context)];
		IR_VERIFY(index < contextSelections.size());
		return contextSelections[index];
	}

	std::size_t SelectionManager::GetSelectionCount(SelectionContext context)
	{
		return s_Contexts[static_cast<uint32_t>(context)].size();
	}

}