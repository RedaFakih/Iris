#pragma once

#include <source_location>

#ifdef IR_CONFIG_DEBUG

	#define IR_CORE_ASSERT_INTERNAL(...) ::Iris::Logging::Log::PrintAssertMessageWithTag(::Iris::Logging::Log::Type::Core, "Core", "Assertion failed!", __VA_ARGS__)

	#define IR_ASSERT(check, ...)\
	{\
		if (!(check))\
		{\
			std::source_location loc = std::source_location::current();\
			IR_CORE_FATAL_TAG("Core", "Assertion Failed in File: {0}, Function: {1}, Line: {2}!", loc.file_name(), loc.function_name(), loc.line());\
			IR_CORE_ASSERT_INTERNAL(__VA_ARGS__);\
			IR_DEBUG_BREAK();\
		}\
	}

#else

	#define IR_ASSERT(check, ...)		  (void)0

#endif // IR_CONFIG_DEBUG

// Here we define verifies that are stronger asserts and they also work in release and dist builds...
// #ifdef IR_CONFIG_RELEASE

#define IR_CORE_VERIFY_INTERNAL(...) ::Iris::Logging::Log::PrintAssertMessageWithTag(::Iris::Logging::Log::Type::Core, "Core", "Verify failed!", __VA_ARGS__)

#define IR_VERIFY(check, ...)\
{\
	if (!(check))\
	{\
		std::source_location loc = std::source_location::current();\
		IR_CORE_FATAL_TAG("Core", "Verify Failed in File: {0}, Function: {1}, Line: {2}!", loc.file_name(), loc.function_name(), loc.line());\
		IR_CORE_VERIFY_INTERNAL(__VA_ARGS__);\
		IR_DEBUG_BREAK();\
	}\
}

// #endif // IR_CONFIG_RELEASE
