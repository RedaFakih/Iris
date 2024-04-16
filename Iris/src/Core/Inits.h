#pragma once

#include "Core/Log.h"

namespace Iris::Initializers {

	void InitializeCore()
	{
		Logging::Log::Init();
	}

	void ShutdownCore()
	{
		Logging::Log::Shutdown();
	}

}