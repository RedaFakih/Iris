#pragma once

#include <spdlog/spdlog.h>
#include <spdlog/fmt/ostr.h>

#include <memory>

namespace vkPlayground::Logging {

	class Log
	{
	public:
		enum class Level : uint8_t
		{
			Trace = 0, Info, Debug, Warn, Error, Fatal
		};

	public:
		static void Init();
		static void Shutdown();

		static const std::shared_ptr<spdlog::logger>& GetCoreLogger() { return s_CoreLogger; }

	template<typename... Args>
	inline static void PrintMessage(Log::Level level, std::string_view tag, Args&&... args)
	{
		const std::shared_ptr<spdlog::logger>& logger = GetCoreLogger();
		std::string logString = tag.empty() ? "{0}{1}" : "[{0}]: {1}";
		std::string formattedString = fmt::format(std::forward<Args>(args)...);

		switch (level)
		{
			case Level::Trace:
				logger->trace(logString, tag, formattedString);
				break;
			case Level::Info:
				logger->info(logString, tag, formattedString);
				break;
			case Level::Debug:
				logger->debug(logString, tag, formattedString);
				break;
			case Level::Warn:
				logger->warn(logString, tag, formattedString);
				break;
			case Level::Error:
				logger->error(logString, tag, formattedString);
				break;
			case Level::Fatal:
				logger->critical(logString, tag, formattedString);
				break;
		}
	}

	template<typename... Args>
	inline void PrintAssertMessageWithTag(std::string_view tag, std::string_view assertFailed, Args&&... args)
	{
		const auto& logger = GetCoreLogger();
		logger->error("[{0}]: {1}\n\t\t\t  Error: {2}", tag, assertFailed, fmt::format(std::forward<Args>(args)...));
	}

	// Without this template specializtion, assert without messages are not possible since the function would still want more arguments for the variadic template!
	template<>
	inline void PrintAssertMessageWithTag(std::string_view tag, std::string_view assertFailed)
	{
		const auto& logger = GetCoreLogger();
		logger->error("[{0}]: {1}", tag, assertFailed);
	}

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

	};

}

// TODO: Add #ifdef guards around these macros

#define PG_CORE_TRACE_TAG(tag, ...)     ::vkPlayground::Logging::Log::PrintMessage(::vkPlayground::Logging::Log::Level::Trace, tag, __VA_ARGS__)
#define PG_CORE_INFO_TAG(tag, ...)      ::vkPlayground::Logging::Log::PrintMessage(::vkPlayground::Logging::Log::Level::Info, tag, __VA_ARGS__)
#define PG_CORE_DEBUG_TAG(tag, ...)     ::vkPlayground::Logging::Log::PrintMessage(::vkPlayground::Logging::Log::Level::Debug, tag, __VA_ARGS__)
#define PG_CORE_WARN_TAG(tag, ...)      ::vkPlayground::Logging::Log::PrintMessage(::vkPlayground::Logging::Log::Level::Warn, tag, __VA_ARGS__)
#define PG_CORE_ERROR_TAG(tag, ...)     ::vkPlayground::Logging::Log::PrintMessage(::vkPlayground::Logging::Log::Level::Error, tag, __VA_ARGS__)
#define PG_CORE_CRITICAL_TAG(tag, ...)  ::vkPlayground::Logging::Log::PrintMessage(::vkPlayground::Logging::Log::Level::Fatal, tag, __VA_ARGS__)