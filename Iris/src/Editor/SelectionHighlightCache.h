#pragma once

#include "Core/Ref.h"
#include "SelectionHighlightMask.h"
#include "SelectionManager.h"

namespace Iris {

    /*
     * Flat "this entity or one of its ancestors is selected" mask
     * Rebuilt only when selection / hierarchy / scene changed
     */

	class SelectionHighlightCache
	{
	public:
        void Prepare(WeakRef<Scene> scene);

        inline SelectionHighlightMask GetMask() const { return { m_Mask.data(), m_Mask.size(), m_AnyHighlighted }; }

        inline bool IsHighlighted(entt::entity e) const
        {
            const uint32_t index = Utils::EntityToIndex(e);
            return m_AnyHighlighted && index < m_Mask.size() && m_Mask[index] != 0;
        }

        bool HasSelectedDescendant(entt::entity entity, SelectionContext context) const;

        void Reset();

        inline bool AnyHighlighted() const { return m_AnyHighlighted; }
        inline bool AnyMeshHighlighted() const { return m_AnyMeshHighlighted; }

	private:
        void Mark(Entity e);
        void MarkHasSelectedDescendant(Entity e, uint8_t bit);
        void Rebuild(WeakRef<Scene> scene);

        static constexpr uint8_t ContextBit(SelectionContext context)
        {
            return uint8_t(1u) << static_cast<uint32_t>(context);
        }
        static_assert(static_cast<uint32_t>(SelectionContext::Global) < 8,
            "m_DescendantMask is a uint8_t bitfield over SelectionContext");

    private:
        bool m_AnyHighlighted = false;
        bool m_AnyDescendantSelected = false;
        bool m_AnyMeshHighlighted = false;

        std::vector<uint8_t> m_Mask;
        std::vector<uint8_t> m_DescendantMask;
        std::vector<Entity> m_Stack;
        
        WeakRef<Scene> m_Scene = nullptr;
        
        uint64_t m_SelectionVersion = 0;
        uint64_t m_HierarchyVersion = 0;
        uint64_t m_RenderableVersion = 0;

	};

}