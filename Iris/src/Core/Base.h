#pragma once

#include "Log.h"
#include "Ref.h"

#include <memory>

#define IR_DEBUG_BREAK() __debugbreak()
#define IR_FORCE_INLINE __forceinline

#include "Assert.h"

// For creating enum bit fields
#define BIT(x) 1 << x

namespace Iris {

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