#include "IrisPCH.h"
#include "SelectionHighlightCache.h"

#include "Scene/Scene.h"

namespace Iris {

    void SelectionHighlightCache::Prepare(WeakRef<Scene> scene)
    {
        if (scene != m_Scene ||
            SelectionManager::GetSelectionVersion() != m_SelectionVersion ||
            scene->GetHierarchyVersion() != m_HierarchyVersion ||
            scene->GetRenderableVersion() != m_RenderableVersion)
        {
            Rebuild(scene);
        }
    }

    bool SelectionHighlightCache::HasSelectedDescendant(entt::entity entity, SelectionContext context) const
    {
        if (!m_AnyDescendantSelected)
            return false;

        const uint32_t index = Utils::EntityToIndex(entity);
        return index < m_DescendantMask.size() &&
            (m_DescendantMask[index] & ContextBit(context)) != 0;
    }

    void SelectionHighlightCache::Reset()
    {
        m_Mask.clear();
        m_Mask.shrink_to_fit();
        m_Stack.clear();
        m_Scene = nullptr;
        m_AnyHighlighted = false;
        m_AnyDescendantSelected = false;
        m_AnyMeshHighlighted = false;
    }

    void SelectionHighlightCache::Mark(Entity e)
    {
        const uint32_t index = Utils::EntityToIndex(static_cast<entt::entity>(e));
        if (index >= m_Mask.size())
            m_Mask.resize(index + 1, 0);

        m_Mask[index] = 1;
        m_AnyHighlighted = true;
    }

    void SelectionHighlightCache::MarkHasSelectedDescendant(Entity e, uint8_t bit)
    {
        const uint32_t index = Utils::EntityToIndex(static_cast<entt::entity>(e));
        if (index >= m_DescendantMask.size())
            m_DescendantMask.resize(index + 1, 0);
        
        m_DescendantMask[index] |= bit;
        m_AnyDescendantSelected = true;
    }

    void SelectionHighlightCache::Rebuild(WeakRef<Scene> scene)
    {
        m_Scene = scene;
        m_SelectionVersion = SelectionManager::GetSelectionVersion();
        m_HierarchyVersion = scene->GetHierarchyVersion();
        m_RenderableVersion = scene->GetRenderableVersion();

        std::fill(m_Mask.begin(), m_Mask.end(), 0);
        m_AnyHighlighted = false;

        std::fill(m_DescendantMask.begin(), m_DescendantMask.end(), 0);
        m_AnyDescendantSelected = false;

        m_AnyMeshHighlighted = false;

        m_Stack.clear();

        for (uint32_t ctx = 0; ctx < static_cast<uint32_t>(SelectionContext::_COUNT); ++ctx)
        {
            const auto context = static_cast<SelectionContext>(ctx);
            const uint8_t bit = ContextBit(context);

            for (UUID id : SelectionManager::GetSelections(static_cast<SelectionContext>(ctx)))
            {
                Entity selected = scene->TryGetEntityWithUUID(id);
                if (!selected)
                    continue;

                m_Stack.push_back(selected);
                while (!m_Stack.empty())
                {
                    Entity e = m_Stack.back();
                    m_Stack.pop_back();

                    Mark(e);
                    if (!m_AnyMeshHighlighted && e.HasComponent<StaticMeshComponent, VisibleComponent>())
                        m_AnyMeshHighlighted = true;

                    for (UUID childID : e.Children())
                        if (Entity child = scene->TryGetEntityWithUUID(childID))
                            m_Stack.push_back(child);
                }

                for (Entity ancestor = selected.GetParent(); ancestor; ancestor = ancestor.GetParent())
                    MarkHasSelectedDescendant(ancestor, bit);
            }
        }
    }

}