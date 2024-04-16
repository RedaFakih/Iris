#pragma once

#include "Core/Log.h"

#include <assimp/DefaultLogger.hpp>
#include <assimp/LogStream.hpp>

namespace Iris {

	class AssimpLogStream : public Assimp::LogStream
	{
	public:
		static void Init()
		{
			if (Assimp::DefaultLogger::isNullLogger())
			{
				Assimp::DefaultLogger::create("", Assimp::Logger::VERBOSE);
				Assimp::DefaultLogger::get()->attachStream(new AssimpLogStream(), Assimp::Logger::Debugging | Assimp::Logger::Info | Assimp::Logger::Warn | Assimp::Logger::Err);
			}
		}

		virtual void write(const char* message) override
		{
			std::string msg(message);
			if (!msg.empty() && msg[msg.length() - 1] == '\n')
			{
				msg.erase(msg.length() - 1); // Remove new line character
			}

			if (strncmp(message, "Debug", 5) == 0)
			{
				IR_CORE_DEBUG_TAG("Assimp", msg);
			}
			else if (strncmp(message, "Info", 4) == 0)
			{
				IR_CORE_INFO_TAG("Assimp", msg);
			}
			else if (strncmp(message, "Warn", 4) == 0)
			{
				IR_CORE_WARN_TAG("Assimp", msg);
			}
			else
			{
				IR_CORE_ERROR_TAG("Assimp", msg);
			}
		}
	};

}