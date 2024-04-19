#pragma once

#include "Scene/Entity.h"

namespace Iris {

	enum class SelectionContext
	{
		Global = 0, Scene
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
		inline static const std::vector<UUID>& GetSelections(SelectionContext context) { return s_Contexts[context]; }

	private:
		inline static std::unordered_map<SelectionContext, std::vector<UUID>> s_Contexts;

	};

}