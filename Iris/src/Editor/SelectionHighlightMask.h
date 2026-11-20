#pragma once

#include <entt/entt.hpp>

namespace Iris {

    namespace Utils {

        inline uint32_t EntityToIndex(entt::entity e)
        {
            return static_cast<std::uint32_t>(e) & entt::entt_traits<std::uint32_t>::entity_mask;
        }

    }

    struct SelectionHighlightMask
    {
        const uint8_t* Bits = nullptr;
        size_t Count = 0;
        bool AnyHighlighted = false;

        bool Test(entt::entity e) const
        {
            if (!AnyHighlighted)
                return false;

            const uint32_t index = Utils::EntityToIndex(e);
            return index < Count && Bits[index] != 0;
        }

    };

}