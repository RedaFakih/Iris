#pragma once

#include "Core/UUID.h"
#include "Core/Ref.h"

namespace Iris {

	class Asset : public RefCountedObject
	{
	public:
		Asset() : Handle(UUID()) {}

		UUID Handle = 0;
	};

}