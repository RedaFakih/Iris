#pragma once

#include <source_location>

#ifdef VKPG_CONFIG_DEBUG

	#define VKPG_CORE_ASSERT_INTERNAL(...) ::vkPlayground::Logging::Log::PrintAssertMessageWithTag(::vkPlayground::Logging::Log::Type::Core, "Core", "Assertion failed!", __VA_ARGS__)

	#define VKPG_ASSERT(check, ...)\
	{\
		if (!(check))\
		{\
			std::source_location loc = std::source_location::current();\
			VKPG_CORE_FATAL_TAG("Core", "Assertion Failed in File: {0}, Function: {1}, Line: {2}!", loc.file_name(), loc.function_name(), loc.line());\
			VKPG_CORE_ASSERT_INTERNAL(__VA_ARGS__);\
			VKPG_DEBUG_BREAK();\
		}\
	}

#else

	#define VKPG_ASSERT(check, ...)		  (void)0

#endif // VKPG_CONFIG_DEBUG

// Here we define verifies that are stronger asserts and they also work in release and dist builds...
// #ifdef VKPG_CONFIG_RELEASE

#define VKPG_CORE_VERIFY_INTERNAL(...) ::vkPlayground::Logging::Log::PrintAssertMessageWithTag(::vkPlayground::Logging::Log::Type::Core, "Core", "Verify failed!", __VA_ARGS__)

#define VKPG_VERIFY(check, ...)\
{\
	if (!(check))\
	{\
		std::source_location loc = std::source_location::current();\
		VKPG_FATAL_TAG("Core", "Verify Failed in File: {0}, Function: {1}, Line: {2}!", loc.file_name(), loc.function_name(), loc.line());\
		VKPG_CORE_VERIFY_INTERNAL(__VA_ARGS__);\
		VKPG_DEBUG_BREAK();\
	}\
}

// #endif // VKPG_CONFIG_RELEASE
