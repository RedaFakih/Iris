#pragma once

#include "LogFormatting.h"

#include <spdlog/spdlog.h>
#include <spdlog/fmt/ostr.h>

#include <memory>
#include <map>

namespace Iris::Logging {

	class Log
	{
	public:
		enum class Type : uint8_t
		{
			None = 0,
			Core,
			Client
		};

		enum class Level : uint8_t
		{
			None = 0,
			Trace,
			Info,
			Warn,
			Error,
			Fatal,
			Debug // Set as greatest so that it is always printed... Since debug is for debugging and should always be printed for debugging
		};

		struct TagDetails
		{
			bool Enabled = true;
			Level LevelFilter = Level::Trace; // Min level filter, meaning that if we set this to Warn then info and trace will NOT be logged only warn and higher levels
		};

	public:
		static void Init();
		static void Shutdown();

		static const std::shared_ptr<spdlog::logger>& GetCoreLogger() { return s_CoreLogger; }
		static const std::shared_ptr<spdlog::logger>& GetClientLogger() { return s_ClientLogger; }

		static bool HasTag(const std::string& tag) { return s_EnabledTags.contains(tag); }
		static std::map<std::string, TagDetails>& GetEnabledTags() { return s_EnabledTags; }


		template<typename... Args>
		static void PrintMessage(Log::Type type, Log::Level level, std::string_view tag, Args&&... args);

		template<typename... Args>
		static void PrintAssertMessageWithTag(Log::Type type, std::string_view tag, std::string_view assertFailed, Args&&... args);

	public:
		// Enum utils
		static const char* LevelToString(Level level)
		{
			switch (level)
			{
				case Level::Trace: return "Trace";
				case Level::Info:  return "Info";
				case Level::Debug: return "Debug";
				case Level::Warn:  return "Warn";
				case Level::Error: return "Error";
				case Level::Fatal: return "Fatal";
			}

			return "";
		}

		static Level LevelFromString(std::string_view string)
		{
			if (string == "Trace") return Level::Trace;
			if (string == "Info")  return Level::Info;
			if (string == "Debug") return Level::Debug;
			if (string == "Warn")  return Level::Warn;
			if (string == "Error") return Level::Error;
			if (string == "Fatal") return Level::Fatal;

			return Level::Trace;
		}

	private:
		static std::shared_ptr<spdlog::logger> s_CoreLogger;
		static std::shared_ptr<spdlog::logger> s_ClientLogger;

		// TODO: These are to be serialized when we have a projects so that we can properly filter out logging messages
		static std::map<std::string, TagDetails> s_EnabledTags;

	};

}

// TODO: Add #ifdef guards around these macros for dist build when they exist

#define IR_CORE_TRACE_TAG(tag, ...)  ::Iris::Logging::Log::PrintMessage(::Iris::Logging::Log::Type::Core, ::Iris::Logging::Log::Level::Trace, tag, __VA_ARGS__)
#define IR_CORE_INFO_TAG(tag, ...)   ::Iris::Logging::Log::PrintMessage(::Iris::Logging::Log::Type::Core, ::Iris::Logging::Log::Level::Info, tag, __VA_ARGS__)
#define IR_CORE_WARN_TAG(tag, ...)   ::Iris::Logging::Log::PrintMessage(::Iris::Logging::Log::Type::Core, ::Iris::Logging::Log::Level::Warn, tag, __VA_ARGS__)
#define IR_CORE_ERROR_TAG(tag, ...)  ::Iris::Logging::Log::PrintMessage(::Iris::Logging::Log::Type::Core, ::Iris::Logging::Log::Level::Error, tag, __VA_ARGS__)
#define IR_CORE_FATAL_TAG(tag, ...)  ::Iris::Logging::Log::PrintMessage(::Iris::Logging::Log::Type::Core, ::Iris::Logging::Log::Level::Fatal, tag, __VA_ARGS__)
#define IR_CORE_DEBUG_TAG(tag, ...)  ::Iris::Logging::Log::PrintMessage(::Iris::Logging::Log::Type::Core, ::Iris::Logging::Log::Level::Debug, tag, __VA_ARGS__)

#define IR_CORE_TRACE(...)           ::Iris::Logging::Log::PrintMessage(::Iris::Logging::Log::Type::Core, ::Iris::Logging::Log::Level::Trace, "", __VA_ARGS__)
#define IR_CORE_INFO(...)            ::Iris::Logging::Log::PrintMessage(::Iris::Logging::Log::Type::Core, ::Iris::Logging::Log::Level::Info, "", __VA_ARGS__)
#define IR_CORE_WARN(...)            ::Iris::Logging::Log::PrintMessage(::Iris::Logging::Log::Type::Core, ::Iris::Logging::Log::Level::Warn, "", __VA_ARGS__)
#define IR_CORE_ERROR(...)           ::Iris::Logging::Log::PrintMessage(::Iris::Logging::Log::Type::Core, ::Iris::Logging::Log::Level::Error, "", __VA_ARGS__)
#define IR_CORE_FATAL(...)           ::Iris::Logging::Log::PrintMessage(::Iris::Logging::Log::Type::Core, ::Iris::Logging::Log::Level::Fatal, "", __VA_ARGS__)
#define IR_CORE_DEBUG(...)           ::Iris::Logging::Log::PrintMessage(::Iris::Logging::Log::Type::Core, ::Iris::Logging::Log::Level::Debug, "", __VA_ARGS__)

// Client side logging
#define IR_TRACE_TAG(tag, ...)       ::Iris::Logging::Log::PrintMessage(::Iris::Logging::Log::Type::Client, ::Iris::Logging::Log::Level::Trace, tag, __VA_ARGS__)
#define IR_INFO_TAG(tag, ...)        ::Iris::Logging::Log::PrintMessage(::Iris::Logging::Log::Type::Client, ::Iris::Logging::Log::Level::Info, tag, __VA_ARGS__)
#define IR_WARN_TAG(tag, ...)        ::Iris::Logging::Log::PrintMessage(::Iris::Logging::Log::Type::Client, ::Iris::Logging::Log::Level::Warn, tag, __VA_ARGS__)
#define IR_ERROR_TAG(tag, ...)       ::Iris::Logging::Log::PrintMessage(::Iris::Logging::Log::Type::Client, ::Iris::Logging::Log::Level::Error, tag, __VA_ARGS__)
#define IR_FATAL_TAG(tag, ...)       ::Iris::Logging::Log::PrintMessage(::Iris::Logging::Log::Type::Client, ::Iris::Logging::Log::Level::Fatal, tag, __VA_ARGS__)
#define IR_DEBUG_TAG(tag, ...)       ::Iris::Logging::Log::PrintMessage(::Iris::Logging::Log::Type::Client, ::Iris::Logging::Log::Level::Debug, tag, __VA_ARGS__)

#define IR_TRACE(...)                ::Iris::Logging::Log::PrintMessage(::Iris::Logging::Log::Type::Client, ::Iris::Logging::Log::Level::Trace, "", __VA_ARGS__)
#define IR_INFO(...)                 ::Iris::Logging::Log::PrintMessage(::Iris::Logging::Log::Type::Client, ::Iris::Logging::Log::Level::Info, "", __VA_ARGS__)
#define IR_WARN(...)                 ::Iris::Logging::Log::PrintMessage(::Iris::Logging::Log::Type::Client, ::Iris::Logging::Log::Level::Warn, "", __VA_ARGS__)
#define IR_ERROR(...)                ::Iris::Logging::Log::PrintMessage(::Iris::Logging::Log::Type::Client, ::Iris::Logging::Log::Level::Error, "", __VA_ARGS__)
#define IR_FATAL(...)                ::Iris::Logging::Log::PrintMessage(::Iris::Logging::Log::Type::Client, ::Iris::Logging::Log::Level::Fatal, "", __VA_ARGS__)
#define IR_DEBUG(...)                ::Iris::Logging::Log::PrintMessage(::Iris::Logging::Log::Type::Client, ::Iris::Logging::Log::Level::Debug, "", __VA_ARGS__)

namespace Iris::Logging {

	template<typename... Args>
	inline void Log::PrintMessage(Log::Type type, Log::Level level, std::string_view tag, Args&&... args)
	{
		// Enable Filtering based on serialized level filter
		const TagDetails& detail = s_EnabledTags[std::string(tag)]; // Insert if it does not exist
		if (detail.Enabled && detail.LevelFilter <= level)
		{
			const std::shared_ptr<spdlog::logger>& logger = (type == Type::Core) ? GetCoreLogger() : GetClientLogger();
			std::string logString = tag.empty() ? "{0}{1}" : "[{0}]: {1}";
			switch (level)
			{
			case Level::Trace:
				logger->trace(logString, tag, fmt::format(std::forward<Args>(args)...));
				break;
			case Level::Info:
				logger->info(logString, tag, fmt::format(std::forward<Args>(args)...));
				break;
			case Level::Debug:
				logger->debug(logString, tag, fmt::format(std::forward<Args>(args)...));
				break;
			case Level::Warn:
				logger->warn(logString, tag, fmt::format(std::forward<Args>(args)...));
				break;
			case Level::Error:
				logger->error(logString, tag, fmt::format(std::forward<Args>(args)...));
				break;
			case Level::Fatal:
				logger->critical(logString, tag, fmt::format(std::forward<Args>(args)...));
				break;
			}
		}

	}

	template<typename... Args>
	inline void Log::PrintAssertMessageWithTag(Log::Type type, std::string_view tag, std::string_view assertFailed, Args&&... args)
	{
		const std::shared_ptr<spdlog::logger>& logger = (type == Type::Core) ? GetCoreLogger() : GetClientLogger();
		logger->critical("[{0}]: {1}\n\t\t\t  Error: {2}", tag, assertFailed, fmt::format(std::forward<Args>(args)...));
	}

	// Without this template specializtion, assert without messages are not possible since the function would still want more arguments for the variadic template!
	template<>
	inline void Log::PrintAssertMessageWithTag(Log::Type type, std::string_view tag, std::string_view assertFailed)
	{
		const std::shared_ptr<spdlog::logger>& logger = (type == Type::Core) ? GetCoreLogger() : GetClientLogger();
		logger->critical("[{0}]: {1}", tag, assertFailed);
	}

}