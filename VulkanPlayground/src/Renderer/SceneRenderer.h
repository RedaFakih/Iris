#pragma once

#include "Scene/Scene.h"

namespace vkPlayground {

	class SceneRenderer :public RefCountedObject
	{
	public:
		SceneRenderer(Ref<Scene> scene);
		~SceneRenderer();

		[[nodiscard]] static Ref<SceneRenderer> Create(Ref<Scene> scene);

	private:

	};

}