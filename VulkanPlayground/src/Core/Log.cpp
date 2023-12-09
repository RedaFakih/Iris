#include "vkPch.h"
#include "Log.h"

#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace vkPlayground::Logging {

	std::shared_ptr<spdlog::logger> Log::s_CoreLogger;

	void Log::Init()
	{
		std::vector<spdlog::sink_ptr> playgroundSinks =
		{
			std::make_shared<spdlog::sinks::basic_file_sink_mt>("../VulkanPlayground/LogDump/PlaygroundDump.log", true),
			std::make_shared<spdlog::sinks::stdout_color_sink_mt>()
		};

		playgroundSinks[0]->set_pattern("[%T] [%l] %n: %v");
		playgroundSinks[1]->set_pattern("%^[%T] %n(%l): %v%$");

		s_CoreLogger = std::make_shared<spdlog::logger>("Core", begin(playgroundSinks), end(playgroundSinks));
		spdlog::register_logger(s_CoreLogger);
		s_CoreLogger->set_level(spdlog::level::trace);
	}

	void Log::Shutdown()
	{
		s_CoreLogger.reset();
		spdlog::drop_all();
	}

}