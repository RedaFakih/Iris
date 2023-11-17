#pragma once

#include "Log.h"
#include "Ref.h"

#include <source_location>
#include <memory>

#define PG_ASSERT(x, message)\
{\
	if(!(x))\
	{\
		std::source_location loc = std::source_location::current();\
		PG_CORE_CRITICAL_TAG("Core", "Assertion Failed in File: {0}, Function: {1}, Line: {2}!\n{3}", loc.file_name(), loc.function_name(), loc.line(), message);\
		__debugbreak();\
	}\
}

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