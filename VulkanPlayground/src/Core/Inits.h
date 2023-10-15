#pragma once

#include "Core/Log.h"

namespace vkPlayground::Initializers {

	void InitializeCore()
	{
		vkPlayground::Logging::Log::Init();
	}

	void ShutdownCore()
	{
		vkPlayground::Logging::Log::Shutdown();
	}

}