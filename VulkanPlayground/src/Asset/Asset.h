#pragma once

#include "Core/UUID.h"
#include "Core/Ref.h"

namespace vkPlayground {

	class Asset : public RefCountedObject
	{
	public:
		Asset() : Handle(UUID()) {}

		UUID Handle;
	};

}