#pragma once

#include "Scene/Entity.h"

namespace Iris {

	enum class SelectionContext
	{
		Scene = 0, ContentBrowser, Global, _COUNT
	};

	class SelectionManager
	{
	public:
		static void Select(SelectionContext context, UUID selectionID);
		static bool IsSelected(UUID selectionID);
		static bool IsSelected(SelectionContext context, UUID selectionID);
		static bool IsEntityOrAncestorSelected(const Entity entity);
		static bool IsEntityOrAncestorSelected(SelectionContext context, const Entity entity);
		static void Deselect(UUID selectionID);
		static void Deselect(SelectionContext context, UUID selectionID);
		static void DeselectAll();
		static void DeselectAll(SelectionContext context);
		static UUID GetSelection(SelectionContext context, std::size_t index);

		static std::size_t GetSelectionCount(SelectionContext context);
		inline static const std::vector<UUID>& GetSelections(SelectionContext context) { return s_Contexts[static_cast<uint32_t>(context)]; }

		inline static bool HasAnySelection()
		{
			for (const auto& selections: s_Contexts)
			{
				if (!selections.empty())
					return true;
			}

			return false;
		}

		inline static bool HasAnySelection(SelectionContext context)
		{
			return !s_Contexts[static_cast<uint32_t>(context)].empty();
		}

		inline static uint64_t GetSelectionVersion() { return s_SelectionVersion; }

	private:
		inline static std::array<std::vector<UUID>, 3> s_Contexts;
		inline static uint64_t s_SelectionVersion = 0;

	};

}