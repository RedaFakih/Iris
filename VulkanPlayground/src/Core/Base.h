#pragma once

#include "Log.h"
#include "Ref.h"

#include <memory>

#define VKPG_DEBUG_BREAK() __debugbreak()

#include "Assert.h"

#define VKPG_SET_EVENT_FN(function) [this](auto&&... args) -> decltype(auto) { return this->function(std::forward<decltype(args)>(args)...); }

// For creating enum bit fields
#define BIT(x) 1 << x

namespace vkPlayground {

	template<typename T, typename... Args>
	constexpr Ref<T> CreateRef(Args&&... args)
	{
		return CreateReferencedObject<T>(std::forward<Args>(args)...);
	}

	template<typename T>
	using Scope = std::unique_ptr<T>;

	template<typename T, typename... Args>
	constexpr Scope<T> CreateScope(Args&&... args)
	{
		return std::make_unique<T>(std::forward<Args>(args)...);
	}

}