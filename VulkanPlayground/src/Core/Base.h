#pragma once

#include <iostream>
#include <memory>
#include <source_location>

#define PG_ASSERT(x, message) { if(!(x)) { std::source_location loc = std::source_location::current(); std::cerr << "Assertion Failed in File: " << loc.file_name() << " Function: " << loc.function_name() << " Line: " << loc.line() << "!\n" << message << std::endl; __debugbreak(); } }

namespace vkPlayground {

	template<typename T>
	using Ref = std::shared_ptr<T>;

	template<typename T, typename... Args>
	constexpr Ref<T> CreateRef(Args&&... args)
	{
		return std::make_shared<T>(std::forward<Args>(args)...);
	}

	template<typename T>
	using Scope = std::unique_ptr<T>;

	template<typename T, typename... Args>
	constexpr Scope<T> CreateScope(Args&&... args)
	{
		return std::make_unique<T>(std::forward<Args>(args)...);
	}

}